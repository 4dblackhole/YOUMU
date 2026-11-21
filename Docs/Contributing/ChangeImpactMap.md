# 변경 영향 지도

[설계 기준](../Design/Principles.md)과 [관련 결정 ID](../Design/Decisions.md), [기능 문서](../README.md)에서
유지할 계약을 확인한 뒤 아래 소유자와 직접 호출부·관련 테스트만
확인합니다. 경로의 `Submodules/`는 해당 Scene 또는 기능의 구현 폴더입니다.
모든 코드 변경의 공통 빌드·분석·smoke 절차는 [Verification](Verification.md)을 따릅니다.

| 작업 | 소유 코드 | 관련 회귀 |
| --- | --- | --- |
| 곡 카드·검색·정렬·이동·배경/메타데이터 | `MusicSelectScene/Submodules`: MusicSelectView, SongListView(행·카드 가상화·스크롤·난이도 버튼), SongSelectLayout/Panels/Input/Search, SongSelectionState | SceneStateTests, RunMusicSelectViewTests(SongSearchInputTests 포함) |
| 선곡 필터(줄 스위치·HP·Length·Tag)·계산 난이도 빈 필드 | SongFilter, SongFilterPanel, SongSelectionState의 패턴 투영; SongCatalogPattern 런타임 메타데이터, 채보 길이는 SongCatalog/SongCatalogIndex(캐시 스키마) | TestSongFilters·TestSongFilterRows, TestSongCatalogChartLength, RunMusicSelectViewTests(필터 적용/취소·스위치·실 native 입력 포함) |
| 컬렉션(즐겨찾기)·탭·컬렉션 파일·Import/Export·favorites.json 옮기기 | SongFavorites/Json, SongFavoriteTabs, SongFavoritesPanel, SongSelectFavorites; Scene 진입 로드와 SongSelectionState의 패턴 투영; 채보 해시는 `Catalog/ChartHash`와 SongCatalog/SongCatalogIndex(캐시 스키마), 저장 위치는 SongLibrary(`UserAssetRoot()/Collections`) | SongFavoritesTests(저장·옮기기·선택), TestSongCatalogIndex(해시), RunMusicSelectViewTests(관리·복수 소속·가로 스크롤·실 이름 입력·모달 격리·Import/Export) |
| 선곡 화면 틀·대표 이름·기록 줄·하단 띠 | SongSelectLayout(열·하단 띠 좌표, 레터박스 배율, 글자 도우미), SongSelectPanels(면·곡 정보·하단 띠 생성과 갱신), SongRecordView(기록 줄 툴팁, 판정 수는 `Records/RecordJudgements`), SongFavoriteTabs, `Presentation/RankName.h`(결과 화면과 공유) | RunMusicSelectViewTests, RunSkinTests, ResultPresentationTests |
| 선택 채보 BG의 선곡·로딩·플레이·리절트 배경 | SongBackgroundPreview, RhythmTestScene의 초기 BG 준비, YOUMUGame의 Result context, TaikoResultView; Presentation/BackgroundLayout | MusicSelectViewTests, GameplayPauseTests(초기화 중 BG 재사용·준비 순서), ResultPresentationTests, GameplayJudgementTests |
| 곡 목록·곡 폴더 배치·디스크 캐시·비동기 패턴 로딩 | ChartLoadJob, SongCatalogLoader, `YOUMU.Chart/Catalog`(SongCatalog, SongLayout: 곡 폴더·YMM 찾기·BG 경로·폴더 이름), PlaySessionLoader, GameplaySessionFactory | ChartLoadingTests(취소·worker 수명·부분 목록 선택), 카탈로그 로직(TestSongLayout·TestSongCatalogIsolatesInvalidFiles), MusicSelectViewTests, GameplayPauseTests |
| 미리듣기·공유 곡 목록 | `GameFlow/SongLibrary`(목록 로더·선택 상태·미리듣기 묶음, 타이틀·게임 곡 선택 공유), SongPreviewController, MusicPlayerState(재생 상태·재생 목록 순환) | SceneStateTests(TestMusicPlayerTransport·TestMusicPlaylistSteps), RunEditorPresentationTests(실제 미리듣기 재생·이동·곡 끝), MusicSelectViewTests, 엔진 오디오 계약 |
| 타이틀 음악 플레이어 | `YOUMULogoScene/Submodules/TitlePlayerView`, LogoView/LogoInput(키·Space/Enter 분리), YOUMULogoScene(요청 실행·화면 전환 때 소리 유지/정지) | RunTitlePlayerTests, RunSkinTests, 화면 캡처 |
| 곡·채보 표기(로마자 줄, 첫 표기/로마자 표시) | `YOUMU.Chart/Model/Submodules/NotationText`(규칙), MusicParser·PatternParser·ChartEditor::WritePattern(로마자 줄), SongSelectionText(표시·검색·툴팁), SongSelectionState(정렬), OptionSettings·OptionsDisplay(`Song text`), TaikoEditorMetadata(입력 칸), 변환기 `FillMetadata`·`MusicText` | ChartParserTests(TestRomanizedNotation), SceneStateTests(TestSongSelectionNotation), RunMusicSelectViewTests(옵션 전환), RunOptionsPanelTests(저장), RunOsuImportValidationTests(폴더 이름), 변환기 단위 테스트 |
| 새 난이도·.osu 가져오기 | MusicSelectScene::CreateDifficulty, `EditorScene/Submodules/EditorPatternCreation`, IEditorMode::NewDocument/CreateDocument, EditorWorkspace(Creating·Save), EditorOsuImport(폴더 훑기·배치 작업), OsuImportValidation(변환 결과 검사), OsuSongPlacement(이름·복사·잠금·게시), OsuImportPanel(난이도 목록) | RunNewDifficultyTests, EditorModeTests(TestNewDifficultyDraft), RunOsuImportValidationTests(검사·배치), RunMusicSelectViewTests(실제 변환기·난이도 목록) |
| 전역 옵션·분류 막대·모드별 키 편집·Option.ini | `Client/Presentation/OptionsPanel`의 OptionsRail(분류 막대·섹션 배치·스크롤)/OptionsVolume/OptionsKeys(키 카드·키 설정 창)/각 필드·OptionsPanelStyle, `Client/App/OptionSettings`, 스킨 `[Options]` | RunOptionsPanelTests(섹션 이동·추적·막대 펼침·안내 칸·음량 저장·ja 저장·표현 등록부 정합), MusicSelectViewTests(키 그림 열 머리), OptionTimingTests |
| 공용 경고 대화상자 | `Client/Presentation/UiAlert`, `UiTextWrap`; 쓰는 곳 ResultScene(기록 저장 실패)·MusicSelectView(`ShowAlert`, 열린 동안 입력 차단)·MusicSelectScene(osu! 가져오기 실패) | RunOptionsPanelTests(배치·키·클릭·막·진단 스크롤), RunResultPresentationTests(저장 실패·다시 시도), RunMusicSelectViewTests(열린 동안 키 차단), RunLanguageFileTests |
| 공용 드롭다운 | `Client/Presentation/UiDropdown`; 쓰는 곳 OptionsPanel·PlayOptionsPanel·SongFilterPanel·RecordFilterPanel·SongRecordView·SongSelectPanels, 키 가로채기 MusicSelectView·LogoView | RunOptionsPanelTests(목록 행 클릭·바깥 누르기), RunMusicSelectViewTests(정렬 선택, SongSearchInputTests의 Esc 순서), 화면 캡처 |
| 공용 평평한 모달(Play options·곡 필터·기록 필터) | `Client/Presentation/UiModal`(막·창·제목·Reset/Cancel/주 버튼·오류 줄, 밑줄 입력칸·스위치·밑줄 탭·네이티브 입력 배치) | RunMusicSelectViewTests(PlayOptionsPanelTests·SongSearchInputTests), RunSkinTests, 화면 캡처 |
| 공용 툴팁 | `Client/Presentation/UiTooltip`; 소유자 MusicSelectView(하단 띠·이름 줄, SongListView 카드 줄 힌트)·LogoView(플레이어 버튼)·OptionsPanel(분류 막대·닫기)·TaikoResultView(Back·점수·모드·옵션 타일·제작자), `[Tooltips]` 문구 | RunOptionsPanelTests(TestTooltip·TestTooltipLines), RunMusicSelectViewTests(카드 이름 힌트), RunResultPresentationTests(결과 툴팁), 화면 캡처 |
| 공용 스크롤바 | `Client/Presentation/UiScrollbar`; 쓰는 곳 SongListView·SongFavoriteTabs(가로)·OptionsPanel/OptionsRail·UiDropdown, 끌기 연결 SongSelectInput | RunOptionsPanelTests(TestScrollbar), RunMusicSelectViewTests(곡 목록 끌기), RunTitlePlayerTests·RunSkinTests(오브젝트 목록), 화면 캡처 |
| F7 성능 띠 | `Client/Presentation/PerformanceBand`, YOUMUGame(F7·측정 전달), `[Performance]` 문구 | RunMusicSelectViewTests(TestPerformanceBand), smoke(띠가 켜진 채 렌더), 화면 캡처 |
| 곡 선택 재생 중 표시 | SongListView(카드 표시·애니메이션), MusicSelectView::SetNowPlaying, MusicSelectScene(선택 곡 = 재생 곡 판단) | RunMusicSelectViewTests(재생 중 표시), 화면 캡처 |
| 플레이어 음량 | `App/AudioVolumes`, OptionSettings, GameplayLaunchRequest(진입 당시 복사), GameplayAudioRouter/GameplayAudioSession(bus 배율·자동화 곱), SongLibrary/SongPreviewController(미리듣기·타이틀 플레이어), EditorPlayback/EditorWorkspace | RunOptionsPanelTests(저장·범위), RunGameplayAudioRetirementTests(bus 배율), RunEditorPresentationTests(EditorPlaybackTests 음량), 수동 청음 |
| 모드 등록·키 정책·레인 입력 | `YOUMU.Modes/Mode`: PlayGameModeFactory, ModeInputLayout, `Submodules/PlaySession`의 행동→레인 라우팅 | ModeRegistryTests, LaneRoutingTests, OptionTimingTests |
| 모드별 Client 표현(효과음 파일·키 오버레이·바인딩 문구) | `Client/Presentation/ModePresentation`; 소비자 GameplayAudioSession, OptionsKeys/OptionsRail, OptionTexts, TaikoEditorAudio | RunOptionsPanelTests의 표현 등록부 검사, GameplayPauseTests, RunEditorPresentationTests |
| 영어 UI/3언어 설명·글꼴·공통 색상 | `Client/Texts`의 TextCatalog/ExplanationKeys/언어 INI, GameIdentity.props, LanguageFile, `Presentation/UiColors.h`, MarqueeTextComponent | RunLanguageFileTests, 해당 화면 표시 회귀 |
| 스킨·자산·배포 | SkinManifest, SkinSetSelection, AssetPaths, SkinVisualComponent, `YOUMU.Assets` | RunSkinTests, RunAssetPathTests |
| 시작·Scene 수명·launch snapshot | `Client/App`, `Client/GameFlow`, 각 Scene 주 파일 | SceneStateTests, ResultPresentationTests |
| 플레이 시계·Debug 정지·입력 순서 | `GameFlow/Submodules`: GameplaySessionController, GameplayInput, GameplayState; `Rhythm/Time` | RhythmCoreTests, InputOrderingTests, RunGameplayPauseTests |
| 게임플레이 오디오 등록·자원 준비·메트로놈 | `GameFlow/Submodules/GameplayAudioSession`(라우터·등록·준비 polling·메트로놈 커서) | GameplayPauseTests, AllNotesGameplayTests |
| ESC·재개·되감기·Retire | GameplayPause, GameplayPauseMenu/Layout, PlaySessionSeeking | PlayPauseTests, GameplayPauseTests, ResultPresentationTests |
| Lane·노트/롱노트·마디선·키빔·진행바 | `RhythmTestScene/Submodules`: GameplayPresenter/Layout, GameplayNoteView(노트·tick·마디선·focus 연출), ScreenMarkThinning(겹치는 tick 솎기, ClientLogic), GameplayNoteVisibility, GameplayInputView, GameplayProgressView; LaneKeyBeam | RunGameplayJudgementTests, RunAllNotesGameplayTests, ScreenMarkThinningTests |
| Highlight 판정원·풍선/뎅뎅 그림·남은 타격 배지·기어 띠 | `RhythmTestScene/Submodules`: GameplayHighlightView, GameplayNoteView(헤드 세우기·처리 그림·배지), GameplayLayout; 스킨 `Gear.Band`·`Highlight.*`·`Hud.HitCountBadge`; 에디터 TaikoEditorScore의 풍선·뎅뎅 비율 | RunGameplayJudgementTests(TestHighlightJudgementCircle·TestCountOnlyHeadsAndCounters), RunSkinTests, RunEditorPresentationTests |
| 화면 오류 줄·로딩 준비 실패 흐름 | `Client/Presentation/UiErrorLine`, RhythmTestScene(PresentScreenError·PresentLoadingScreen), GameplaySessionController(ErrorSource·AttemptStarted·PreparationFailed), PlaySession(AutomationErrorLocation), GameplayPauseMenu(SetPreparationFailed) | RunGameplayJudgementTests(TestScreenErrorLine), RunGameplayPauseTests, RunLanguageFileTests |
| 판정 인디케이터·판정 글자 | GameplayJudgementView, GameplayFeedbackVisuals | GameplayJudgementTests |
| 정확도·POOR·Buzz·물리 키 유지 | `YOUMU.Rhythm/Note`, Judgement, Lane; `Modes/Mode/Submodules/PlaySession` | AccuracyTests, InputOrderingTests, BuzzReplayTests, PlayPauseTests |
| Taiko 노트 종류·사운드 정책 | `YOUMU.Modes/Taiko/Submodules`: TaikoNoteDefinition, SessionBuilder, LongNoteFactory, SoundPolicy, NoteOptions | TaikoModeTests, SoundPolicyTests, EditorModeTests |
| 판정 난이도·JudgeLevel | `Rhythm/Judgement`, PatternDocument/Parser, TaikoSessionBuilder | RhythmCoreTests, ChartParserTests, NewDifficultyTests |
| HP 체력·게이지(틀·채움·클리어 표시·자물쇠)·게이지 선택·빈 채보 진입 | `Rhythm/Gauge`(HpGaugeKind 목록과 클리어 기준), RhythmTestScene의 게이지 생성, PlaySession/Results, TaikoSessionBuilder, GameplayHpView, PlaySessionLoader | HpGaugeTests, SongCatalogTests, GameplayJudgementTests, GameplayPauseTests |
| 플레이 옵션·배속·Random·메트로놈·HP gauge·선곡 배지 | PlayOptions, TaikoNoteTransform/SessionBuilder, RhythmTimer, GameplayAudioRouter, PlayOptionsPanel/PlayOptionsLayout, SongSelectPanels/Layout(배지), TaikoResultHero(모드·옵션 타일) | PlayOptionsTests, RunMusicSelectViewTests(PlayOptionsPanelTests 포함), GameplayPauseTests, RunResultPresentationTests |
| 영속 기록·기록 필터(줄 스위치·결과 상태)·내부 조건 텍스트·결과 옵션 표시 | `Client/Records`, RecordFilterPanel, SongRecordView(조회·페이지·필터·내부 노드 소유), SongSelectRecords(화면 조정), `Taiko/Results`, ResultScene | RecordFilterTests, MusicSelectViewTests/SongSearchInputTests, RunResultPresentationTests |
| 음악/히트사운드·YME 오디오 연결 | `GameFlow/Submodules/GameplayAudioSession`, `Client/Presentation/ModePresentation`, `Client/Audio/GameplayAudioRouter`·`SampleRestartGate`(ClientLogic), TaikoChartAudio/AudioPreview | SoundPolicyTests, SampleRestartGateTests, GameplayPauseTests, AllNotesGameplayTests |
| 정확도 표시 형식(둘째 자리 버림)·빈 값 | `Client/Presentation/AccuracyText.h`; 소비자 GameplayPresenter, TaikoResultView, SongRecordView | RunResultPresentationTests(TestAccuracyText·랭크 경계), RunGameplayJudgementTests |
| Result·통계·그래프·판정 비율 원·Retry | `Modes/Mode/Results`, `Taiko/Results`, `Client/GameScene/ResultScene`(TaikoResultView 공통 틀, Panels 배너·왼쪽 열, Hero 원·점수·모드·옵션 타일, GraphView/Graphs Timing·HP, Ring 조각 계산, Layout 좌표) | PlayResultTests, ResultPresentationTests |
| 리디자인 색·글꼴 | `Presentation/UiColors.h`의 theme, `Presentation/UiFonts.h`, `YOUMU.Assets/Assets/Fonts` | ResultPresentationTests, 화면 캡처 |
| 에디터 조립·모드/문서 교체 | EditorScene, EditorWorkspace, Modes/IEditorMode/IEditorContext/IEditorAudioSource, `Chart/Editing/IEditorDocument` | RunEditorPresentationTests의 EditorModeTests |
| 에디터 화면 배치·Compose 채널·편집 패널 | EditorLayout/EditorTimeAxis, EditorView/ShellView/ComposeView/ChannelView/PanelView, EditorCurvesDialog(`Chart/Automation/InterpolationExpression`의 위치 오류), EditorTextFields, Taiko Score/Preview/Metadata | EditorModeTests, EditorAudioViewTests |
| 에디터 확인·실패 대화상자 | EditorAlerts(`Presentation/UiAlert`), EditorInput의 Back/Esc | EditorAudioViewTests의 미저장 대화상자 |
| 에디터 메뉴·공통 이동·선택·붙여넣기 | EditorMenuBar/Input/View/Alerts, EditorSelection/Clipboard, TaikoEditorSelection | EditorMenuTests, EditorModeTests, ChartEditorTests |
| 노트/타이밍/효과 편집·Undo/Redo·저장 | TaikoEditorNoteInspector/Score/Metadata, EditorForms/PanelView, ChartEditor/History | ChartEditorTests, ChartReviewRegressionTests, EditorModeTests |
| 파형·FFT·탐색·배속·offset | EditorAnalysisController, EditorSpectrumCache, EditorAudioView/Selection/Controls, EditorPlayback/Metronome, `YOUMU.Editor/Audio` | EditorAudioAnalysisTests, EditorAudioAnalysisIntegrationTests, EditorAudioViewTests, EditorMetronomeTests |
| YMM/YMP/YME 문법·효과·타이밍 | `Chart/Parsing/Submodules`, Model, MusicalTimeline, EffectValidation, InterpolationExpression/ScrollAutomation | ChartParserTests, YmeEffectsTests, 저장 왕복·캐시 무효화 |
| 엔진 API·GPU/폰트·장치 수명 | 독립 PhantomCore 저장소의 [EngineOverview](../../Dependencies/PhantomCore/Docs/EngineOverview.md) | 엔진 AGENTS/SDK 회귀 |

