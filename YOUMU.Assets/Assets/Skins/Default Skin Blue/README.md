# Default Skin Blue

YOUMU UI 시안(`rhythmgameUI/redesign`, 2026-11-12)의 부품을 투명 PNG로 뽑아 Default Skin과 같은 구조로 놓은 스킨입니다.
부품별 상태 그림과 쓰이는 곳은 시안의 부품 상태표(`rhythmgameUI/redesign/kit.html`)에서 같은 이름으로 볼 수 있습니다.

- 크기: 1080p 기준(논리 1280×720 × 1.5). 투명 배경. 글자는 굽지 않음(게임이 그림).
- 라벨이 있는 버튼은 바탕만, 라벨 옆 아이콘은 `Common/Icons`. 아이콘만 있는 버튼(옵션 톱니 · +)은 아이콘 포함.
- 상태: `이름.png` = 기본, `.Hovered` · `.Pressed` · `.Disabled`는 skin.ini의 `키.Hovered` 등과 같은 이름.
  `.Selected` · `.Focused` · `.Open` · `.On` · `.Waiting` 등은 게임에 아직 키가 없는 상태.
- `키` 열 = 이 파일을 쓰는 skin.ini 키. 비어 있으면 게임이 아직 읽지 않는 새 부품(UI 시안을 구현할 때 키를 정함).
- 여기 없는 그림 · 소리(노트 · 레인 · 판정 문구 · 입력 패널 · 히트사운드 등)는 내장 Default Skin을 씁니다.
- 게임 화면 배치는 아직 예전 디자인이라 연결한 키도 예전 자리 크기에 맞춰 늘어나 보일 수 있습니다.
- 다시 만들기: 시안 폴더에서 `python tools/skin_export.py` (시안 서버를 켠 상태, 경로 앞부분을 주면 그 부품만).
- 게임에서 고르려면 이 폴더를 외부 자산 루트의 `Skins`(개발 실행은 `Client/Assets/Skins`)에 복사합니다.

## Logo — 타이틀

타이틀 부품(하늘 배경 · 메뉴 막대 · 깃털 커서 · 떠도는 깃털 · 음악 플레이어)은 모두 내장 Default Skin의 `Logo`로 옮겼습니다.

## MusicSelect — 곡 선택 (게임 · 에디터)

곡 선택 부품은 내장 Default Skin의 `MusicSelect`로 옮겼습니다. 아래는 아직 읽는 키가 없어 이 폴더에 남긴 파일입니다.

| 파일 | 크기 | 부품 |
| --- | --- | --- |
| `AddDifficulty.Highlight.png` | 48×48 | 패턴 없는 곡의 +(금빛 밑줄) |
| `Back.Focused.png` · `Go.Focused.png` · `SearchField.Focused.png` · `SortSelector.Focused.png` |  | 키보드 포커스 상태 |
| `SortSelector.Open.png` | 372×48 | 펼친 정렬 드롭다운 상자 |
| `SongCardPlaying.png` | 20×18 | 선택한 곡 제목 앞 재생 중 막대 3개 |
| `Panels/Transparent.png` | 1×1 | 쓰지 않음(가운데 열은 이제 패널이 없음) |
| `Records/Best.png` | 432×84 | BEST 칸(왼쪽 코발트 줄) |
| `Records/Status.Failed.png` | 62×30 | 기록 상태 칩 바탕: Failed |

## Loading — 로딩

로딩 부품(가로 띠 · 막대 바탕 · 막대 채움)은 내장 Default Skin의 `Loading`으로 옮겼습니다.

## InGame — 플레이 · 일시정지 · 리플레이

### `InGame/Gear/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Band.png` | 1920×240 |  | 기어 바탕 띠(코발트 + 결, 위아래 흰 선). 화면 가로 전체 |

### `InGame/Hud/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `HitCountBadge.png` | 141×90 |  | 연타 남은 타격 수 배지(남색 블록 + 금빛 표지). 숫자는 게임이 그림 |
| `TimingBar.png` | 630×18 |  | 타이밍 막대(판정 구간 색, 가운데 0선) |
| `TimingMarker.png` | 9×39 |  | 타이밍 표시(마지막 타격의 오차 자리) |

### `InGame/Hud/HpGauge/`

HP 게이지 부품 다섯 개(틀 · 채움 두 종류 · 클리어선 · 자물쇠)는 내장 Default Skin의 `InGame/Hud/HpGauge`로 옮겼습니다.

### `InGame/Pause/`

일시정지 부품(뒤 막 · 행 · 값 선택기 · 준비 중 띠)은 내장 Default Skin의 `InGame/Pause`로 옮겼습니다.

