param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MT', '/DNDEBUG') }
$testObjects = "build/obj/OptionsPanelTests/$Configuration"
New-Item -ItemType Directory -Path $testObjects -Force | Out-Null
$objects = @('Pch', 'OptionSettings', 'OptionsPanel', 'OptionsRail', 'OptionsCategory', 'OptionsPercentSlider', 'OptionsGeneral', 'OptionsDisplayPosition', 'OptionsAudio', 'OptionsInput', 'UiTooltip', 'UiDropdown', 'UiAlert', 'UiScrollbar', 'OptionsDisplay', 'ModePresentation', 'OptionTexts', 'TextCatalog', 'LanguageFile', 'SkinSetSelection', 'SkinManifest', 'SkinVisualComponent') |
    ForEach-Object { "build/obj/YOUMU.Client/x64/$Configuration/$_.obj" }
& cl /nologo /analyze /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /IClient /IYOUMU.Assets/Public /IYOUMU.Modes /IYOUMU.Rhythm /IYOUMU.Chart `
    /IDependencies/PhantomCore/Engine/SDK Tests/Presentation/OptionsPanelTests.cpp @objects `
    "bin/x64/$Configuration/YOUMU.Assets.lib" "bin/x64/$Configuration/YOUMU.Modes.lib" "bin/x64/$Configuration/YOUMU.Chart.lib" "bin/x64/$Configuration/YOUMU.Rhythm.lib" "Dependencies/PhantomCore/bin/x64/$Configuration/PhantomCore.lib" `
    "/Fo$testObjects/" "/Febin/x64/$Configuration/OptionsPanelTests.exe" /link /OPT:NOICF /OPT:NOREF `
    "/LIBPATH:$env:FMOD_ROOT/api/core/lib/x64" "build/obj/YOUMU.Assets/x64/$Configuration/YOUMU.Assets.res" user32.lib gdi32.lib ole32.lib
if ($LASTEXITCODE -ne 0) { throw 'Options test compilation failed.' }
& "./bin/x64/$Configuration/OptionsPanelTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Options tests failed.' }
