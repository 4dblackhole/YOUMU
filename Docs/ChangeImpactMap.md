# 변경 영향 지도

[설계 기준](Design.md)과 [관련 결정 ID](DesignDecisions.md), [기능 문서](README.md)에서
유지할 계약을 확인한 뒤 아래 소유자와 직접 호출부·관련 테스트만
확인합니다. 경로의 `Submodules/`는 해당 Scene 또는 기능의 구현 폴더입니다.
모든 코드 변경의 공통 빌드·분석·smoke 절차는 [Verification](Verification.md)을 따릅니다.

| 작업 | 소유 코드 | 관련 회귀 |
| --- | --- | --- |
| 곡 카드·검색·정렬·이동·배경/메타데이터 | `MusicSelectScene/Submodules`: MusicSelectView, SongListView(행·카드 가상화·스크롤·난이도 버튼), SongSelectLayout/Panels/Input/Search, SongSelectionState | SceneStateTests, RunMusicSelectViewTests(SongSearchInputTests 포함) |
| 선곡 필터·계산 난이도 빈 필드 | SongFilter, SongFilterPanel, SongSelectionState의 패턴 투영; SongCatalogPattern 런타임 메타데이터 | TestSongFilters, RunMusicSelectViewTests(필터 적용/취소·실 native 입력 포함) |
| 패턴별 즐겨찾기·폴더 탭·JSON 저장 | SongFavorites/Json, SongFavoriteTabs, SongFavoritesPanel, SongSelectFavorites; Scene 진입 로드와 SongSelectionState의 패턴 투영 | SongFavoritesTests, RunMusicSelectViewTests(관리·복수 소속·가로 스크롤·실 이름 입력·모달 격리) |
| 선곡 화면 틀·대표 이름·기록 줄·하단 띠 | SongSelectLayout(열·하단 띠 좌표, 레터박스 배율, 글자 도우미), SongSelectPanels(면·곡 정보·하단 띠 생성과 갱신), SongRecordView, SongFavoriteTabs, `Presentation/RankName.h`(결과 화면과 공유) | RunMusicSelectViewTests, RunSkinTests, ResultPresentationTests |
| 선택 채보 BG의 선곡·로딩·플레이·리절트 배경 | SongBackgroundPreview, RhythmTestScene의 초기 BG 준비, YOUMUGame의 Result context, TaikoResultView; Presentation/BackgroundLayout | MusicSelectViewTests, GameplayPauseTests(초기화 중 BG 재사용·준비 순서), ResultPresentationTests, GameplayJudgementTests |
| 곡 목록·디스크 캐시·비동기 패턴 로딩 | ChartLoadJob, SongCatalogLoader, `YOUMU.Chart/Catalog`, PlaySessionLoader, GameplaySessionFactory | ChartLoadingTests(취소·worker 수명·부분 목록 선택), 카탈로그 로직, MusicSelectViewTests, GameplayPauseTests |
| 미리듣기·공유 곡 목록 | `GameFlow/SongLibrary`(목록 로더·선택 상태·미리듣기 묶음, 타이틀·게임 곡 선택 공유), SongPreviewController, MusicPlayerState(재생 상태·재생 목록 순환) | SceneStateTests(TestMusicPlayerTransport·TestMusicPlaylistSteps), RunEditorPresentationTests(실제 미리듣기 재생·이동·곡 끝), MusicSelectViewTests, 엔진 오디오 계약 |
| 타이틀 음악 플레이어 | `YOUMULogoScene/Submodules/TitlePlayerView`, LogoView/LogoInput(키·Space/Enter 분리), YOUMULogoScene(요청 실행·화면 전환 때 소리 유지/정지) | RunTitlePlayerTests, RunSkinTests, 화면 캡처 |
| 새 난이도·.osu 가져오기 | NewDifficultyDialog, EditorPatternCreation, EditorOsuImport, OsuImportValidation | RunNewDifficultyTests, RunOsuImportValidationTests |
| 전역 옵션·분류 막대·모드별 키 편집·Option.ini | `Client/Presentation/OptionsPanel`의 OptionsRail(분류 막대·섹션 배치·스크롤)/OptionsVolume/OptionsKeys(키 카드·키 설정 창)/각 필드·OptionsPanelStyle, `Client/App/OptionSettings`, 스킨 `[Options]` | RunOptionsPanelTests(섹션 이동·추적·막대 펼침·안내 칸·음량 저장·ja 저장·표현 등록부 정합), MusicSelectViewTests(키 그림 열 머리), OptionTimingTests |
| 공용 드롭다운 | `Client/Presentation/UiDropdown`; 쓰는 곳 OptionsPanel·PlayOptionsPanel·SongFilterPanel·RecordFilterPanel·SongRecordView·SongSelectPanels, 키 가로채기 MusicSelectView·LogoView | RunOptionsPanelTests(목록 행 클릭·바깥 누르기), RunMusicSelectViewTests(정렬 선택, SongSearchInputTests의 Esc 순서), 화면 캡처 |
| 공용 툴팁 | `Client/Presentation/UiTooltip`; 소유자 MusicSelectView(하단 띠·이름 줄, SongListView 카드 줄 힌트)·LogoView(플레이어 버튼)·OptionsPanel(분류 막대·닫기)·TaikoResultView(Back·점수·옵션 칩·제작자), `[Tooltips]` 문구 | RunOptionsPanelTests(TestTooltip·TestTooltipLines), RunMusicSelectViewTests(카드 이름 힌트), RunResultPresentationTests(결과 툴팁), 화면 캡처 |
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
| 게임플레이 오디오 등록·자원 준비·메트로놈 | `GameFlow/Submodules/GameplayAudioSession`(라우터·등록·준비 polling·메트로놈 커서) | GameplayPauseTests, AllNotesGameplayTests, RunEditorTestSceneTests |
| ESC·재개·되감기·Retire | GameplayPause, GameplayPauseMenu/Layout, PlaySessionSeeking | PlayPauseTests, GameplayPauseTests, ResultPresentationTests |
| Lane·노트/롱노트·마디선·키빔·진행바 | `RhythmTestScene/Submodules`: GameplayPresenter/Layout, GameplayNoteView(노트·tick·마디선·focus 연출), GameplayNoteVisibility, GameplayInputView, GameplayProgressView; LaneKeyBeam | RunGameplayJudgementTests, RunAllNotesGameplayTests |
| 판정 인디케이터·링·문구 | GameplayJudgementView, GameplayFeedbackVisuals | GameplayJudgementTests |
| 정확도·POOR·Buzz·물리 키 유지 | `YOUMU.Rhythm/Note`, Judgement, Lane; `Modes/Mode/Submodules/PlaySession` | AccuracyTests, InputOrderingTests, BuzzReplayTests, PlayPauseTests |
| Taiko 노트 종류·사운드 정책 | `YOUMU.Modes/Taiko/Submodules`: TaikoNoteDefinition, SessionBuilder, LongNoteFactory, SoundPolicy, NoteOptions | TaikoModeTests, SoundPolicyTests, EditorModeTests |
| 판정 난이도·JudgeLevel | `Rhythm/Judgement`, PatternDocument/Parser, TaikoSessionBuilder | RhythmCoreTests, ChartParserTests, NewDifficultyTests |
| HP 체력·게이지(틀·채움·클리어 표시·자물쇠)·빈 채보 진입 | `Rhythm/Gauge`, PlaySession/Results, TaikoSessionBuilder, GameplayHpView, PlaySessionLoader | HpGaugeTests, SongCatalogTests, GameplayJudgementTests, GameplayPauseTests |
| 플레이 옵션·배속·Random·메트로놈 | PlayOptions, TaikoNoteTransform/SessionBuilder, RhythmTimer, GameplayAudioRouter, PlayOptionsPanel/PlayOptionsLayout | PlayOptionsTests, RunMusicSelectViewTests(PlayOptionsPanelTests 포함), GameplayPauseTests |
| 영속 기록·조건식·기록 필터·결과 옵션 표시 | `Client/Records`, RecordFilterPanel, SongRecordView(조회·페이지·필터·내부 노드 소유), SongSelectRecords(화면 조정), `Taiko/Results`, ResultScene | RecordFilterTests, MusicSelectViewTests/SongSearchInputTests, RunResultPresentationTests |
| 음악/히트사운드·YME 오디오 연결 | `GameFlow/Submodules/GameplayAudioSession`, `Client/Presentation/ModePresentation`, `Client/Audio/GameplayAudioRouter`, TaikoChartAudio/AudioPreview | SoundPolicyTests, GameplayPauseTests, AllNotesGameplayTests |
| 정확도 표시 형식(둘째 자리 버림)·빈 값 | `Client/Presentation/AccuracyText.h`; 소비자 GameplayPresenter, TaikoResultView, SongRecordView | RunResultPresentationTests(TestAccuracyText·랭크 경계), RunGameplayJudgementTests |
| Result·통계·그래프·판정 비율 원·Retry | `Modes/Mode/Results`, `Taiko/Results`, `Client/GameScene/ResultScene`(TaikoResultView 공통 틀, Panels 배너·왼쪽 열, Hero 원·점수·옵션 칩, GraphView/Graphs Timing·HP, Ring 조각 계산, Layout 좌표) | PlayResultTests, ResultPresentationTests |
| 리디자인 색·글꼴 | `Presentation/UiColors.h`의 theme, `Presentation/UiFonts.h`, `YOUMU.Assets/Assets/Fonts` | ResultPresentationTests, 화면 캡처 |
| 에디터 조립·모드/문서 교체 | EditorScene, EditorWorkspace, Modes/IEditorMode/IEditorContext/IEditorAudioSource, `Chart/Editing/IEditorDocument` | RunEditorPresentationTests의 EditorModeTests |
| 에디터 메뉴·공통 이동·선택·붙여넣기 | EditorMenuBar/Input/View, EditorSelection/Clipboard, TaikoEditorSelection | EditorMenuTests, EditorModeTests, ChartEditorTests |
| 저장한 채보 Test·복귀·부분 노트·가변 배속 | EditorScene/WorkspaceStore, TestLaunchRequest, RhythmTestScene/EditorTestControls, GameplayState/GameplayAudioSession, TaikoSessionBuilder의 minimumNoteTime, RhythmTimer | RunEditorTestSceneTests, EditorAudioViewTests의 저장 확인, TestPartialPreviewNotes, TestAcceleratedClockAndWindows |
| 노트/타이밍/효과 편집·Undo/Redo·저장 | TaikoEditorNoteInspector/Score/Metadata, EditorForms/TimingView/EffectsView, ChartEditor/History | ChartEditorTests, ChartReviewRegressionTests, EditorModeTests |
| 파형·FFT·탐색·배속·offset | EditorAnalysisController, EditorAudioView/Selection/Controls, EditorPlayback/Metronome, `YOUMU.Editor/Audio` | EditorAudioAnalysisTests, EditorAudioAnalysisIntegrationTests, EditorAudioViewTests, EditorMetronomeTests |
| YMM/YMP/YME 문법·효과·타이밍 | `Chart/Parsing/Submodules`, Model, MusicalTimeline, EffectValidation, InterpolationExpression/ScrollAutomation | ChartParserTests, YmeEffectsTests, 저장 왕복·캐시 무효화 |
| 엔진 API·GPU/폰트·장치 수명 | 독립 PhantomCore 저장소의 [EngineOverview](../Dependencies/PhantomCore/Docs/EngineOverview.md) | 엔진 AGENTS/SDK 회귀 |

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