## 소유권 경계

- Scene은 Controller/Presenter를 소유하고 전환을 요청합니다. UI는 전환 의도만 반환합니다.
  Controller는 세션·시계·전송 상태를 소유하고 오디오는 GameplayAudioSession에 맡깁니다.
  Presenter는 Canvas와 배치를 소유하고 노트 표시는 GameplayNoteView에 맡깁니다.
- 노트·편집 원본은 안정 ID와 소유 트리를 유지합니다. 화면/구간/효과 인덱스는 읽기 전용
  projection이며 revision에 따라 갱신합니다. 실패한 수정은 원본과 이력을 보존합니다.
- 저장 노트 ID는 파서의 int/string, 모드 의미는 Taiko 정의, 표시는 NoteVisualKind,
  기본 cue는 TaikoSoundIds가 담당합니다. 노트 규칙에 엔진/UI 의존성을 추가하지 않습니다.
- Scene과 기능의 `.h/.cpp`를 함께 두고 물리 경로와 `.vcxproj.filters`를 맞춥니다.
  로직 테스트는 `Tests/Rhythm/Submodules`, Client 표시 검사는 `Tests/Presentation`에 둡니다.
  ClientLogic의 공통 상태를 재사용하며 테스트에서 Client 구현 `.cpp`를 include하지 않습니다.

