# 디자인 참조

화면 디자인 작업에만 읽습니다. 디자인의 미구현 항목은 기능 추가 지시가 아닙니다.

- Penpot 파일: `3be9e5e1-190f-8090-8008-7468d9cc1bc8`.
- `Music Select · Sky`: 페이지 `3be9e5e1-190f-8090-8008-7468d9cc1bc9`.
- 곡 상세 JudgeLevel/BPM 범위/길이: 보드 `e6833909-d4a4-8067-8008-dc78ac88d495`.
- Taiko 키 설정: 보드 `e6833909-d4a4-8067-8008-dc77adeae44a`.
- `Gameplay · Sky`: 페이지 `618d0170-ff55-8025-8008-80646ceaa9d9`.
- 기본 HP 게이지 시안: 플레이 화면 배치 보드 `e6833909-d4a4-8067-8008-dcc40a6d3d00`,
  60% 기준선과 세 상태를 보여 주는 참고 보드 `e6833909-d4a4-8067-8008-dcc40b03ac47`.
  시안이며 게임 내 HP 게이지는 아직 구현하지 않았습니다. 규칙은 [YMP](Formats/Ymp.md#hp-게이지-설계-미구현)를 참고합니다.
- 같은 페이지의 일시정지 시안: 보드 `b0134a97-87eb-801a-8008-cca341851088`
  (`인게임 일시정지 · Pause Controls · 1920×1080`). 720×336px 중앙 메뉴,
  네 액션 행과 Resume/Rewind 값 선택기를 사용합니다. [일시정지 UI](Gameplay/Pause.md#일시정지-화면).
- `Result · Sky`: 페이지 `3ef1606b-c70d-804e-8008-c87342f13d4e`,
  보드 `3ef1606b-c70d-804e-8008-c87343546e9d` (리절트 화면 · Result · 1920×1080).
  짙은 상단 정보 배너, 중앙 랭크·제작자, 오른쪽 528px 그래프/버튼 열.
  표시·리트라이·미구현 데이터의 계약은 [플레이 결과](Gameplay/Results.md)에 있습니다.
- `Special Note Focus · PNG Sources · Export Ready`: BalloonProcessing,
  BalloonBurst, DengDengProcessing 투명 PNG와 200ms 처리 성공 상태.
- 에디터: `Editor UI`, 페이지 `85e98a9d-d3bd-80e7-8008-a3ca1370f7ea`의
  Pattern / Realtime View / Time Signature / Metadata / Effects.
  편집 탭 시안:
  Time Signature `7e23fd69-9559-80e8-8008-aa6c679a0a94`,
  Metadata `7e23fd69-9559-80e8-8008-aa6c70ebd483`,
  Effects `7e23fd69-9559-80e8-8008-aa6c72475143`.
  시안 갱신이며 이 세 탭의 게임 내 배치 적용은 별도 작업입니다.

실제 데이터만 사용하고 요청에 없는 기능을 시안에서 추론하여 추가하지 않습니다.
