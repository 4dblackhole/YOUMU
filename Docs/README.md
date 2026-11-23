# 문서 지도

어느 문서든 이 페이지에서 세 번 안에 닿도록 정리했습니다. 모르는 말은 [용어집](Glossary.md)에 있습니다.

## 누가 무엇부터 읽나

| 독자 | 읽는 순서 |
| --- | --- |
| 처음 참여하는 사람·AI 세션 | [진행 상황](Status.md) → [작업 방식](Contributing/Workflow.md) → [전체 구조](Architecture/Overview.md) → 아래 표에서 맡은 기능 |
| 기능을 고치는 사람 | [설계 기준](Design/Principles.md)·관련 [결정 ID](Design/Decisions.md) → 해당 기능 문서 → [변경 영향 지도](Contributing/ChangeImpactMap.md) → [빌드와 검증](Contributing/Verification.md) |
| 구조를 이해하려는 사람 | [전체 구조](Architecture/Overview.md) → [실행 흐름](Architecture/ExecutionFlow.md) → [플레이 구조](Architecture/Gameplay.md) → 엔진 [EngineOverview](../Dependencies/PhantomCore/Docs/EngineOverview.md) |
| 스킨·그림을 만드는 사람 | [자산·스킨](Features/Assets.md) → [UI 리디자인의 스킨 부품](Design/UI.md#스킨-부품) → [Penpot 참고](Design/References.md) |
| 채보를 쓰는 사람 | [채보 형식](Formats/README.md) → [YMM](Formats/Ymm.md)·[YMP](Formats/Ymp.md)·[YME](Formats/Yme.md)·[Timing](Formats/Timing.md) → [차트 에디터](Features/ChartEditor.md) |

## 폴더

| 폴더 | 담는 것 |
| --- | --- |
| `Docs/` 바로 아래 | [진행 상황](Status.md), [용어집](Glossary.md), 이 지도 |
| `Architecture/` | 구조 그림과 실행 흐름: 지금 코드가 어떻게 짜여 있나 |
| `Contributing/` | 일하는 방법: 작업 흐름, 검증, 변경 영향 지도, 새 모드 추가 |
| `Design/` | 왜 이렇게 정했나: 설계 기준, 결정 ID, UI 설계, 시안 참고, 검토 제안 |
| `Features/` | 지금 어떻게 동작하나: 기능별 계약 |
| `Formats/` | 파일 형식: YMM·YMP·YME 문법과 타이밍 |

## 문서 목록

### 구조와 일하는 방법

| 작업 | 문서 |
| --- | --- |
| 지금 구현된 것·진행 중·미구현·알려진 문제 | [Status](Status.md) |
| 역할·브랜치와 PR·병합 요청·검증 잠금·지켜야 할 것 | [Workflow](Contributing/Workflow.md) |
| 빌드·테스트·분석·`Scripts/Verify` 도구·CI | [Verification](Contributing/Verification.md) |
| 기능 → 소유 코드·회귀 검사·구조 그림 | [ChangeImpactMap](Contributing/ChangeImpactMap.md) |
| 모드 등록·레인 입력·모드별 화면 연결 | [AddingGameMode](Contributing/AddingGameMode.md) |
| 저장소·프로젝트 의존·영역별 경계·층·Scene 구성(그림) | [Architecture/Overview](Architecture/Overview.md) |
| 시작·Scene 전환·한 프레임·종료·소유권(그림) | [Architecture/ExecutionFlow](Architecture/ExecutionFlow.md) |
| 플레이 화면 클래스·노트 규칙·상태 전이(그림) | [Architecture/Gameplay](Architecture/Gameplay.md) |
| 용어(YMM/YMP/YME, 노트·판정 이름, QPC·DSP 등) | [Glossary](Glossary.md) |
| 엔진 API·수명 자체 변경 | 엔진 [EngineOverview](../Dependencies/PhantomCore/Docs/EngineOverview.md) |

### 설계

| 작업 | 문서 |
| --- | --- |
| 판단 순서·설계 결정 찾아보기·설계 변경 규칙 | [Design/Principles](Design/Principles.md) |
| 결정별 근거·조건·예외·구현 상태·재현 사례 | [Design/Decisions](Design/Decisions.md) |
| 확정 UI 리디자인·공용 부품·스킨 부품·남은 미구현 | [Design/UI](Design/UI.md) |
| Penpot 페이지·보드·자산 제작 | [Design/References](Design/References.md) |
| 음원 고유값·해시 이름 곡 폴더·가져오기 배치 순서 | [Design/AudioIdentity](Design/AudioIdentity.md) |
| 공용 미디어·랭크 무결성 제안(미구현) | [Design/Proposals/RankedContent](Design/Proposals/RankedContent.md) |

### 기능

| 작업 | 문서 |
| --- | --- |
| 로고·타이틀·음악 플레이어·F7 성능 띠 | [Title](Features/Title.md) |
| 곡 선택·검색·필터·정렬·즐겨찾기 폴더·배경·기록 영역·난이도 정보 | [SongSelect](Features/SongSelect.md) |
| 새 난이도·osu! 가져오기 화면 | [EditorSongSelect](Features/EditorSongSelect.md) |
| osu! 가져오기: 변환기 경계·Client 검사·Songs 배치 | [OsuImport](Features/OsuImport.md) |
| 에디터 조작·메뉴·선택·편집 이력·PCM/FFT | [ChartEditor](Features/ChartEditor.md) |
| 목록 캐시·비동기 세션/자원 준비·포맷 변경 시 무효화 | [ResourceLoading](Features/ResourceLoading.md) |
| 표시 이름·번역·글꼴·옵션·키 설정·Option.ini·공용 부품 | [TextManagement](Features/TextManagement.md) |
| 스킨·Songs 곡 폴더 배치·내장 자산·판정/진행 막대 그림·배포 | [Assets](Features/Assets.md) |
| Canvas·PNG·입력·Z-order | [Visual2DGuide](Features/Visual2DGuide.md) |
| 노트·GOOD 우선 자동 포커스·정확도·POOR·Buzz 틱 | [Gameplay/Rules](Features/Gameplay/Rules.md) |
| 노트·Lane·키빔·판정 표시·진행바·콤보·점수·누적 판정 | [Gameplay/Presentation](Features/Gameplay/Presentation.md) |
| 플레이 옵션 패널·HP gauge 선택·배속·스크롤·Random·로컬 기록·기록 필터 | [Gameplay/PlayOptions](Features/Gameplay/PlayOptions.md) |
| Clip·사운드 정책·공통 PCM·DSP 예약 | [Gameplay/Audio](Features/Gameplay/Audio.md) |
| ESC 메뉴·재개·되감기·Retry·Retire·실행 상태 | [Gameplay/Pause](Features/Gameplay/Pause.md) |
| 정확도 랭크·통계·그래프·입력 로그·Retry | [Gameplay/Results](Features/Gameplay/Results.md) |
| Debug 타이머 표시·정지·탐색·전체 노트/Buzz 재현 | [Gameplay/Debugging](Features/Gameplay/Debugging.md) |

### 형식

| 작업 | 문서 |
| --- | --- |
| 형식 진입점·파서 위치·캐시 무효화 규칙 | [Formats](Formats/README.md) |
| 곡 정보·로마자 표기 | [YMM](Formats/Ymm.md) |
| 패턴 metadata·노트·연타 수·기본 HP 게이지 | [YMP](Formats/Ymp.md) |
| 히트사운드·볼륨·스크롤·보간·싱코페이션·Highlight | [YME](Formats/Yme.md) |
| 분수 위치·BPM·마디 길이·delay | [Timing](Formats/Timing.md) |

## 문서를 고칠 때

- 설계 결정은 선택과 이유를, 기능 문서는 상세 계약을 소유하며 함께 일치시킵니다.
- 문서를 줄이거나 옮겨도 확정 조건·예외·근거·검증 연결은 보존합니다.
- 완료 내역·과거 진단·세션별 상태는 Git 이력으로 확인하고, 지금 상태는 [Status](Status.md)에만 적습니다.
- 미구현 제안이나 Penpot 시안을 새 구현 지시로 해석하지 않습니다.
- 링크는 `python Scripts/Verify/check_links.py`로 확인합니다.