## 구조 그림

아래 클래스·흐름을 바꾸면 해당 그림과 근거 줄도 같은 PR에서 고칩니다. 그림은 Mermaid 코드라서 문서 안에서 바로 고칠 수 있습니다.

| 그림 | 문서 | 근거 코드 |
| --- | --- | --- |
| 저장소와 프로젝트 의존 · 층 | [Overview](../Architecture/Overview.md#저장소와-프로젝트-의존) | 각 `.vcxproj`의 `ProjectReference`, `Scripts/CheckArchitecture.ps1` |
| 화면(Scene) 클래스와 수명 | [Overview](../Architecture/Overview.md#화면scene) | `Client/App/YOUMUGame.*`, `Client/GameFlow/YOUMUSceneIds.h`, 각 Scene 헤더 |
| 플레이 클래스 · 노트 규칙 · 화면 부품 | [Gameplay](../Architecture/Gameplay.md#클래스) | RhythmTestScene, GameplaySessionController, GameplayAudioSession, GameplayPresenter, PlaySession, ScrollGear/Lane/RuleBasedNote, TaikoSessionBuilder·TaikoLongNoteFactory |
| 플레이 상태 전이 | [Gameplay](../Architecture/Gameplay.md#상태-전이) | `PlayState`, GameplaySessionController, GameplayInput(재시작 키·ESC), GameplayPause |
| 프로그램 실행 순서도 | [ExecutionFlow](../Architecture/ExecutionFlow.md) | `Client/App/Main.cpp`, YOUMUGame, 각 Scene의 `ChangeScene` |
| 플레이 한 프레임 | [ExecutionFlow](../Architecture/ExecutionFlow.md#플레이-한-프레임) | `RhythmTestScene::Update/Render`, GameplaySessionController, GameplayInput |
| 작업 흐름 | [Workflow](Workflow.md) | 작업 규칙(코드 없음) |
| 엔진 소유 구조 | 엔진 [EngineOverview](../../Dependencies/PhantomCore/Docs/EngineOverview.md) | 엔진 `Engine/Core/App/Engine.cpp`, `SceneGameClient.h` |
