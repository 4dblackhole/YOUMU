# 변경 영향 지도

[기능 문서](README.md)에서 계약을 고른 뒤 아래 소유자와 직접 호출부·관련 테스트만
확인합니다. 경로의 `Submodules/`는 해당 Scene 또는 기능의 구현 폴더입니다.
모든 코드 변경의 공통 빌드·분석·smoke 절차는 [Verification](Verification.md)을 따릅니다.

| 작업 | 소유 코드 | 관련 회귀 |
| --- | --- | --- |
| 곡 카드·검색·정렬·이동·배경/메타데이터 | `MusicSelectScene/Submodules`: MusicSelectView, SongSelectLayout/Cards/Panels/Input, SongSelectionState | SceneStateTests, RunMusicSelectViewTests |
| 곡 목록·디스크 캐시·비동기 패턴 로딩 | SongCatalogLoader, `YOUMU.Chart/Catalog`, `GameFlow/PlaySessionLoader` | 카탈로그 로직, MusicSelectViewTests, GameplayPauseTests |
| 미리듣기 | SongPreviewController | MusicSelectViewTests, 엔진 오디오 계약 |
| 새 난이도·.osu 가져오기 | NewDifficultyDialog, EditorPatternCreation, EditorOsuImport, OsuImportValidation | RunNewDifficultyTests, RunOsuImportValidationTests |
| 전역 옵션·Taiko 키·Option.ini | `Client/Presentation/OptionsPanel`, `Client/App/OptionSettings` | RunOptionsPanelTests, OptionTimingTests |
| 표시 이름·번역·글꼴·공통 색상 | `Client/Texts`, GameIdentity.props, LanguageFile, `Presentation/UiColors.h`, MarqueeTextComponent | RunLanguageFileTests, 해당 화면 표시 회귀 |
| 스킨·자산·배포 | SkinManifest, SkinSetSelection, AssetPaths, SkinVisualComponent, `YOUMU.Assets` | RunSkinTests, RunAssetPathTests |
| 시작·Scene 수명·launch snapshot | `Client/App`, `Client/GameFlow`, 각 Scene 주 파일 | SceneStateTests, ResultPresentationTests |
| 플레이 시계·Debug 정지·입력 순서 | `GameFlow/Submodules`: GameplaySessionController, GameplayInput, GameplayState; `Rhythm/Time` | RhythmCoreTests, InputOrderingTests, RunGameplayPauseTests |
| ESC·재개·되감기·Retire | GameplayPause, GameplayPauseMenu/Layout, PlaySessionSeeking | PlayPauseTests, GameplayPauseTests, ResultPresentationTests |
| Lane·노트/롱노트·마디선·키빔·진행바 | `RhythmTestScene/Submodules`: GameplayPresenter/Layout/NoteVisuals/Keys/ProgressView; LaneKeyBeam | RunGameplayJudgementTests, RunAllNotesGameplayTests |
| 판정 인디케이터·링·문구 | GameplayJudgementView, GameplayFeedbackVisuals | GameplayJudgementTests |
| 정확도·POOR·Buzz·물리 키 유지 | `YOUMU.Rhythm/Note`, Judgement, Lane; `Modes/Mode/Submodules/PlaySession` | AccuracyTests, InputOrderingTests, BuzzReplayTests, PlayPauseTests |
| Taiko 노트 종류·사운드 정책 | `YOUMU.Modes/Taiko/Submodules`: TaikoNoteDefinition, SessionBuilder, LongNoteFactory, SoundPolicy, NoteOptions | TaikoModeTests, SoundPolicyTests, EditorModeTests |
| 판정 난이도·JudgeLevel | `Rhythm/Judgement`, PatternDocument/Parser, TaikoSessionBuilder | RhythmCoreTests, ChartParserTests, NewDifficultyTests |
| 플레이 옵션·배속·Random·메트로놈 | PlayOptions, TaikoNoteTransform/SessionBuilder, RhythmTimer, GameplayAudioRouter, PlayOptionsDialog | PlayOptionsTests, MusicSelectViewTests, GameplayPauseTests |
| 영속 기록·조건식·결과 옵션 표시 | `Client/Records`, SongSelectRecords, `Taiko/Results`, ResultScene | MusicSelectViewTests, RunResultPresentationTests |
| 음악/히트사운드·YME 오디오 연결 | `GameFlow/Submodules/GameplayAudio`, `Client/Audio/GameplayAudioRouter`, TaikoChartAudio/AudioPreview | SoundPolicyTests, GameplayPauseTests, AllNotesGameplayTests |
| Result·통계·그래프·Retry | `Modes/Mode/Results`, `Taiko/Results`, `Client/GameScene/ResultScene` | PlayResultTests, ResultPresentationTests |
| 에디터 조립·모드/문서 교체 | EditorScene, EditorWorkspace, Modes/IEditorMode/IEditorContext/IEditorAudioSource, `Chart/Editing/IEditorDocument` | RunEditorPresentationTests의 EditorModeTests |
| 에디터 메뉴·공통 이동·선택·붙여넣기 | EditorMenuBar/Input/View, EditorSelection/Clipboard, TaikoEditorSelection | EditorMenuTests, EditorModeTests, ChartEditorTests |
| 노트/타이밍/효과 편집·Undo/Redo·저장 | TaikoEditorNoteInspector/Score/Metadata, EditorForms/TimingView/EffectsView, ChartEditor/History | ChartEditorTests, ChartReviewRegressionTests, EditorModeTests |
| 파형·FFT·탐색·배속·offset | EditorAnalysisController, EditorAudioView/Selection/Controls, EditorPlayback/Metronome, `YOUMU.Editor/Audio` | EditorAudioAnalysisTests, EditorAudioAnalysisIntegrationTests, EditorAudioViewTests, EditorMetronomeTests |
| YMM/YMP/YME 문법·효과·타이밍 | `Chart/Parsing/Submodules`, Model, MusicalTimeline, EffectValidation, InterpolationExpression/ScrollAutomation | ChartParserTests, YmeEffectsTests, 저장 왕복·캐시 무효화 |
| 엔진 API·GPU/폰트·장치 수명 | 독립 PhantomCore 저장소의 [EngineOverview](../Dependencies/PhantomCore/Docs/EngineOverview.md) | 엔진 AGENTS/SDK 회귀 |

## 소유권 경계

- Scene은 Controller/Presenter를 소유하고 전환을 요청합니다. UI는 전환 의도만 반환합니다.
  Controller는 세션·시계·오디오를, Presenter는 Canvas/노드·표시 인덱스를 소유합니다.
- 노트·편집 원본은 안정 ID와 소유 트리를 유지합니다. 화면/구간/효과 인덱스는 읽기 전용
  projection이며 revision에 따라 갱신합니다. 실패한 수정은 원본과 이력을 보존합니다.
- 저장 노트 ID는 파서의 int/string, 모드 의미는 Taiko 정의, 표시는 NoteVisualKind,
  기본 cue는 TaikoSoundIds가 담당합니다. 노트 규칙에 엔진/UI 의존성을 추가하지 않습니다.
- Scene과 기능의 `.h/.cpp`를 함께 두고 물리 경로와 `.vcxproj.filters`를 맞춥니다.
  로직 테스트는 `Tests/Rhythm/Submodules`, Client 표시 검사는 `Tests/Presentation`에 둡니다.
  ClientLogic의 공통 상태를 재사용하며 테스트에서 Client 구현 `.cpp`를 include하지 않습니다.
