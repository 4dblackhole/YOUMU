# YOUMU 설계 기준

이 문서는 새 설계를 제안하는 문서가 아니라, 이미 선택한 구조와 동작을 보존하기 위한
설계 기준입니다. 최초 대조 기준은 Client `6786b9c416389a03c3bc5785fe12034a755b99f4`,
고정 엔진 `216d5d44681dcc09e6b0f46aafd94113678bac4a`와 사용자 확정 대화입니다.
이후 실제 상태는 Git으로 확인합니다. 이 기준 작성 자체가 런타임 변경 승인은 아닙니다.

## 판단 순서와 상태

1. 사용자의 가장 최근 명시적 지시·정정이 우선합니다. 이미 지시한 변경을 다시 승인받지 않습니다.
2. [설계 결정](DesignDecisions.md)의 사용자 확정 사항과 프로젝트 계약을 확인합니다.
3. 기능 문서는 그 결정의 세부 문법·동작·소유권을 정의합니다. 충돌하면 자의적으로 고치지 않고
   근거를 대조하고, 해소되지 않는 부분만 사용자에게 확인합니다.
4. RPG는 플레이 규칙의 코드베이스입니다. 사용자 변경 사항과 현재 프로젝트의 구조를 함께
   적용합니다. RPG의 다른 구현을 발견했다는 이유로 확정된 YOUMU 규칙을 덮어쓰지 않습니다.
5. 현재 코드와 테스트는 구현 증거입니다. 불일치가 있다고 설계가 자동 변경되거나 기존
   테스트가 항상 옳아지는 것은 아닙니다. 시안·예시·검토 제안은 채택 결정과 구분합니다.

설계 결정의 **근거 종류**는 사용자 확정 / 프로젝트 계약 / 사용자 설계안 / 검토 제안입니다.
**구현 상태**는 구현 / 일부 구현 / 미구현(설계만)으로 별도 표시하고, 선택이 필요한
미정점도 함께 적습니다. 설계가 존재한다는 이유만으로 구현됐다고 보고하거나
미구현 설계를 새 작업 지시로 해석하지 않습니다.

## 시스템 경계

| 영역 | 소유하는 것 | 경계 |
| --- | --- | --- |
| PhantomCore | Win32·D3D12·Raw Input·오디오 장치·GPU/음성 자원·엔진 루프 | 재사용 엔진. 게임 규칙·곡·Scene ID는 Client 소유 |
| YOUMU.Rhythm | RhythmTimer·AccuracyRange·INote/규칙·Lane·ScrollGear | 엔진/UI에 독립적인 시간·판정·이벤트 값 |
| YOUMU.Chart | YMM/YMP/YME·타이밍 컴파일·카탈로그·편집 원본/저장 | 모드별 노트 의미와 화면/장치 구현을 역참조하지 않음 |
| YOUMU.Modes | IPlayGameMode·PlaySession·Taiko 구성/사운드/결과 정책 | 파일 데이터를 실제 모드 노트로 구성, 순수 세션 상태 소유 |
| YOUMU.Editor | 주입받은 PCM의 파형·FFT 분석 | 엔진 디코더·Scene·UI를 직접 소유하지 않음 |
| Client | App·Scene·Controller·Presenter·전역/플레이 옵션·기록·자산 연결 | 엔진은 PhantomCore.h/PhantomCore.vcxproj로 사용 |
| YOUMU.Assets | 기본 스킨·글꼴·라이선스·기본 제공곡 | 사용자 편집본·개인 곡·설정·기록과 분리 |
| OsuTaikoConverter | osu 입력·파일 변환·저장/경로 검사 | 공통 포맷/노트 계약만 사용. 실제 플레이 결과 검증은 Client |

구체 의존성·소스/필터 검사는 [ChangeImpactMap](ChangeImpactMap.md), 실행 소유권은
[ExecutionFlow](ExecutionFlow.md), 변환 경계는 [OsuTaikoConverter](OsuTaikoConverter.md)가 기준입니다.

## 실행과 소유권

- `YOUMUGame`/`SceneGameClient`는 Scene factory와 공통 서비스를 조립합니다.
  전환은 지연 `ChangeScene`이며 gameplay Scene은 `DestroyOnExit`입니다.
- 곡 선택은 경로·옵션을 값으로 복사하고 선택 패턴을 worker에서 검증합니다.
  성공한 PlaySession만 진입 store로 넘기며 이전 선택의 늦은 결과는 게시하지 않습니다.
- `RhythmTestScene`은 Controller·Presenter·Pause 메뉴를 소유하고 전환만 요청합니다.
  Controller는 시계·세션·오디오 연결, Presenter는 Canvas/노드·표시 상태를 담당합니다.
- 세션은 Loading → Gameplay → Pause/Ending 상태를 사용합니다. 준비 중에는
  시각·판정을 진행시키지 않으며 실제 Gameplay 전에는 플레이 Canvas를 노출하지 않습니다.
- 노트 처리 결과는 `NoteEvent`/`AudioCueRequest`/확정 정확도 값입니다.
  UI·음원·장치 호출을 노트 규칙에 넣지 않습니다. 결과는 값으로 넘기고 플레이 자원을 정리합니다.
