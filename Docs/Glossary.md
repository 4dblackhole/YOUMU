# 용어집

문서와 코드에 자주 나오는 말을 짧게 풀었습니다. 정확한 규칙은 각 줄의 링크가 기준입니다.

## 저장소와 프로젝트

| 용어 | 뜻 |
| --- | --- |
| YOUMU | 이 게임의 프로젝트·코드 이름. 표시 이름은 `Y's Orchestral Unison: Melodic Ultimatum`([ID-01](Design/Decisions.md#id-01-제품과-이름)) |
| PhantomCore | 재사용 엔진. 별도 저장소이며 YOUMU에는 서브모듈 `Dependencies/PhantomCore`로 고정본이 들어 있음 |
| OsuTaikoConverter | `.osu` 하나를 YMM·YMP·YME로 바꾸는 별도 저장소의 변환기 |
| Client | 엔진 위의 게임 쪽 코드(`Client/`). 화면·옵션·기록·자산 연결 |
| gitlink | YOUMU가 가리키는 엔진 커밋. 엔진을 바꾸면 엔진 PR 병합 뒤 이 값을 올림 |
| RPG | 플레이 규칙의 원래 코드베이스인 형제 저장소 `../RPG`. 규칙을 대조할 때만 읽음([GAME-01](Design/Decisions.md#game-01-rpg-포커스)) |
| 결정 ID | `TIME-01`, `GAME-02`처럼 [설계 결정](Design/Decisions.md)의 항목 이름. 바꾸기 전에 조건·예외를 확인함 |

## 곡과 채보 파일

| 용어 | 뜻 |
| --- | --- |
| 곡 폴더 | `Songs/<곡 폴더>`. YMM 하나와 그 곡의 YMP·YME·음원·배경 그림을 담음 |
| YMM | 곡 정보(제목·아티스트·음원 파일 등)([Ymm](Formats/Ymm.md)) |
| YMP | 패턴(난이도 하나). 메타데이터·HP·노트([Ymp](Formats/Ymp.md)) |
| YME | 효과. 스크롤·보간·히트사운드·볼륨·싱코페이션·Kiai([Yme](Formats/Yme.md)) |
| 마디·위치 | 채보 시각은 `마디, 분자/분모`의 유리수 위치로 쓰고 로드할 때 정수 마이크로초로 컴파일함([Timing](Formats/Timing.md)) |
| SongCatalog · SongLibrary | 카탈로그는 Songs를 훑은 곡·패턴 목록, 라이브러리는 목록·선택 상태·미리듣기 묶음(타이틀과 게임 곡 선택이 공유) |
| 내장 자산 | EXE에 묶인 기본 스킨·글꼴·기본곡. 실행 때 `%LOCALAPPDATA%/YOUMU/BuiltInAssets`에 풀림([자산·스킨](Features/Assets.md)) |
| 스킨 | `Skins/<폴더>`의 같은 상대 경로 그림이 내장 그림보다 먼저 쓰임 |

## 노트

| 용어 | 뜻 |
| --- | --- |
| 동 · 캇 | 빨강(가운데)·파랑(테두리) 기본 노트. 큰 동·큰 캇은 두 번 타격이 필요한 큰 노트 |
| 보라 | 동과 캇을 한 번씩(순서 무관) 쳐야 하는 노트 |
| 롤 · 틱 롤 | 구간 동안 연타하는 롱노트. 틱 롤은 틱마다 한 번씩만 받음 |
| 풍선 · 뎅뎅 | 정해진 횟수를 제한 시간 안에 치는 롱노트. 풍선은 동만, 뎅뎅은 동·캇 교대 |
| Buzz | 헤드를 친 뒤 같은 색을 누르고 있는 롱노트. 몸통 틱마다 판정([GAME-04](Design/Decisions.md#game-04-buzz와-틱)) |
| 헤드 · 테일 · 틱 | 롱노트의 시작·끝, 그 사이에 판정하는 격자 지점 |
| Lane · Gear | Lane은 노트가 흐르는 줄 하나, Gear(`ScrollGear`)는 Lane 묶음과 스크롤 |
| 논리 노트 | 정확도 집계의 단위. 큰 노트·롱노트도 하나로 셈 |

## 판정과 점수

| 용어 | 뜻 |
| --- | --- |
| MAX · Perfect · Great · Good · Bad | 타이밍 등급. 판정 레벨 50에서 ±4.5/9.5/23.5/54.5/90 ms([Rules](Features/Gameplay/Rules.md#판정과-점수)) |
| MISS | 치지 못하고 지나간 노트, 또는 Retire로 남은 노트 |
| POOR | 틀린 입력을 거부한 표시. 노트는 소비하지 않지만 그 노트의 최종 정확도는 0([GAME-03](Design/Decisions.md#game-03-poor)) |
| OK | 롤 계열 중 타격·틱을 하나라도 처리하고 끝난 노트(결과 화면의 수). 화면에는 처리 비율에 맞는 판정 글자가 뜸 |
| 정확도 | 논리 노트 점수의 평균(%). 타이밍 점수는 등급 경계 사이를 선형 보간하고, 최종 결과는 처리하지 못한 노트를 0점으로 넣음([GAME-02](Design/Decisions.md#game-02-판정과-정확도)) |
| SCORE | 정확도 × 100,000,000 × 처리한 논리 노트 비율 |
| JudgeLevel | 판정 폭 배율. 레벨 `L`의 폭은 레벨 50의 `50/L`배 |
| 싱코페이션 | YME 구간 안 노트의 판정 폭을 등급마다 10 ms 넓히는 설정 |
| Kiai | YME로 켜는 강조 구간. 판정원 연출이 바뀜 |
| HP 게이지 | 판정에 따라 오르내리는 체력. 기본 게이지는 60% 이상이면 클리어([Ymp HP](Formats/Ymp.md#hp-게이지)) |
| 배드말림 방지 | 앞 노트가 늦은 Bad이고 다음 노트가 Good 범위에 들면 앞 노트를 바로 MISS로 넘기는 포커스 규칙([GAME-01](Design/Decisions.md#game-01-rpg-포커스)) |

## 플레이

| 용어 | 뜻 |
| --- | --- |
| 플레이 옵션 | Scroll Speed·Accelerate·Mirror·Random·Metronome·JudgeLevel 덮어쓰기·HP 게이지([PLAY-01](Design/Decisions.md#play-01-플레이-옵션)) |
| Accelerate | 재생 속도 0.5~2.0배. 시계와 음악이 함께 빨라지고 판정 폭은 실제 시간 기준으로 줄어듦 |
| PlayState | 플레이 실행 상태 Loading · Gameplay · Pause · Ending([플레이 구조](Architecture/Gameplay.md#상태-전이)) |
| Resume · Rewind · Retry · Retire | 일시정지 메뉴의 재개·몇 마디 되감기·처음부터 새 시도·포기하고 결과로([Pause](Features/Gameplay/Pause.md)) |
| 개별 오프셋 | 패턴마다 저장하는 오프셋. 첫 노트 전에 +/- 키로 바꾸고 글로벌 오프셋에 더해짐([Audio](Features/Gameplay/Audio.md#개별-오프셋)) |
| 재시작 키 | 플레이 중 누르고 있으면(0.5초) 처음부터 다시 시작. 옵션에서 키를 바꿈 |
| 결과 · 기록 DB | 플레이가 끝나면 결과(`IPlayResult`)를 결과 화면으로 넘기고, 기록 DB(사용자 데이터, 게임 폴더 `Data/Result/plays.sqlite3`)에 저장함([Results](Features/Gameplay/Results.md)) |

## 시간과 엔진

| 용어 | 뜻 |
| --- | --- |
| QPC | Windows 고해상도 시계. 입력 이벤트마다 찍히는 시각이며 판정의 기준([TIME-01](Design/Decisions.md#time-01-시간과-입력)) |
| Raw Input | 키 입력을 순서와 QPC 시각 그대로 받는 Win32 입력. 프레임 단위 키 상태로 판정하지 않음 |
| DSP 시각 | 오디오 mixer의 sample 시계. 음악·메트로놈을 미래 시각에 예약할 때 씀 |
| RhythmTimer | 플레이 세션마다 하나인 시계. QPC·채보 시각·DSP 시각을 서로 변환함 |
| Scene | 화면 하나의 객체(타이틀·곡 선택·플레이·결과·에디터). 등록은 `RegisterScene`, 전환은 지연된 `ChangeScene` |
| KeepAlive · DestroyOnExit | Scene 수명 정책. 나갈 때 남겨 둘지, 파기하고 다음에 새로 만들지 |
| Visual2D · Canvas | 엔진의 2D 노드 트리와 그것을 그리는 화면. Scene은 Canvas handle과 노드만 들고 있음([Visual2DGuide](Features/Visual2DGuide.md)) |
| Snapshot(진입 요청) | 곡 선택이 넘기는 `GameplayLaunchRequest` 값 복사본. 플레이·에디터는 생성될 때 복사해 쓰므로 뒤의 선택 변경과 무관 |

## 작업 방식

| 용어 | 뜻 |
| --- | --- |
| 지휘 세션 | 요청을 배정하고 PR을 검증·병합하며 [진행 상황](Status.md)을 고치는 세션 |
| 작업 세션 · S1/O1~O3/F1 | 복잡도별로 유지하는 작업 세션([작업 방식](Contributing/Workflow.md#역할)) |
| 병합 요청 | 작업 세션이 PR을 연 뒤 지휘 세션에 보내는 정해진 형식의 메시지 |
| 공유 잠금 | 여러 세션의 빌드·게임 실행이 겹치지 않게 하는 `Scripts/Verify`의 대기열 잠금 |
| 러너 | `Tests/Presentation/Run*.ps1`. 실제 Client 코드를 링크해 화면·입력 동작을 검사하는 회귀 테스트 |
