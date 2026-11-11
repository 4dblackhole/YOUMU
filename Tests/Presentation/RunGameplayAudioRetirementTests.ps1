param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$FmodRoot = $env:FMOD_ROOT
)
$ErrorActionPreference = 'Stop'
if (!$FmodRoot) { throw 'Set FMOD_ROOT or pass -FmodRoot.' }
$runtime = if ($Configuration -eq 'Debug') { @('/MDd','/D_DEBUG') } else { @('/MD','/DNDEBUG') }
$objects = @('Pch','GameplayAudioRouter') | ForEach-Object { "build/obj/YOUMU.Client/x64/$Configuration/$_.obj" }
$libraries = @('Modes','Rhythm','Chart') | ForEach-Object { "bin/x64/$Configuration/YOUMU.$_.lib" }
$testObjects = "build/obj/GameplayAudioRetirementTests/$Configuration"
New-Item -ItemType Directory -Path $testObjects -Force | Out-Null
& cl /nologo /utf-8 /std:c++20 /EHsc /W4 /analyze @runtime /DNOMINMAX /IClient /IYOUMU.Modes /IYOUMU.Rhythm /IYOUMU.Chart /IDependencies/PhantomCore/Engine/SDK Tests/Presentation/GameplayAudioRetirementTests.cpp @objects @libraries "Dependencies/PhantomCore/bin/x64/$Configuration/PhantomCore.lib" "/Fo$testObjects/" "/Febin/x64/$Configuration/GameplayAudioRetirementTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$FmodRoot/api/core/lib/x64" user32.lib gdi32.lib ole32.lib mfplat.lib mfreadwrite.lib mfuuid.lib
if ($LASTEXITCODE -ne 0) { throw 'Gameplay audio retirement test build failed.' }
& "./bin/x64/$Configuration/GameplayAudioRetirementTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Gameplay audio retirement test failed.' }
