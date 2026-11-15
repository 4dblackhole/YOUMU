param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MD', '/DNDEBUG') }
$units = @('Pch', 'SkinSetSelection', 'SkinManifest', 'SkinVisualComponent', 'LogoView', 'LogoLayout', 'LogoInput', 'LogoTexts', 'TitlePlayerView',
    'OptionsPanel', 'OptionsRail', 'OptionsVolume', 'OptionsDisplay', 'OptionsKeys', 'ModePresentation', 'OptionsOffset', 'OptionsSongs', 'OptionSettings',
    'OptionTexts', 'TextCatalog', 'LanguageFile')
$objects = $units | ForEach-Object { "build/obj/YOUMU.Client/x64/$Configuration/$_.obj" }
$libraries = @('Assets', 'Modes', 'Chart', 'Rhythm', 'ClientLogic') | ForEach-Object { "bin/x64/$Configuration/YOUMU.$_.lib" }
$testObjects = "build/obj/TitlePlayerTests/$Configuration"
New-Item -ItemType Directory -Path $testObjects -Force | Out-Null
& cl /nologo /analyze /W4 /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IYOUMU.Assets/Public /IYOUMU.Modes /IYOUMU.Chart /IYOUMU.Rhythm `
    /IDependencies/PhantomCore/Engine/SDK Tests/Presentation/TitlePlayerTests.cpp @objects @libraries "Dependencies/PhantomCore/bin/x64/$Configuration/PhantomCore.lib" `
    "/Fo$testObjects/" "/Febin/x64/$Configuration/TitlePlayerTests.exe" /link /OPT:NOICF /OPT:NOREF `
    "/LIBPATH:$env:FMOD_ROOT/api/core/lib/x64" "build/obj/YOUMU.Assets/x64/$Configuration/YOUMU.Assets.res" user32.lib gdi32.lib ole32.lib comdlg32.lib
if ($LASTEXITCODE -ne 0) { throw 'Title player test compilation failed.' }
& "./bin/x64/$Configuration/TitlePlayerTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Title player tests failed.' }
