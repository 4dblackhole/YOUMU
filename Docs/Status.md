# 진행 상황

이 문서는 "지금 어디까지 왔나"만 적습니다. 동작 계약은 [기능 문서](README.md), 결정의 이유는
[설계 결정](Design/Decisions.md), 일하는 방법은 [작업 방식](Contributing/Workflow.md)에 있습니다.
지휘 세션이 PR을 병합할 때마다 이 문서를 함께 고칩니다. 날짜별 완료 기록은 남기지 않고 Git 이력을 봅니다.

## 기준 커밋

| 저장소 | main | 비고 |
| --- | --- | --- |
| YOUMU | `c6f5c0b` (#86) | 게임 |
| PhantomCore | `b58b2ee` (#24) | YOUMU의 `Dependencies/PhantomCore` gitlink와 같음 |
| OsuTaikoConverter | `6d6d5ef` (#6) | YOUMU 솔루션이 형제 폴더로 참조 |

## 한눈에

- 태고형 리듬 게임 하나(Taiko 모드)와 차트 에디터가 동작합니다. 채보 형식은 YMM(음악)·YMP(패턴)·YME(효과)입니다.
- 화면은 [UI-02 리디자인](Design/Decisions.md#ui-02-ui-리디자인)의 모든 화면 묶음을 구현했습니다:
  타이틀(음악 플레이어), 곡 선택, 로딩, 플레이, 일시정지, 결과, 옵션, 플레이 옵션, 차트 에디터.
- osu!taiko `.osu` 가져오기는 [변환기](Features/OsuImport.md)가 한 파일을 YMM/YMP/YME로 바꾸고, Client가 실제 플레이 세션으로 검증해 Songs에 배치합니다.

## 구현된 것

| 영역 | 현재 계약 |
| --- | --- |
| 판정·정확도·노트 규칙(MAX~BAD, POOR, 롤·풍선·뎅뎅·Buzz 틱) | [Rules](Features/Gameplay/Rules.md) |
| 플레이 화면(기어·노트·판정 글자·타이밍 인디케이터와 평균 오차·HUD·Kiai) | [Presentation](Features/Gameplay/Presentation.md) |
| 일시정지·되감기·Retry·Retire, 재시작 키(옵션에서 변경)와 검은 페이드아웃 | [Pause](Features/Gameplay/Pause.md) |
| 플레이 옵션(스크롤·Accelerate·Mirror·Random·Metronome·JudgeLevel·HP 게이지)·로컬 기록 | [PlayOptions](Features/Gameplay/PlayOptions.md) |
| 결과 화면·통계·그래프·입력 기록·기록 DB | [Results](Features/Gameplay/Results.md) |
| 기본 HP 게이지(클리어 60%) | [YMP의 HP](Formats/Ymp.md#hp-게이지) |
| 곡 선택(검색·필터·정렬·즐겨찾기 폴더·미리듣기·F1 플레이 옵션) | [SongSelect](Features/SongSelect.md) |
| 차트 에디터(Compose·Audio·Metadata, 새 난이도, osu! 가져오기) | [ChartEditor](Features/ChartEditor.md), [EditorSongSelect](Features/EditorSongSelect.md) |
| 옵션·언어·글꼴·키 설정·키빔 불투명도 | [TextManagement](Features/TextManagement.md) |
| 스킨·내장 자산·배포 | [자산·스킨](Features/Assets.md) |

## 진행 중

- 열린 PR: 없음.
- 문서 재정리(사용자 승인, 4단계): ① 진행 상황·작업 방식 문서, 검증 도구(`Scripts/Verify`), 세 저장소 AGENTS 한국어화,
  엔진 AudioOutput 복원 ② 구조 그림(클래스·실행 흐름) ③ 폴더 재배치(`Architecture`·`Contributing`·`Design`·`Features`·
  `Formats`)와 통합, 남은 영어 문서 한국어화 ④ 저장소 밖 메모 정리. 네 단계를 모두 마쳤습니다(④는 저장소 밖 정리라 PR 없음).

## 미구현·보류

[PLAN-01](Design/Decisions.md#plan-01-제안과-미구현-범위)이 기준입니다. 아래 항목은 설계만 있거나 보류이며 작업 지시가 아닙니다.

- 리플레이 재생·파일 저장(메모리 입력 로그와 기록 DB는 구현).
- 기본 게이지 외의 추가 HP 게이지(플로팅 등): 미정.
- 난이도 값의 계산식: 미정(결과 화면 배지는 `—`).
- 예전 배치 Songs의 이전 도구, 곡 폴더를 옮기거나 이름을 바꿀 때 기록·즐겨찾기 연결 유지.
- 음원·채보 해시 검증(저장 위치 미정).
- [랭크 콘텐츠 제안](Design/Proposals/RankedContent.md)(공용 미디어·인증): 검토 제안.
- UI-02의 작은 남은 항목([UI 리디자인](Design/UI.md)의 "미구현" 표시): 기록 조회 실패 화면(지금은 오류 한 줄),
  검색 결과가 없을 때 기록·정보 칸 유지, 패턴이 없는 곡의 `+` 금빛 밑줄.
- 곡 선택의 작은 남은 항목([SongSelect](Features/SongSelect.md)): 키보드 포커스 그림, 옵션 서랍·즐겨찾기 패널의 새 배치.

## 알려진 문제·주의

- 검증 도구는 `Scripts/Verify`가 기준입니다. 저장소 밖의 옛 사본(`build/ui02`)은 모든 세션이 새 도구로 바꾼 뒤 지웁니다.
  그때까지는 새 도구가 옛 잠금 폴더(`build/ui02/locks`)를 함께 쓰므로 두 도구가 동시에 돌아도 잠금은 하나입니다
  ([작업 방식](Contributing/Workflow.md#검증과-공유-잠금)).

## 요청이 들어오는 곳

사용자 요청은 사용자 파일 `TODOLIST.txt`(git 무시)와 대화로 들어옵니다. 지휘 세션이 복잡도에 맞는 작업 세션에 배정합니다
([작업 방식](Contributing/Workflow.md)).