### `InGame/Progress/`

진행 막대 부품 네 개와 HUD 판 세 개(정확도 · 판정 수 · 타이밍)는 내장 Default Skin의 `InGame`으로 옮겼습니다.

### `InGame/Replay/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Back.png` | 180×72 |  | 리플레이 진행 막대 왼쪽 Back 블록 |
| `Back.Hovered.png` | 180×72 |  | 〃 |
| `Button.png` | 66×60 |  | 재생/정지 · −5s · +5s 버튼 바탕 |
| `Button.Hovered.png` | 66×60 |  | 〃 |
| `RateBox.png` | 195×54 |  | 재생 속도 −/값/+ 상자 |
| `Tag.png` | 125×42 |  | 왼쪽 위 REPLAY 표시 바탕(글자는 게임이 그림) |

### `InGame/Replay/Marks/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Arrow.png` | 12×9 |  | Resume · Rewind 꺾쇠의 도착 쪽 화살촉. 꺾쇠 선은 코발트 2px로 게임이 그림 |
| `Pause.png` | 18×41 |  | 원래 플레이의 일시정지 자리 깃발(막대 위) |

## Result — 결과

결과 화면 부품(배너 · 아래 띠 · 버튼 · 양옆 막 · 하늘 · 모드 표시 · 랭크 글자와 원 틀)은 모두 내장 Default Skin의 `Result`로 옮겼습니다.

## Options — 옵션 · 키 설정

### `Options/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Drawer.png` | 648×1080 |  | 옵션 서랍 바탕(왼쪽) |
| `KeyCard.png` | 480×96 |  | Input 섹션의 키 설정 카드 |
| `KeyCard.Hovered.png` | 480×96 |  | 〃 |
| `Rail.png` | 96×1080 |  | 옵션 분류 막대(남색) |
| `RailButton.png` | 276×90 |  | 분류 버튼. On = 켜진 분류(금빛 왼쪽 줄). 아이콘은 Common/Icons |
| `RailButton.Hovered.png` | 276×90 |  | 〃 |
| `RailButton.On.png` | 276×90 |  | 〃 |

### `Options/Keys/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Glyph.Don.png` | 21×36 |  | 키 설정 열 머리의 키 그림(동 · 캇) |
| `Glyph.Kat.png` | 21×36 |  | 〃 |
| `Slot.Empty.png` | 133×51 |  | 키 설정 칸: Main(메인) · Sub(보조, Hovered = × 보임) · Empty(빈 칸) · Waiting(입력 대기) |
| `Slot.Main.png` | 133×72 |  | 〃 |
| `Slot.Sub.png` | 133×51 |  | 〃 |
| `Slot.Sub.Hovered.png` | 133×51 |  | 〃 |
| `Slot.Waiting.png` | 133×51 |  | 〃 |

## Common — 공용 부품

### `Common/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `FPS.png` | 247×41 |  | FPS 띠(F7) |

### `Common/Button/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Ghost.Hovered.png` | 188×96 |  | 글자 버튼 바탕(기본은 투명) |
| `Ghost.Pressed.png` | 188×95 |  | 〃 |
| `Icon.png` | 96×81 |  | 아이콘 버튼 바탕(아이콘은 Common/Icons) |
| `Icon.Hovered.png` | 96×81 |  | 〃 |
| `Icon.Pressed.png` | 96×81 |  | 〃 |
| `Secondary.png` | 241×81 |  | 보조 버튼(흰 바탕 + 코발트 테두리) |
| `Secondary.Disabled.png` | 241×81 |  | 〃 |
| `Secondary.Hovered.png` | 241×81 |  | 〃 |
| `Secondary.Pressed.png` | 241×81 |  | 〃 |
| `SecondarySmall.png` | 120×48 |  | 작은 보조 버튼(Filter 등) |
| `SecondarySmall.Hovered.png` | 120×48 |  | 〃 |
| `SecondarySmall.Pressed.png` | 120×48 |  | 〃 |

### `Common/Feedback/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Alert.Error.png` | 720×375 |  | 경고 대화상자 바탕: Error · Warning · Info(위 4px 띠 색) |
| `Alert.Info.png` | 720×375 |  | 〃 |
| `Alert.Warning.png` | 720×375 |  | 〃 |
| `ErrorLine.png` | 960×84 |  | 화면 오류 줄(흰 면 + 왼쪽 빨간 표지) |
| `Status.Error.png` | 480×72 |  | 상태 줄 바탕: Error · Warning · Info |
| `Status.Info.png` | 480×72 |  | 〃 |
| `Status.Warning.png` | 480×72 |  | 〃 |
| `Toast.png` | 68×57 |  | 토스트 바탕 |
| `Tooltip.png` | 205×72 |  | 툴팁(진한 남색 + 아래 화살표) |
| `TooltipKey.png` | 40×30 |  | 툴팁 안 단축키 칩 |

