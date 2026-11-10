param([ValidateSet('Debug','Release')][string]$Configuration='Debug', [string]$FmodRoot=$env:FMOD_ROOT)
$ErrorActionPreference='Stop'
if (-not $FmodRoot) { throw 'Set FMOD_ROOT or pass -FmodRoot.' }
$runtime=if ($Configuration -eq 'Debug') { @('/MDd','/D_DEBUG') } else { @('/MD','/DNDEBUG') }
[xml]$project=Get-Content Client/YOUMU.Client.vcxproj
$units=@('Pch','GameplaySessionController','GameplaySessionFactory','GameplayInput','GameplayAudio','GameplayState','GameplayPause',
    'GameplayPauseMenu','GameplayPauseLayout','PauseTexts','GameplayAudioRouter','TextCatalog','LanguageFile','SkinSetSelection',
    'SkinManifest','SkinVisualComponent','RhythmTestScene','EditorTestControls','GameplayPresenter','GameplayLayout','GameplayNoteVisuals', 'GameplayNoteVisibility',
    'GameplayFeedbackVisuals','GameplayKeys','GameplayJudgementView','GameplayProgressView','GameplayHpView','GameplayStatisticsView','LaneKeyBeam','GameplayTexts')
$units+=@($project.Project.ItemGroup.ClCompile | Where-Object { $_.Include -match '^(EditorScene\\|Texts\\EditorScene\\)' } |
    ForEach-Object { [IO.Path]::GetFileNameWithoutExtension($_.Include) })
$objects=$units | ForEach-Object { "build/obj/YOUMU.Client/x64/$Configuration/$_.obj" }
$libraries=@('Rhythm','Chart','Modes','Editor','Assets') | ForEach-Object { "bin/x64/$Configuration/YOUMU.$_.lib" }
$directory="build/obj/EditorTestSceneTests/$Configuration"
New-Item -ItemType Directory -Force $directory | Out-Null
& cl /nologo /utf-8 /std:c++20 /EHsc /W4 /analyze @runtime /DNOMINMAX /IClient /IYOUMU.Rhythm /IYOUMU.Chart /IYOUMU.Modes /IYOUMU.Editor /IYOUMU.Assets/Public /IDependencies/PhantomCore/Engine/SDK Tests/Presentation/EditorTestSceneTests.cpp @objects @libraries "Dependencies/PhantomCore/bin/x64/$Configuration/PhantomCore.lib" "/Fo$directory/" "/Febin/x64/$Configuration/EditorTestSceneTests.exe" /link /OPT:NOICF /OPT:NOREF "/LIBPATH:$FmodRoot/api/core/lib/x64" "build/obj/YOUMU.Assets/x64/$Configuration/YOUMU.Assets.res" user32.lib gdi32.lib ole32.lib shell32.lib mfplat.lib mfreadwrite.lib mfuuid.lib
if ($LASTEXITCODE -ne 0) { throw 'Editor Test scene build failed.' }
& "./bin/x64/$Configuration/EditorTestSceneTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Editor Test scene checks failed.' }
