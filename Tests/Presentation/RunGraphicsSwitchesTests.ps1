param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
# Option.ini [Diagnostics] graphics switches: builds GraphicsSwitchesTests twice, with YOUMU_GRAPHICS_SWITCHES=1 (the
# default build) and =0 (a build made with /p:YoumuGraphicsSwitches=false), and checks that only the first passes the
# switches to the engine. Then checks that the Client project turns the MSBuild property into that definition.
# Option.ini copies are written under build/, never next to the game.
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$testRoot = Join-Path $repoRoot ('build/graphics-switches-tests/' + $Configuration + '-' + [guid]::NewGuid().ToString('N'))
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MT', '/DNDEBUG') }
$units = @('Pch', 'OptionSettings', 'TextCatalog', 'LanguageFile')
$clientObjects = $units | ForEach-Object { "build/obj/YOUMU.Client/x64/$Configuration/$_.obj" }
$libraries = @('Modes', 'Chart', 'Rhythm', 'ClientLogic', 'Assets') | ForEach-Object { "bin/x64/$Configuration/YOUMU.$_.lib" }

Push-Location $repoRoot
try {
    foreach ($variant in @(@{ Name = 'allowed'; Value = 1 }, @{ Name = 'blocked'; Value = 0 })) {
        $objects = Join-Path $testRoot ('obj-' + $variant.Name)
        New-Item -ItemType Directory -Path $objects -Force | Out-Null
        $exe = Join-Path $testRoot ('GraphicsSwitchesTests-' + $variant.Name + '.exe')
        # GraphicsSwitches.cpp is compiled here from source, so the definition under test is the one it sees.
        & cl /nologo /W4 /analyze /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /DWIN32_LEAN_AND_MEAN /DUNICODE /D_UNICODE `
            "/DYOUMU_GRAPHICS_SWITCHES=$($variant.Value)" /IClient /IYOUMU.Modes /IYOUMU.Chart /IYOUMU.Rhythm /IYOUMU.Assets/Public `
            /IDependencies/PhantomCore/Engine/SDK Tests/Presentation/GraphicsSwitchesTests.cpp Client/Diagnostics/GraphicsSwitches.cpp `
            @clientObjects @libraries "Dependencies/PhantomCore/bin/x64/$Configuration/PhantomCore.lib" `
            "/Fo$objects/" "/Fe$exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$env:FMOD_ROOT/api/core/lib/x64" `
            "build/obj/YOUMU.Assets/x64/$Configuration/YOUMU.Assets.res" user32.lib gdi32.lib ole32.lib
        if ($LASTEXITCODE -ne 0) { throw "Graphics switches test compilation failed ($($variant.Name))." }
        & $exe $variant.Name (Join-Path $testRoot ('ini-' + $variant.Name))
        if ($LASTEXITCODE -ne 0) { throw "Graphics switches tests failed ($($variant.Name))." }
    }

    # The project maps the property to the definition: true (the default) to 1, false to 0.
    $project = Get-Content -Raw Client/YOUMU.Client.vcxproj
    if (-not $project.Contains('YOUMU_GRAPHICS_SWITCHES=$(YoumuGraphicsSwitchesValue)')) { throw 'The Client project does not pass YOUMU_GRAPHICS_SWITCHES to the compiler.' }
    foreach ($case in @(@{ Property = @(); Expected = '1' }, @{ Property = @('-p:YoumuGraphicsSwitches=false'); Expected = '0' })) {
        $value = "$(& msbuild Client/YOUMU.Client.vcxproj -nologo -getProperty:YoumuGraphicsSwitchesValue "-p:Configuration=$Configuration" -p:Platform=x64 @($case.Property))".Trim()
        if ($LASTEXITCODE -ne 0 -or $value -ne $case.Expected) { throw "YoumuGraphicsSwitches $($case.Property) gave '$value', expected $($case.Expected)." }
    }
    Write-Output 'PASS graphics switches: allowed and blocked builds, YoumuGraphicsSwitches=false maps to YOUMU_GRAPHICS_SWITCHES=0.'
    Remove-Item -LiteralPath $testRoot -Recurse -Force
} finally { Pop-Location }