### `Common/Icons/`

아이콘 37개(흰색 36px)는 내장 Default Skin의 `Common/Icons`로 옮겼습니다. 키는 `[Common]`의 `Icons.<이름>`입니다.

### `Common/Input/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Chip.png` | 127×39 |  | 옵션 칩. Active = 사용 표시 칩 |
| `Chip.Active.png` | 137×33 |  | 〃 |
| `DropdownItem.Active.png` | 372×51 |  | 목록 항목: Active = 가리킴(옅은 코발트 + 왼쪽 선), Selected = 고름(체크 포함) |
| `DropdownItem.Selected.png` | 372×51 |  | 〃 |
| `DropdownList.png` | 372×216 |  | 펼친 목록 바탕(흰 면 + 위 코발트 선) |
| `Segment.png` | 91×42 |  | 묶음 버튼 바탕 |
| `SegmentItem.Selected.png` | 17×42 |  | 묶음 버튼의 고른 칸 |
| `SelectBox.png` | 480×60 |  | 드롭다운 상자(화살표 포함). Open = 펼침, Empty = 항목 없음 |
| `SelectBox.Empty.png` | 417×60 |  | 〃 |
| `SelectBox.Hovered.png` | 480×60 |  | 〃 |
| `SelectBox.Open.png` | 480×60 |  | 〃 |
| `Switch.png` | 60×36 |  | 스위치: 기본(꺼짐) · On · Hovered · Focused · Disabled |
| `Switch.Disabled.png` | 60×36 |  | 〃 |
| `Switch.Focused.png` | 60×36 |  | 〃 |
| `Switch.Hovered.png` | 60×36 |  | 〃 |
| `Switch.On.png` | 60×36 |  | 〃 |
| `TextField.png` | 346×60 |  | 입력칸(밑줄 입력). Focused = 코발트 밑줄 |
| `TextField.Disabled.png` | 346×60 |  | 〃 |
| `TextField.Focused.png` | 346×60 |  | 〃 |
| `TextField.Hovered.png` | 346×60 |  | 〃 |

### `Common/Popup/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Backdrop.png` | 1920×1080 |  | 팝업 뒤 막(사선 결 남색) |
| `Panel.png` | 840×783 |  | 팝업 바탕 |

## Editor — 에디터

### `Editor/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `BottomBar.png` | 1920×96 |  | 에디터 아래 띠 |
| `ChannelHead.png` | 276×60 |  | 채널 머리(Music · Timing · …) |
| `Inspector.png` | 522×786 |  | 오른쪽 편집 패널 바탕 |
| `LaneHead.png` | 276×228 |  | 레인 머리(Notes, 간격 −/+ 자리) |
| `MenuBar.png` | 1920×54 |  | 에디터 위 남색 메뉴 줄 |
| `MenuButton.png` | 87×54 |  | 메뉴 줄 버튼(File · Edit) |
| `MenuButton.Hovered.png` | 87×54 |  | 〃 |
| `ToolButton.png` | 72×78 |  | 도구 버튼. Selected = 고른 도구(금빛 밑줄). 노트 그림은 InGame 노트 |
| `ToolButton.Hovered.png` | 72×78 |  | 〃 |
| `ToolButton.Selected.png` | 72×78 |  | 〃 |
| `ToolStrip.png` | 516×78 |  | Compose 도구 줄 바탕 |

## NumberImage — 숫자

### `NumberImage/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Number-0.png` | 39×60 |  | 숫자 그림(Barlow Semi Condensed 기울인 800, 글자색 #16294A). 내장 숫자 그림을 같은 경로로 대신함 |
| `Number-1.png` | 28×60 |  | 〃 |
| `Number-2.png` | 39×60 |  | 〃 |
| `Number-3.png` | 38×60 |  | 〃 |
| `Number-4.png` | 42×60 |  | 〃 |
| `Number-5.png` | 38×60 |  | 〃 |
| `Number-6.png` | 38×60 |  | 〃 |
| `Number-7.png` | 36×60 |  | 〃 |
| `Number-8.png` | 38×60 |  | 〃 |
| `Number-9.png` | 38×60 |  | 〃 |
| `Number-Comma.png` | 23×60 |  | 〃 |
| `Number-Dot.png` | 24×60 |  | 〃 |
| `Number-Percent.png` | 55×60 |  | 〃 |
