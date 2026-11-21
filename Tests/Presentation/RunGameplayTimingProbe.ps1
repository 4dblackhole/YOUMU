[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$FmodRoot = $env:FMOD_ROOT,
    [switch]$BuildOnly,
    # Passed to GameplayTimingProbe.exe, e.g. --pattern "all notes test" --replay --label buzz
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$ProbeArguments
)
# Timing measurement, not a pass/fail test: plays one chart through the real gameplay scene,
# FMOD output and a visible window, then prints the clock offset and frame/tick-sound summary.
# Run it alone: concurrent builds or engine checks distort the numbers it measures.
$ErrorActionPreference = 'Stop'
if (-not $FmodRoot) { throw 'Set FMOD_ROOT or pass -FmodRoot.' }
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MT', '/DNDEBUG') }
$units = @('Pch', 'GameplaySessionController', 'GameplaySessionFactory', 'GameplayInput', 'GameplayAudioSession', 'ModePresentation',
    'GameplayState', 'GameplayPause', 'GameplayPauseMenu', 'GameplayPauseLayout', 'PauseTexts', 'GameplayAudioRouter', 'TextCatalog', 'LanguageFile', 'SkinSetSelection', 'SkinManifest', 'SkinVisualComponent',
    'RhythmTestScene', 'UiErrorLine', 'GameplayScreenFade', 'GameplayOffsetNotice', 'GameplayLoadingSettings', 'GameDataPaths', 'GameplayPresenter', 'GameplayLayout', 'GameplayNoteView', 'GameplayNoteVisibility', 'GameplayFeedbackVisuals',
    'GameplayInputView', 'GameplayKiaiView', 'GameplayJudgementView', 'GameplayProgressView', 'GameplayHpView', 'GameplayStatisticsView', 'LaneKeyBeam', 'GameplayTexts')
$objects = $units | ForEach-Object { "build/obj/YOUMU.Client/x64/$Configuration/$_.obj" }
$libraries = @('Rhythm', 'Chart', 'Modes', 'Assets', 'ClientLogic') | ForEach-Object { "bin/x64/$Configuration/YOUMU.$_.lib" }
$objectDirectory = "build/obj/GameplayTimingProbe/$Configuration"
New-Item -ItemType Directory -Path $objectDirectory -Force | Out-Null
& cl /nologo /utf-8 /std:c++20 /EHsc /W4 /analyze /O2 @runtime /DNOMINMAX /IClient /IYOUMU.Rhythm /IYOUMU.Chart /IYOUMU.Modes /IYOUMU.Assets/Public /IDependencies/PhantomCore/Engine/SDK Tests/Presentation/GameplayTimingProbe.cpp @objects @libraries "Dependencies/PhantomCore/bin/x64/$Configuration/PhantomCore.lib" "/Fo$objectDirectory/" "/Febin/x64/$Configuration/GameplayTimingProbe.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$FmodRoot/api/core/lib/x64" "build/obj/YOUMU.Assets/x64/$Configuration/YOUMU.Assets.res" user32.lib gdi32.lib ole32.lib mfplat.lib mfreadwrite.lib mfuuid.lib
if ($LASTEXITCODE -ne 0) { throw 'Timing probe build failed.' }
if ($BuildOnly) { return }
& "./bin/x64/$Configuration/GameplayTimingProbe.exe" @ProbeArguments
if ($LASTEXITCODE -ne 0) { throw 'Timing probe run failed.' }
