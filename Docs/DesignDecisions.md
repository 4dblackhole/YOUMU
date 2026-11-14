# 설계 결정

[설계 기준](Design.md)의 판단 순서·상태·변경 규칙을 따릅니다. ID는 문서를 정리하거나
코드를 재배치해도 보존합니다. 아래의 구현 여부와 미정점을 하나의 상태로 합치지 않습니다.
수치·예시는 별도 표기가 없으면 현재 계약의 회귀 사례이며 새 기능 제안이 아닙니다.
검증 항목은 확인할 기존 검사 위치입니다. 문서 작성만으로 그 검사를 새로 실행했다는 뜻은 아닙니다.

## ID-01 제품과 이름

근거: 사용자 확정. 구현: 구현.

- 엔진은 PhantomCore, Client/프로젝트 식별자는 YOUMU입니다.
- 표시 이름과 EXE 이름의 원본은 `Client/Texts/GameIdentity.props`의 GameDisplayName입니다.
  현재 표시는 `Y's Orchestral Unison: Melodic Ultimatum`, EXE는 콜론을 치환한
  `Y's Orchestral Unison - Melodic Ultimatum.exe`입니다.
- YOUMU 제품/도메인 타입은 전역 이름을 사용합니다. 엔진 공개 이름은 `PhantomCore`에 둡니다.
  Client 헤더는 `PhantomCore::`를 명시하고 구현 파일은 include 뒤에서만
  `using namespace PhantomCore;`를 사용할 수 있습니다(사용자 D9 확정).
  내부 링크·충돌 방지 UI 상수·외부 라이브러리의 이름 범위는 허용합니다.
  이전 이름을 유지하는 호환 별칭/전달 헤더를 복원하지 않습니다.