- EditorWorkspace는 문서·모드·분석·재생 수명을 소유합니다. 모드는 제한된
  `IEditorContext`, 분석은 읽기 전용 `IEditorAudioSource`를 사용합니다.

## 설계 결정 찾아보기

| ID | 보존할 결정 | 세부 기준 |
| --- | --- | --- |
| ID-01 | PhantomCore/YOUMU 이름·EXE·이름 범위 | [제품](DesignDecisions.md#id-01-제품과-이름) |
| ARC-01 | 저장소·모듈·Scene·소유권 | [구조](DesignDecisions.md#arc-01-구조와-소유권) |
| TIME-01 | 단일 시계·Raw Input·DSP·판정 순서 | [시간](DesignDecisions.md#time-01-시간과-입력) |
| FMT-01 | 포맷 분리·원본 좌표·자동화·문법 변경 | [포맷](DesignDecisions.md#fmt-01-파일-형식과-편집-원본) |
| LOAD-01 | 비동기 목록/세션·자원 준비·캐시 무효화 | [로딩](DesignDecisions.md#load-01-로딩과-캐시) |
| CONV-01 | 변환기의 독립성·Client 검증·OD 대응 | [변환](DesignDecisions.md#conv-01-변환과-검증) |
| GAME-01 | RPG의 입력 없는 GOOD 우선 포커스 | [포커스](DesignDecisions.md#game-01-rpg-포커스) |
| GAME-02 | 판정 범위·논리 노트 정확도·정밀도 | [정확도](DesignDecisions.md#game-02-판정과-정확도) |
| GAME-03 | POOR 거부와 최종 0점 패널티 | [POOR](DesignDecisions.md#game-03-poor) |
| GAME-04 | Buzz 헤드/몸통 분리·색 유지·틱 확정 | [Buzz](DesignDecisions.md#game-04-buzz와-틱) |
| PLAY-01 | Scroll/Mirror/Random/Accelerate/JudgeLevel | [플레이 옵션](DesignDecisions.md#play-01-플레이-옵션) |
| PAUSE-01 | ESC 메뉴·재개/되감기/재시도·자격 | [Pause](DesignDecisions.md#pause-01-일시정지와-복원) |
| DEBUG-01 | Debug 전용 Space·1~4·시간 표시·복원 | [Debug](DesignDecisions.md#debug-01-디버그-조작) |
| EDIT-01 | 편집 원본·32단계 이력·저장 실패 보존 | [편집](DesignDecisions.md#edit-01-에디터) |
| EDIT-02 | 공통 탐색·원본 PCM·확대 파형/STFT | [분석](DesignDecisions.md#edit-02-오디오-뷰) |
| UI-01 | 선곡 정보·키 패널·메뉴·표시 계약 | [화면](DesignDecisions.md#ui-01-화면과-키-설정) |
| DATA-01 | 결과·영속 기록·조건식·사용자 데이터 | [데이터](DesignDecisions.md#data-01-결과와-기록) |
| HP-01 | 기본 게이지 수치/가중치, 문서만 유지 | [HP 수치](DesignDecisions.md#hp-01-기본-hp-설계) |
| HP-02 | 게이지 인터페이스/씬 소유 방향과 미정점 | [HP 구조](DesignDecisions.md#hp-02-hp-구조와-미정점) |
| PLAN-01 | 제안·미구현 기능을 확정 기능과 구분 | [미구현](DesignDecisions.md#plan-01-제안과-미구현-범위) |

## 변경 규칙

작업 시작 시 관련 ID, 유지할 동작, 요청이 명시적으로 바꾸는 항목을 식별합니다.
버그 수정·리팩터링·문서 압축·최적화 요청 자체는 다른 확정 설계를 바꾸는 허가가 아닙니다.

- 기존 계약을 만족하도록 고치는 작업은 그대로 진행합니다. 사용자에게 이미 확정한 설계를
  다시 선택하게 하거나 승인받은 작업에 중복 확인을 요구하지 않습니다.
- 명시적 요청으로 설계를 바꿀 때는 같은 ID의 결정·근거·영향 범위·기능 문서·회귀 사례를
  함께 갱신합니다. 승인 근거가 없고 중요한 해석이 갈리면 영향을 받는 부분만 멈추고 확인합니다.
- 코드/테스트가 결정과 다르면 먼저 차이를 기록합니다. 코드에 맞춰 결정을 바꾸거나,
  새 해석을 통과시키려고 기대값만 바꾸지 않습니다. 원래 요구를 재현하는 검사를 먼저 찾습니다.
- 문서 정리 때 ID, 상태, 조건, 예외, 수치/단위, 이유, 근거, 재현 사례와 검증 연결을 보존합니다.
  이동·중복 제거는 가능하지만 확정 설계를 날짜가 오래됐다는 이유로 삭제/약화하지 않습니다.
- 결정 문서는 무엇을/왜 선택했는지, 기능 문서는 상세 사용법·문법·실행 계약을 소유합니다.
  완료 로그·개인 PC 상태는 여기에 누적하지 않습니다.

현재 미정 항목은 [HP-02](DesignDecisions.md#hp-02-hp-구조와-미정점)와
[PLAN-01](DesignDecisions.md#plan-01-제안과-미구현-범위)에 명시합니다.
결정이 없다는 이유로 임의 기본값·파일 문법·새 기능을 확정하지 않습니다.