상세: [TextManagement](TextManagement.md#게임-이름-변경). 검증: 빌드·네 smoke와 CheckArchitecture.

## ARC-01 구조와 소유권

근거: 프로젝트 계약. 구현: 구현.

- 저장소/도메인 경계는 [Design](Design.md#시스템-경계)을 따릅니다. 엔진 변경은 독립 저장소,
  Client는 단일 공개 SDK를 사용합니다. 노트·포맷·분석에 UI/엔진 역의존성을 넣지 않습니다.
- Scene은 수명/조립/전환, Controller는 세션/시계/오디오, Presenter는 Canvas/노드를 소유합니다.
  Controller의 오디오 등록·준비·메트로놈은 GameplayAudioSession, Presenter의 노트 표시는
  GameplayNoteView처럼 자기 상태를 가진 객체에 맡깁니다.
  gameplay Scene은 DestroyOnExit이며, 진입은 값 snapshot, 종료는 결과 데이터만 전달합니다.
- 노트는 INote/INoteRule/INoteSoundPolicy의 계약과 의미 이벤트로 확장합니다.
  새 모드는 IPlayGameMode, 에디터 모드는 IEditorMode와 별도 IEditorDocument를 사용합니다.
  모드 생성·지원 여부는 PlayGameModeFactory의 등록 표, 키 정책은 ModeInputLayout, Client의
  효과음 파일·키 편집 형태·바인딩 문구는 ModePresentation 데이터로 등록합니다. 공통 코드는
  이 등록 지점과 에디터·결과 화면 factory 밖에서 모드 이름으로 분기하지 않습니다.
  절차는 [AddingGameMode](AddingGameMode.md)입니다.
- 기능별 파일/물리 폴더/필터를 맞추며 큰 클래스의 함수만 여러 cpp로 옮기는 것을 책임 분리로
  간주하지 않습니다. 기존 소유권을 바꾸는 리팩터링은 별도의 설계 변경입니다.

상세: [ExecutionFlow](ExecutionFlow.md), [ChangeImpactMap](ChangeImpactMap.md).
검증: CheckArchitecture, SceneStateTests와 관련 Scene 회귀.

## TIME-01 시간과 입력

근거: 프로젝트 계약. 구현: 구현.

- 활성 세션당 RhythmTimer 하나로 QPC ↔ 차트 정수 us ↔ DSP 시각을 연결합니다.
  입력 판정은 Raw Input timestamp와 순서를 사용하고 렌더 시각/프레임 delta로 대체하지 않습니다.
- 입력 시각 이전 틱은 입력 직전 물리 상태로, 입력과 같은 시각의 틱은 전이 후 상태로 처리합니다.
  최종 프레임 snapshot을 과거 입력 상태로 그대로 사용하지 않습니다.
- 같은 action에 여러 물리 키가 있으면 마지막 키를 놓을 때 유지가 해제됩니다.
  수락되지 않은 mapped 입력도 물리 상태와 입력 기록에는 반영합니다.
- 모드의 각 행동은 한 레인으로 전달되며, 여러 행동은 한 레인을 공유할 수 있습니다.
  세션 준비에서 레인 번호를 검증합니다. 라이브 입력과 되감기는 레인 선택을 공유하고
  각 경로의 기존 물리 상태 반영 순서를 따릅니다. [모드 입력 계약](AddingGameMode.md#세션과-입력)
- 음악/미래 cue는 DSP로 예약하고, 입력 히트사운드는 판정 후 즉시 재생합니다.
  Pause/seek 후에는 새 DSP 기준을 잡습니다. BPM/박자/효과 시각은 로딩 시 컴파일합니다.

상세: [Rules](Gameplay/Rules.md#한-개의-타이머), [Audio](Gameplay/Audio.md).
검증: InputOrderingTests, PlayPauseTests, GameplayPauseTests와 전체 노트 replay.

## FMT-01 파일 형식과 편집 원본

근거: 프로젝트 계약. 구현: 구현.

- YMM은 음악 정보, YMP는 플레이 노트/타이밍, YME는 이펙트/히트사운드 표를 소유합니다.
  YMP의 Effect file과 각 상대경로 기준을 유지하며 효과 명령을 YMP에 섞지 않습니다.
- 원본 마디/박자는 정확한 유리수와 안정 ID로 보존하고 실행용 시간/구간/스크롤은 파생 데이터입니다.
  음수 delay로 무효화된 구간도 원본을 삭제하거나 끝점을 잘린 경계로 옮기지 않습니다.
- 파서·직접 DTO·편집 저장은 같은 의미 검증을 사용합니다. 실패한 수정/저장은 원본과 이력을 보존합니다.
- 원본 Version, 목록 parser revision, 캐시 schema, 기록 DB schema는 별도 계약입니다.
  문법/기본값/진단/목록 해석 변경은 parser revision, 캐시 배치 변경은 cache schema를 갱신합니다.

상세: [ChartFormats](ChartFormats.md), [Timing](Formats/Timing.md), [ResourceLoading](ResourceLoading.md).
검증: ChartParserTests, YmeEffectsTests, ChartReviewRegressionTests, SongCatalogTests.

## LOAD-01 로딩과 캐시

근거: 사용자 확정 + 프로젝트 계약. 구현: 구현.

- 곡/패턴 목록과 실제 플레이 세션 구성은 worker에서 수행해 게임 Update를 막지 않습니다.
  목록은 메타데이터/노트 수/진단만 보관하고 실제 세션은 현재 원본으로 검증합니다.
- worker는 경로/데이터를 소유합니다. Scene/노드/엔진 observer를 캡처하지 않으며 최신 선택/루트의
  결과만 게시합니다. GPU/Canvas 등록은 Scene 스레드, 종료 시 작업 정리는 별도 수명 경계입니다.
- 첫 입력음 준비 전에 플레이 화면을 노출하지 않습니다. 샘플·음악·이미지 준비와 실제 Gameplay
  진입을 기다립니다. 준비 실패는 오류/Pause/Retry로 전달하며 시간만 먼저 진행하지 않습니다.
- 로딩 BG는 전체 플레이 자원 준비보다 먼저 시작합니다. 선곡에서 등록한 BG는 로딩 씬
  초기화 중 동일 handle을 연결해 첫 로딩 프레임부터 표시하며, 미등록 BG는 비동기로 준비합니다.
- 목록 디스크 캐시는 원본에서 재생성합니다. 영속 기록 DB는 같은 방식으로 삭제하지 않습니다.
- 곡 목록은 검증이 끝난 곡·패턴을 탐색 중 바로 추가하며 첫 로딩 전용 문구는 두지 않습니다.
  첫 탐색 중 곡이 아직 없거나 패턴이 추가되지 않은 곡은 `NO DIFFICULTIES`로 표시합니다.
  읽힌 패턴은 전체 탐색 중에도 선택·플레이/편집기 진입을 허용하고 나머지 탐색은 계속합니다.
  새 난이도 생성·가져오기는 전체 탐색 후 허용합니다. 최신 루트만 게시하고 부분/최종 갱신은
  선택 경로·검색·필터를 보존합니다. 파싱·컴파일·세션 구성은 협력 취소하며 UI에서 join하지 않습니다.

상세: [ResourceLoading](ResourceLoading.md), [Audio](Gameplay/Audio.md).
검증: SongCatalogTests, MusicSelectViewTests, GameplayPauseTests의 cold-start/실패/Retry.

## CONV-01 변환과 검증

근거: 사용자 확정. 구현: 구현.

- 변환기는 파일 포맷과 Taiko 노트 계약만으로 동작해야 합니다. Client 전체·Scene·엔진·
  실제 플레이 세션을 변환기의 의존성으로 추가하지 않습니다. 구현은 기존 전용 채팅/독립 저장소가 담당합니다.
- 변환기는 입력/시간 계산·직렬화 왕복·경로/저장 안전성을, Client OsuImportValidation은 실제 저장
  결과의 참조·노트 행/원본 객체 수·모드 세션 생성·누락 여부를 검증합니다.
- osu! 100/OK 폭과 YOUMU GOOD 폭을 맞추되 JudgeLevel은 0.01 정밀도를 유지하고 가장 가까운 값을
  선택합니다. 이 정밀도 때문에 생기는 오차를 없애려고 포맷 정밀도를 임의로 바꾸지 않습니다.
- 배경은 Songs/BG로 가져와 선곡/로딩/플레이에서 사용합니다. 기존/공유 파일의 충돌은 보존합니다.
  +25ms 보정·격자·BG 좌표·지원 범위는 변환 문서의 현재 계약을 따릅니다.

상세: [OsuTaikoConverter](OsuTaikoConverter.md), [EditorSongSelect](EditorSongSelect.md).
검증: 변환기 단위/CLI + Client OsuImportValidationTests/실제 모드 세션.

## GAME-01 RPG 포커스

근거: 사용자 확정, RPG의 NoteUpdateTaikoMode/MoveTargetNote. 구현: 구현.

사용자 정정은 “입력이 있든 없든” 앞 노트가 LATE BAD이고 다음 노트가 GOOD 범위에
도달하면 앞 노트를 바로 MISS 처리한다는 것입니다. 입력이 있어야 전환한다는 해석으로 되돌리지 않습니다.

RPG 참조 위치는 Client 루트 기준 형제 경로 `../RPG/RPG/GameScene/PlayScene.cpp`입니다.
NoteUpdateTaikoMode는 키 검사 전에 MoveTargetNote(..., Good)를 호출합니다.
YOUMU의 적용 코드는 [Lane](../YOUMU.Rhythm/Lane/Lane.cpp)의 ResolveFocus/GoodFocusIndex이며,
적용 근거 커밋은 [6786b9c](https://github.com/4dblackhole/YOUMU/commit/6786b9c416389a03c3bc5785fe12034a755b99f4)입니다.

- 시간순 첫 미처리 노트에 우선권을 줍니다. 현재 타이밍 노트가 LATE BAD이고 뒤에 GOOD 이상
  후보가 있으면 **입력 없이도** 그 첫 후보로 이동하고 건너뛴 미처리 노트를 MISS로 확정합니다.
- 후보 선택은 입력 유무/색과 독립적입니다. 입력이 들어오면 이미 선택한 노트가 그 색을 판정합니다.
- GOOD 후보가 없으면 현재 노트를 전체 판정 범위까지 유지합니다. 범위 끝은 포함하며,
  그 다음 us에 만료합니다. 확정 MISS는 반복하지 않습니다. EARLY BAD는 시도만 보고 포커스를 유지합니다.
- 성공 입력 하나는 노트 하나에만 전달합니다. 앞 MISS + 다음 HIT는 배드말림 방지의 정상 조합이며
  두 노트에 성공 입력을 중복 적용하는 동작과 구분합니다.
- 진행 중 롱노트 몸통/확정 헤드는 고유 테일·틱 규칙을 따릅니다. 후보의 실제 GOOD 프로필과
  Syncopation 확장을 사용합니다. 동일한 포커스 갱신을 일반 Update와 timestamp 입력에 적용합니다.

레벨 50, 일반 노트의 GOOD ±54.5ms/BAD ±90ms를 사용한 필수 사례:

| 앞/뒤 노트 시각 | 현재 시각·입력 | 결과 |
| --- | --- | --- |
| 0 / 100ms | 60ms, 입력 없음 | 앞 LATE BAD → MISS, 뒤 GOOD → 포커스만 이동, HIT 없음 |
| 0 / 100ms | 60ms, 뒤 노트에 맞는 키 | 앞 MISS + 뒤 HIT 하나 |
| 동 0 / 캇 100ms | 60ms, 동 입력 | 앞 MISS, 캇에 포커스, 그 입력은 캇에서 거부 |
| 0 / 200ms | 60ms, 입력 없음 | 뒤가 GOOD 후보가 아니므로 앞 노트 유지 |
| 0 / 200ms | 90ms / 90.001ms | 후보가 없다면 앞 유지 / 앞 MISS |
| 0 / 0ms | 0ms, 입력 하나 | 안정 순서의 첫 노트만 HIT, 다음은 별도 입력 대기 |

상세: [Rules](Gameplay/Rules.md#lane과-배드말림-방지).
검증: [TestLaneFocusRules](../Tests/Rhythm/Submodules/RhythmCoreTests.cpp),
[TestExpiredInputOrdering](../Tests/Rhythm/Submodules/InputOrderingTests.cpp),
[GameplayPauseTests.CheckAutomaticFocus](../Tests/Presentation/GameplayPauseTests.cpp).

## GAME-02 판정과 정확도

근거: RPG 판정 기준 + 프로젝트 계약. 구현: 구현.

- JudgeLevel은 1.00~100.00/0.01 단위이며 기본 50입니다. 범위는 50/L 배율을 적용합니다.
  레벨 50의 반범위는 MAX 4.5, PERFECT 9.5, GREAT 23.5, GOOD 54.5, BAD 90ms입니다.
- 타이밍 점수는 판정 경계 사이를 보간합니다. 정확도는 0.000001%p 정수 단위이며
  입력/논리 노트/평균의 소수점 6자리 아래는 버립니다.
- 각 유효 논리 노트는 동일 가중치 1입니다. 큰/보라 노트는 두 입력을 각 1/2로 평균하고,
  연타는 min(인정 입력/목표 입력, 1), 틱형은 성공 틱/전체 틱을 사용합니다.
- Buzz는 헤드 점수 1/2 + 몸통 성공률 1/2이며 몸통 틱이 없으면 헤드만 사용합니다.
  HUD는 확정된 노트 평균, 최종 결과는 미처리 0점을 포함한 전체 노트 평균입니다.
- 정확도 없는 연타 입력에 가상의 타이밍 등급/오차 표본을 만들지 않습니다.

상세: [Rules](Gameplay/Rules.md#노트별-정확도와-세션-집계), [Results](Gameplay/Results.md).
검증: RhythmCoreTests, AccuracyTests, PlayResultTests.

## GAME-03 POOR

근거: 사용자 확정. 구현: 정확도·HP 구현.

- POOR는 거부 이벤트이며 즉시 노트를 소비하지 않습니다. 이후 올바른 입력을 기다려
  배드말림을 방지하되, 일반/큰/보라 노트의 최종 정확도는 전체 0입니다.
- 큰 노트는 개별 입력의 기본 가중치가 1/2이어도 POOR 패널티는 **노트 전체 0**입니다.
  POOR 뒤 두 타격 모두 MAX여도 최종 0입니다. RPG의 기존 1/2 패널티를 복사하지 않습니다.
- Buzz 헤드 POOR는 헤드만 0입니다. 몸통을 모두 성공하면 최종 50%입니다.
- 키 해제(InputEdge::Released)·판정 범위 밖 입력을 이 패널티로 취급하지 않습니다.
  이 키 해제는 Release 빌드라는 뜻이 아니며 판정 규칙은 Debug/Release 공통입니다.
- HP 설계에서는 POOR 발생 즉시 감소하지 않고 대상 판정이 확정될 때 MISS에 해당하는
  감소를 반영합니다.

상세: [Rules](Gameplay/Rules.md), [HP 설계](Formats/Ymp.md#hp-게이지).
검증: [TestPoorSingleNoteAccuracy·TestPoorLargeNoteAccuracy·TestBuzzTickColorFocus](../Tests/Rhythm/Submodules/AccuracyTests.cpp).

## GAME-04 Buzz와 틱

근거: 사용자 확정. 구현: 정확도/틱·HP 구현.

- 정상 처리한 헤드는 확정됩니다. 이후 입력은 몸통 틱을 대상으로 하며 헤드의 판정·POOR·
  정확도를 다시 바꾸지 않습니다. 반대 색을 유지하는 각 틱은 TickRejected/POOR 하나로 확정합니다.
- 헤드 MAX, 몸통 9틱 중 1~3 성공·4~6 실패·7~9 성공이면
  정확도와 설계상 HP 회복률은 모두 `0.5*1 + 0.5*(6/9) = 83.333333…%`입니다.
- 잘못된 색을 놓고 올바른 색을 유지하면 이후 틱은 다시 성공할 수 있습니다.
  실패 틱 때문에 헤드·이전 성공 틱·몸통 전체를 0으로 만들지 않습니다.
- TickRoll/Buzz는 헤드 기준 유리수 격자를 사용하고 정확히 정렬된 유효 테일 틱을 포함합니다.
  Buzz 헤드는 몸통 틱에 중복 포함하지 않습니다. Buzz 틱과 같은 시각에 필요한 색의
  마지막 유지 키를 놓으면 그 틱은 실패합니다. 보조 키가 남아 있는 경우는 TIME-01을 따릅니다.
- 정상 진행에서 확정 틱을 소급 변경하지 않습니다. 명시적 Rewind의 이력 복원은 별도 계약입니다.

상세: [Rules](Gameplay/Rules.md), [Timing](Formats/Timing.md).
검증: [AccuracyTests](../Tests/Rhythm/Submodules/AccuracyTests.cpp),
[BuzzReplayTests](../Tests/Rhythm/Submodules/BuzzReplayTests.cpp),
[InputOrderingTests](../Tests/Rhythm/Submodules/InputOrderingTests.cpp),
[PlayPauseTests](../Tests/Rhythm/Submodules/PlayPauseTests.cpp).

## PLAY-01 플레이 옵션

근거: 사용자 확정 + 채택한 옵션 계약. 구현: 구현.

| 옵션 | 유지할 의미 |
| --- | --- |
| Scroll Speed | x 또는 BPM 입력으로 스크롤 거리 배율 적용. Ignore scroll effects는 스크롤 자동화만 무시하며 음악/판정 시각을 바꾸지 않음 |
| Metronome | 활성/비활성, 컴파일한 4분음표 박과 마디 강조 |
| Mirror | 작은 동/캇, 큰 동/큰 캇, 동/캇 Buzz 반전; 보라·롤·풍선·뎅뎅 유지 |
| Random | 지정 그룹 안의 **재배치 시도 확률**. 100%도 반드시 다른 색이 되는 것은 아님. 같은 uint64 시드는 같은 결과, 롱노트는 한 번만 추첨 |
| Accelerate | 0.5~2.0x. 실제 판정 폭은 기본/배속. 시계 배율 외에 판정 프로필을 다시 나눠 이중 축소하지 않음 |
| 음정 | BGM/HIT 음정 유지 여부를 독립 선택. 현재 HIT 유지는 원본 음정/길이, BGM은 준비 단계의 PCM 변환 사용 |
| JudgeLevel | 채보 기본값 또는 0.01 단위 덮어쓰기, 실제 사용 값을 결과/기록에 보존 |

Mirror 다음 Random 순서입니다. 앱 실행 중 선택을 유지하고 시작 때 값으로 복사하며,
Retry는 방금 플레이한 옵션/시드를 다시 사용합니다. 원본 YMM/YMP/YME는 수정하지 않습니다.

사용자 승인에 따라 선곡의 Play options는 아래에서 올라오는 게임 내 세 열 패널입니다.
Difficulty / Layout / Assist 분류와 하위 설정 펼침·필요 시 단일 본문 스크롤을 사용합니다.
후속 지시로 Apply를 제외하며 Close/Esc(기존 Enter 확정 포함)에 전체 값을 검증·적용합니다.
오류 시에는 닫지 않고 마지막 적용값을 유지합니다. Reset은 전체 편집값을 기존 PlayOptions
기본값으로 복원하고 닫을 때 적용합니다. 별도 취소·파일 저장·새 플레이 옵션은 추가하지 않습니다.

상세: [PlayOptions](Gameplay/PlayOptions.md). 검증: PlayOptionsTests, MusicSelectViewTests의
PlayOptionsPanelTests(닫기 적용·Reset·오류·모달 격리·실 네이티브 입력), GameplayPauseTests.

## PAUSE-01 일시정지와 복원

근거: 프로젝트 계약 및 사용자 지정(마지막 타겟 처리 후 ESC). 구현: 구현. HP 연동 구현, 리플레이 재생은 미구현.

- 플레이 중 ESC는 해당 QPC 시각까지 판정합니다. 모든 레인에 다음 타겟이 없으면
  즉시 정상 종료 결과로 이동하고, 타겟이 남아 있으면 일시정지 메뉴를 엽니다.
  이후 입력과 held 전이는 판정에서 제외합니다. Resume은 판정 상태를 유지하며
  기본 0.5초(0.5~3초 선택) 이전부터 재개합니다.
- Rewind는 기본 4마디(조절 가능) 전의 컴파일된 마디 시작으로 이동하고 대상 시각 **이상**의
  판정을 취소합니다. 이전 헤드/틱/물리 상태와 전체 입력·제어 마커는 보존합니다.
- Retry는 자산을 재사용하는 새 시도, Retire는 남은 논리 노트를 MISS·0점으로 확정합니다.
  Retire 전에 확정된 부분 점수는 바꾸지 않습니다.
- 음악만 paused Voice로 유지하고 지난 transient 샘플을 재개하지 않습니다.
  준비 완료 후 음악/시계를 같은 새 QPC/DSP 기준으로 시작합니다.
- ESC 메뉴 사용은 그 시도의 연습/클리어·HP 자격에 남으며 Resume/Rewind로 복구하지 않습니다.
  CanUseHp는 자격 정보이며 실제 종료 HP 합격 여부와 함께 클리어를 결정합니다.
- 일시정지 메뉴 또는 Rewind 사용 시 HP 처리를 비활성화하고 이전 값·이력은 보존합니다.
  Resume·Rewind·Retire는 이후 HP를 누적/재계산하지 않으며 Retry만 0%로 다시 활성화합니다.

상세: [Pause](Gameplay/Pause.md), [Audio](Gameplay/Audio.md).
검증: PlayPauseTests, GameplayPauseTests, ResultPresentationTests.

## DEBUG-01 디버그 조작

근거: 사용자 확정. 구현: 구현.

- Debug 빌드의 Space는 시간/노트 이동/음악을 정지해도 노트 입력·판정·히트사운드를 허용합니다.
  일반 곡에서도 사용하며 ESC 메뉴 정지와 구분합니다.
- 1/2 유지: 실제 1초당 차트 시간 -/+1초. Accelerate와 독립입니다.
  3/4 누름: -/+1ms 단발 이동. 현재 시각은 signed us와 소수점 3자리 ms로 표시합니다.
- 뒤로 이동하면 대상 시각 이상의 판정을 취소하고 이전 이력을 복원합니다.
  과거 MISS 피드백/오디오를 반복 출력하거나 세션 전체를 매번 새 시도로 초기화하지 않습니다.
- 공통 GOOD 우선 포커스는 정지 중 시간 탐색에도 적용합니다. Space 재개는 음악 준비와 시계를
  재정렬합니다. Space/1~4/타이머 표시 기능은 Release에 활성화하지 않습니다.

상세: [Debugging](Gameplay/Debugging.md).
검증: [GameplayPauseTests](../Tests/Presentation/GameplayPauseTests.cpp),
[GameplayJudgementTests](../Tests/Presentation/GameplayJudgementTests.cpp).

## EDIT-01 에디터

근거: 사용자 확정 + 프로젝트 계약. 구현: 구현.

- IEditorDocument의 원본은 안정 ID/유리수 위치 트리이며 게임플레이 INote 상태와 분리합니다.
  IEditorMode는 모드별 도구/배치/표시 정책, Workspace는 공통 명령과 수명을 소유합니다.
- 편집 전체 32동작 Undo/Redo, 선택/다중 선택·복사·붙여넣기를 유지합니다.
  Ctrl+V는 실제 겹침 교체, Ctrl+Shift+V는 복사 영역의 공백까지 교체합니다.
- 실패한 배치/수정/저장은 원본·ID·dirty·Undo/Redo를 보존합니다. 두 파일 staging/backup/
  설치 전에 YMP/YME/tmp/bak 경로의 정규화·대소문자·hardlink 별칭을 검사합니다.
- 빈/미지원 모드를 Taiko로 대신 열지 않습니다. 모드 context와 분석의 읽기 전용 계약을 분리합니다.
- 실시간 뷰는 컴파일 시각 차이를 Base BPM·시간축 줌으로 환산한 등속 시간축입니다.
  YME Whole·Separate 속도 효과를 노트·롱노트·마디선·그리드·선택·스냅에 적용하지 않습니다.
  같은 시간 차이는 BPM 변화 구간에서도 같은 거리입니다. 원본 효과는 보존하며 플레이에서 적용합니다.
- 실시간 뷰에서 선택 도구로 빈 레인을 클릭해도 재생 위치를 옮기지 않고 선택만 해제합니다.
  악보 뷰의 빈 그리드 클릭은 해당 시각으로 이동합니다.
- 저장 버튼 옆 Test는 저장 확인 후 현재 편집 시각·1.0배속으로 실제 플레이를 엽니다.
  이전 헤드와 시작 시각에 걸친 롱노트를 제외하고 Space 정지/재개와 상단 0.25~2.0배속
  (음높이도 변경)을 제공합니다. Esc/곡 종료 시 리절트 없이 편집 문서·Undo/Redo·시각/배속으로
  복귀합니다. 일반 Accelerate 범위와 YMP 형식은 유지합니다.

상세: [ChartEditor](ChartEditor.md). 검증: ChartEditorTests, ChartReviewRegressionTests, EditorModeTests.

## EDIT-02 오디오 뷰

근거: 사용자 확정 + 채택한 분석 계약. 구현: 구현.

- 실시간/오디오 뷰는 공통 Space·휠 탐색을 사용합니다. 확대 파형/스펙트로그램은 저해상도
  개요 이미지를 늘리는 대신 원본 PCM을 조회/재분석합니다.
- 원본 min/max 파형 피라미드, 화면 구간/픽셀 해상도의 비동기 STFT 타일과 무효화 캐시를 사용합니다.
  호스트가 엔진 공통 PCM reader를 주입하고 YOUMU.Editor는 분석만 소유합니다.
- 음악 재생과 분석은 같은 유효 PCM의 sample 0을 사용합니다. MP3 패딩은 공통 디코더에서 한 번
  제거하고 의도된 무음은 보존합니다. 분석 메모리/FFT 창 한계를 감추려고 시간 보정을 덧붙이지 않습니다.

상세: [ChartEditor](ChartEditor.md), [Audio](Gameplay/Audio.md).
검증: EditorAudioAnalysisTests, EditorAudioAnalysisIntegrationTests, EditorAudioViewTests.

## UI-01 화면과 키 설정

근거: 사용자 확정. 구현: 구현. Penpot의 추가 시안은 별도 상태.
확정된 다음 화면 설계는 [UI-02](#ui-02-ui-리디자인)이며 해당 묶음을 구현하기 전까지 아래 현재 계약을 유지합니다.

- 선곡 노트 수는 기본 플레이 세션의 실제 논리 노트 객체 수로 집계합니다. 롱노트의
  머리·꼬리 행이나 개별 입력/틱을 별도 노트로 세지 않습니다. 목록 표시와 노트 수 필터는
  같은 값을 사용하며 집계할 수 없으면 `—`로 표시하고 노트 수 범위 필터에서는 제외합니다.
- 선곡 상세에 JudgeLevel·HP 난이도·변속 BPM 범위·길이·실제 배경을 표시합니다. 목록 행은 제목/아티스트입니다.
  HP 난이도는 JudgeLevel 아래에 소수 둘째 자리까지 표시하며 HP가 없는 채보는 기존 기본값 9.00입니다.
  Loading에는 곡/패턴 소개를 표시하고 실제 자원이 준비된 뒤 플레이 화면으로 전환합니다.
- 선택 채보의 정적 BG를 선곡·플레이·리절트 화면 전체의 배경으로 표시합니다.
  원본 비율과 채보 오프셋을 유지하며 리절트에는 플레이 선택 snapshot을 전달합니다.
  BG 미지정/읽기 실패는 기존 스킨 배경으로 복귀합니다. BGA·MP4 등 추가 그래픽 포맷은
  사용자가 추후 지정하므로 현재 이미지 참조 형식을 유지합니다.
- 사용자 정정으로 곡 목록의 BG 썸네일을 제거하고 제목·아티스트의 기존 카드 폭을 복원합니다.
  선택 채보의 전체 BG와 상세 미리보기는 유지하며, 곡 복귀는 기존처럼 첫 표시 채보입니다.
- 선곡의 기록·곡 정보·곡 목록 패널 바탕은 승인한 제안대로 불투명도 90%입니다.
  내부가 빈 테두리를 사용해 BG가 실제 비치며 글자·버튼·내부 정보 영역은 흐리게 하지 않습니다.
  에디터 선곡의 두 주요 패널과 스킨 패널 이미지에도 적용하고 resize/스킨 복귀 시 유지합니다.
- 선곡 검색은 모든 곡명·아티스트 표기와 곡/패턴 태그를 대상으로 공백 단위 AND 부분 일치를
  사용합니다. 대소문자와 단어 순서는 무시하며 서로 다른 필드에 단어가 나뉘어 있어도 일치합니다.
  텍스트 검색 자체는 곡 단위로 적용하고 패턴 목록은 잘라내지 않습니다.
  단일 `angel` 검색도 `Angel Dream`을 포함합니다. 입력은 검색창 내부의 Windows Unicode 편집기를
  사용하며 한글/일본어 IME·커서/선택·삭제·붙여넣기를 처리합니다. 조합 중 Enter/Escape와
  검색 편집 중 방향키/공백은 Scene 명령으로 해석하지 않습니다.
- rhythmgameUI의 신규 타이틀 캐러셀 설계는 사용자 지시로 폐기하고 현재 타이틀을 유지합니다.
  나머지 UI는 같은 화면·기능에 속한 요소를 묶어 구현 방식을 브리핑하고 질문합니다.
  사용자 답변을 받은 뒤 승인한 묶음부터 차례로 구현합니다.
- 전역 옵션은 General(설명 언어·Songs), Display(스킨·해상도), Audio(출력·오프셋),
  Input(키 설정)의 네 탭과 탭별 스크롤을 제공합니다. 모션 속도 설정은 사용자 지시로 폐기합니다.
- 항목명·버튼·값/단위 표시는 영어, 설명·도움말·오류 안내는 선택한 영어/한국어/일본어입니다.
  첫 설정은 시스템 UI 언어를 따르고 미지원 언어는 영어입니다. 저장된 ko/en과 사용자 파일을
  보존하며 같은 INI Language 키에 ja를 추가합니다. 역할 분류는 TextCatalog가 소유합니다.
- 선곡의 Base BPM 정렬은 사용자 승인에 따라 각 곡의 첫 패턴 기본 BPM을 기준으로
  오름차순 정렬합니다. 동률은 catalog 순서, 기본 BPM이 없는 곡은 마지막이며 검색과 결합합니다.
  기존 정렬 선택기에 추가하고 선택 곡의 포커스를 보존합니다.
- 곡명·아티스트·제작자의 복수 표기는 사용자 정정에 따라 각 칸에서 등록 순서대로
  여러 줄로 표시합니다. 넘치는 내용은 칸 내부 스크롤로 읽고 곡 목록 스크롤과 분리합니다.
  구분자로 한 줄에 합치거나 자동 마키로 순환하는 이전 제안은 이 칸들에 적용하지 않습니다.
- 노트 종류 비율 그래프와 NPS(초당 노트 수) 그래프는 사용자 지시로 폐기했습니다.
  두 그래프와 이를 위한 패턴 분석 버튼/패널은 구현 범위에 포함하지 않습니다.
- 선곡 필터는 별도 패널에서 모드·노트 수·기본 BPM·JudgeLevel 범위를 선택합니다.
  같은 패턴에서 모든 조건을 만족해야 하며 일치하는 패턴만 표시합니다. 빈 경계는 무제한,
  양 끝을 포함하고 적용 전 편집은 취소할 수 있습니다. 세부 동작과 회귀는 [선곡](SongSelect.md)에 있습니다.
  난이도는 향후 클라이언트에서 계산하며 현재 catalog 내부 optional 필드만 미계산으로 둡니다.
  YMP 형식과 디스크 인덱스는 유지하고 미계산을 0으로 해석하지 않습니다.
- 패턴별 즐겨찾기는 복수 폴더 소속을 지원하며 폴더 생성·이름 변경·삭제와 소속을 편집합니다.
  곡 목록 상단에 전체/폴더 탭을 두고 넘치면 가로 스크롤합니다. 검색·필터와 AND로 결합하고
  별도 `favorites.json`에 저장합니다. YMP는 유지합니다. 관리 패널은 전체 편집을 적용/취소하며
  파일 손상·저장 실패 시 사용자 파일과 적용 상태를 보존합니다. 상세 계약은 [선곡](SongSelect.md)에 있습니다.
- 플레이 모드/플레이 옵션은 선곡 하단, 전역 설정은 별도 패널입니다.
- 기록은 사용자가 지정한 기존 왼쪽 기록 영역에서 표시하며 별도 넓은 열람 창으로 옮기지 않습니다.
  기록 필터만 별도 패널에서 편집합니다. 적용/닫기 뒤 검색·선택 곡/패턴·목록 스크롤을 유지합니다.
- 플레이 진행바는 화면 아래 끝 전체 폭의 띠입니다(UI-02로 아래 여백 16px 배치를 대체).
- 화면상 일시정지 진입 버튼은 사용자 지정으로 구현 범위에서 제외합니다. ESC 메뉴는 유지합니다.
- 현재 콤보는 키 레이아웃 안에 표시합니다. 일반·큰·보라 노트 GOOD 이상 완료/롱노트 OK당
  1을 더하고 BAD·MISS·POOR에서 0으로 만듭니다. 최대 콤보는 같은 수집기에서 보존합니다.
  판정별 누적 횟수는 기존 리절트 집계와 같으며 Retry는 초기화, Rewind는 이력으로 복원합니다.
- 인게임 SCORE는 현재 정확도(0–1) × 100,000,000 × 처리한 논리 노트 비율입니다.
  확정 정확도 정수 단위의 합을 전체 유효 논리 노트 수로 한 번 나눠 버림합니다.
  리절트 SCORE에는 종료 시점의 인게임 점수값을 전달합니다.
- Taiko 키 변경은 버튼 하나로 Gameplay 키 레이아웃 이미지를 쓰는 네 열 패널을 엽니다.
  각 열은 강한 불빛의 메인 키 1개와 각각 비울 수 있는 보조 키 3개입니다.
  기본 메인 D/F/J/K와 첫 보조 C/V/N/M을 사용자가 변경할 수 있습니다.
  키 설정 패널은 옵션 패널보다 앞에 표시되며 겹친 영역의 포인터 입력도 우선합니다.
- 에디터는 선택 도구로 시작합니다. 파일 메뉴는 저장/YMM·YMP·YME 폴더 열기,
  편집 메뉴는 Undo/Redo/Copy/Paste를 제공하고 기존 단축키와 같은 명령을 연결합니다.
- 실시간 마디선 최소 5px·틱 2px 요구와 4분 흰색/8분 빨강/16분 파랑/32분 노랑/
  64분 회색/12분 초록/24분 보라/나머지 주황색을 보존합니다. 선택 아이콘은 커서+지우개입니다.
- Lane 요소는 로컬 부모 Transform을 함께 회전합니다. UI/표시 변경은 판정 시각과 점수 규칙을 바꾸지 않습니다.
  시안의 모든 장식을 자동으로 기능 요구로 승격하지 않습니다.

상세: [SongSelect](SongSelect.md), [TextManagement](TextManagement.md), [ChartEditor](ChartEditor.md),
[Presentation](Gameplay/Presentation.md), [DesignReferences](DesignReferences.md).
검증: MusicSelectViewTests, OptionsPanelTests, EditorMenuTests, GameplayJudgementTests와 해당 시각 검증.

## UI-02 UI 리디자인

근거: 사용자 확정(rhythmgameUI/redesign 시안 검토 결정과 이후 구현 지시). 구현: **일부 구현**.

- 타이틀·곡 선택·에디터 곡 선택·플레이 옵션·옵션 패널·로딩·플레이 HUD·일시정지·결과·리플레이·
  차트 에디터와 공용 부품·경고 대화상자의 화면 설계를 확정했습니다. 세부는 [UI 리디자인](UIRedesign.md)입니다.
- 사용자가 설계 전체의 게임 적용을 지시했습니다. 화면·기능 묶음 순서대로 구현하고, 구현한 묶음은 해당
  기능 문서의 현재 계약으로 옮기며 아래 목록을 갱신합니다. 구현하지 않은 묶음은 기존 계약대로 동작합니다.
- 구현한 묶음:
  - 표시 규칙: 정확도 표시는 소수 둘째 자리 버림(계산·저장·랭크 판정은 0.000001% 단위 그대로),
    랭크 이름과 skin.ini 키·PNG는 X·SS·S·AA, 기록이 없는 패턴은 `NO RECORD`.
  - 결과 화면: 배너·왼쪽 HP/요약/판정 열·판정 비율 원 안의 랭크와 점수·오른쪽 Timing 그래프·하단 띠,
    16:9 미만 레터박스와 32:9 초과 좌우 띠(결과 화면에 한함), 시안 글꼴. 기록 저장 실패 대화상자는 공용
    대화상자 묶음에서 구현합니다. 게이지는 클리어 기준(`HpGaugeState::clearPercent`)을 보고합니다.
  - 플레이 HUD: HP 게이지 다섯 부품(틀·채움 두 종류·클리어 표시·자물쇠), 오른쪽 위 정확도·점수 판, 판정원 아래 타이밍
    인디케이터, 오른쪽 아래 판정 수 판, 키 레이아웃 안 콤보, 화면 아래 진행 띠. 숫자는 시안 글꼴로 그립니다.
    기어 띠·입력 패널 색조·마디선·키 누름 색, Kiai 판정원, 풍선·뎅뎅 그림과 남은 타격 배지, 화면 오류 줄은 아직 예전 모습입니다.
- 구현되면 대체하는 현재 계약(미구현 묶음):
  - UI-01: 현재 타이틀 유지(캐러셀 폐기는 유지), 선곡 패널 바탕 90%, 복수 이름 여러 줄 표시,
    옵션 네 탭 전환, 상단 `ALL` 바.
  - 표시 규칙: 일시정지 제목/설명 없음 → `PAUSED`와 설명, 좁은 화면 축소 → 16:9 미만 레터박스·32:9 초과 좌우 띠
    (결과 화면은 구현됨).
  - 에디터: 탭 구성(Compose·Audio·Metadata), Test 버튼, 새 난이도 입력 창·.osu 진행 표시.
- 구현 범위에 대한 사용자 결정:
  - 내장 스킨 이름은 `Default Skin`을 유지하고 그림은 시안 부품(Default Skin Blue)으로 전체 교체합니다.
    시안에 없는 노트·레인·판정원·키·판정 문구 그림과 히트사운드·메트로놈은 기존 파일을 계속 씁니다.
  - 글꼴은 시안의 Nunito·Barlow Semi Condensed(SIL OFL 1.1)를 라이선스와 함께 내장합니다.
  - 타이틀 음악 플레이어는 썸네일·진행 막대·재생/일시정지·정지·다음 곡만 두고 주파수 시각화는 두지 않습니다.
  - 음량 조절 옵션을 추가합니다. 모션 속도·움직임 줄이기 설정은 두지 않습니다(UI-01의 모션 속도 폐기 유지).
  - 리플레이는 버튼만 두고 재생 기능은 DATA-01대로 미구현으로 남깁니다.
  - 한글·일본어 조합 글자의 전용 표시는 구현 범위에서 제외합니다(입력은 기존 Windows 편집기 유지).
  - 계산 난이도는 식이 정해질 때까지 모든 자리에 `—`를 표시하고 Difficulty 정렬은 catalog 순서를 유지합니다.
- 판정·정확도 계산·점수·HP 규칙은 바꾸지 않습니다. HP 게이지 선택 자리는 두지만 추가 게이지는 HP-02대로 미정입니다.
- 미정점: 난이도 계산식·척도, 추가 HP 게이지, 영어·일본어 문구 길이, 새 부품의 skin.ini 키 이름.

상세: [UIRedesign](UIRedesign.md). 검증: 구현 묶음마다 해당 기능의 기존 회귀와 시각 검증을 정합니다. 표시 규칙은 ResultPresentationTests(정확도 버림·랭크 21개 경계),
GameplayJudgementTests, MusicSelectViewTests, LanguageFileTests. 결과 화면은 ResultPresentationTests(배치·판정 비율 원 계산·
그래프 영역·버튼)와 화면 캡처 확인.

## DATA-01 결과와 기록

근거: 사용자 확정 + 프로젝트 계약. 구현: 결과/로컬 기록 구현, 리플레이 재생 미구현.

- 실제 시도의 정확도·판정·오차·그룹 통계·옵션을 결과로 전달하고 옵션 아이콘을 표시합니다.
  논리 노트 평균과 개별 입력 판정 개수를 같은 분모로 혼동하지 않습니다.
- 리절트는 HP 판정이 가능한 정상 종료 실패만 HP 그래프 내부에 표시합니다. 클리어·중도
  종료의 별도 표시는 제외합니다. Retire는 남은 노트를 MISS·0점으로 확정하는 기존 경로를
  유지하며 종료 사유나 실제 HP 곡선을 바꾸지 않습니다. HP 비활성화 완주는 실패로 표시하지 않습니다.
- 재실행 후에도 남는 SQLite 기록과 제한된 조건식을 사용합니다.
  `JudgeLevel >= 60 AND Random = true` 등의 비교/논리식을 지원하며 임의 SQL 실행으로 확장하지 않습니다.
- 기록 필터의 선택형 옵션·포함 범위·시드는 지정한 항목만 AND로 결합합니다. 미지정은 제한 없음이며
  직접 입력한 AND/OR/NOT 조건식 전체와도 AND로 결합합니다. 적용 전에는 기록 조건을 바꾸지 않고
  닫기/Escape/Scene 숨김은 편집을 취소합니다. 기존 저장 형식·페이지 조회는 유지합니다.
- 기록 DB의 손상/미지원 버전은 오류로 알리고 보존합니다. 원본에서 재생성하는 목록 캐시와
  달리 삭제/재생성하지 않습니다. 원본 음원·곡·스킨·설정도 사용자 데이터로 보존합니다.
- 입력/제어 로그와 현재 가지의 최종 통계를 구분합니다. 취소된 입력도 로그에 남지만
  제어 마커를 무시한 전체 재판정으로 결과 통계를 재구성하지 않습니다.

상세: [Results](Gameplay/Results.md), [PlayOptions](Gameplay/PlayOptions.md#로컬-기록).
검증: PlayResultTests, PlayPauseTests, MusicSelectViewTests, ResultPresentationTests.

## HP-01 기본 HP 설계

근거: 사용자 확정 및 인게임 구현 재개 지시. 구현: **기본 게이지 구현**. 기본 체력 레벨은 9.00입니다.

- 기본 게이지는 시작 0%, TOTAL 120%p, 종료 60% 이상 합격, 0~100% 제한, 중도 실패 없음입니다.
  이전에 계산해 본 다른 시작값/TOTAL/합격선을 현재 기본안으로 재사용하지 않습니다.
- 여러 게이지를 동시에 계산하고 달성한 가장 높은 등급으로 판단하는 방향입니다.
  다른 게이지의 규칙·등급 순서는 아직 정해지지 않았습니다.
- N은 총 유효 논리 노트 수, 기본 증가량 g = 120%p/N입니다. 각 구성 요소 가중치 합은
  논리 노트당 1이며, timing MISS/BAD 감소는 g × 체력 레벨 × 해당 가중치입니다.
- MAX/PERFECT의 회복률은 1, GOOD은 0입니다. GREAT는 PERFECT/GREAT 바깥 경계 사이에서
  1→0으로 보간합니다. 이는 GAME-02의 정확도 점수와 다른 회복 함수입니다.
- 등급 없는 성공 입력/틱은 자기 가중치의 100%를 회복합니다. 미처리 비타이밍 연타/틱은
  회복하지 않으며 별도 감소도 없습니다. 큰/롤 노트는 완료 후, 틱은 결과 확정 때 반영합니다.
- Buzz 회복량은 g × (0.5×헤드 회복률 + 0.5×몸통 성공률)입니다.
  GREAT 중간 헤드 회복률 0.5와 틱 40%이면 g×0.45입니다.
- POOR는 발생 즉시 HP를 바꾸지 않습니다. 일반/큰/보라 노트는 최종 MISS 상당 감소,
  Buzz 헤드는 헤드 가중치만 감소, 몸통 POOR 틱은 해당 틱의 회복만 없습니다.
- 나쁜 판정 5%는 밸런스 참고값입니다. 별도 실패/합격 조건으로 추가하지 않습니다.

상세: [YMP HP 설계](Formats/Ymp.md#hp-게이지).
검증: HpGaugeTests의 타이밍/구성요소/POOR/상한/되감기/종료 판정·빈 채보와 GameplayJudgementTests의 두 사각형 표시 회귀.

## HP-02 HP 구조와 미정점

근거: 사용자 제시 구조·구현 브리핑 승인. 구현: **기본 게이지 및 YMP 필드 구현**, 추가 게이지는 미정.

- Scene이 `unique_ptr<IHpGauge>` 배열을 소유하고 기본 게이지 하나를 생성합니다. 세션은
  비소유 포인터로 기존 판정 결과를 순서대로 전달하며 화면은 상태를 조회해 표시합니다.
- `[Difficulty]`의 `JudgeLevel` 아래 `HP:`를 저장합니다. 1.00~100.00, 0.01 단위,
  두 자리 고정 출력이며 누락 시 기본 9.00입니다. osu→YMP는 9.00을 출력합니다.
- 유효 논리 노트 N=0은 일반 플레이 진입을 차단합니다. 에디터 미리보기/테스트에서는
  허용하되 HP 판정에서 제외합니다.
- 일시정지 메뉴 또는 Rewind가 HP를 비활성화합니다. 이전 값·곡선은 남기고 Resume·Rewind·
  Retire에서는 HP를 누적/재계산하지 않습니다. Retry만 0%·빈 이력·활성 상태로 시작합니다.
  기존 Debug 전용 예외는 유지하며 활성 HP의 Debug 역방향 탐색만 값·이력을 다시 계산합니다.
- 마지막 판정 이후 종료 HP로 클리어를 결정하며 기존 기록의 cleared 값을 사용합니다.
  HP 그래프는 실제 이력을 표시하고 사용한 조작만 내부에 요약으로 겹쳐 표시합니다.
  곡선은 비활성화 이후에도 유지합니다. 추가 게이지의 규칙과 등급 순서는 여전히 미정입니다.

상세: [YMP](Formats/Ymp.md#hp-게이지). 검증: HpGaugeTests, GameplayJudgementTests, GameplayPauseTests.

## PLAN-01 제안과 미구현 범위

근거: 현재 범위와 검토 제안의 구분. 구현: 항목별 아래 상태.

| 항목 | 상태 |
| --- | --- |
| HP 기본 규칙·YMP HP 필드 | 기본 게이지 구현, 추가 플로팅 게이지는 미정 |
| HP 구조 | Scene 소유·세션 판정 전달·상태/이력 조회와 결과 곡선 구현. 일시정지/Rewind는 비활성화, 추가 게이지는 미정 |
| SCORE 계산 규칙 | 구현. 인게임 정확도 × 100,000,000 × 처리한 논리 노트 비율, 종료 점수를 리절트에 전달 |
| 리플레이 재생/파일 저장 | 미구현. 기존 메모리 입력/제어 로그와 영속 성적 DB는 구현 |
| 랭크 콘텐츠·서버/라이선스 인증·공용 미디어 ID/BGA | RankedContentProposal의 검토 제안, 미구현 |
| 현재 등록 모드/편집 형식 | Taiko와 YMP/YME. 확장 인터페이스가 있다는 이유로 다른 모드 구현을 추정하지 않음 |
| Penpot의 나머지 시안 요소 | 시안 상태. 해당 기능 요청·구현 여부는 기능 문서로 확인 |
| UI 리디자인(UI-02) | 사용자 확정 설계, 일부 구현(표시 규칙·결과 화면·플레이 HUD). 나머지 화면은 묶음 순서대로 구현하며 리플레이 재생은 제외 |

제안을 채택하거나 보류를 해제할 때 사용자 요청과 영향 범위를 기록합니다.
현재 코드·문서에 이름이 등장한다는 이유만으로 구현 완료/채택 완료로 표시하지 않습니다.
