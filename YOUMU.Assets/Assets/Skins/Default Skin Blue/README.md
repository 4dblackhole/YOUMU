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

### `Logo/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Background.png` | 1920×1080 | `Logo.Background` | 타이틀 화면 전체 하늘(그라데이션 + 오른쪽 위 햇살). 구름 · 깃털 없음 |

### `Logo/Menu/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Button.png` | 463×78 | `Logo.StartButton`, `Logo.EditorButton`, `Logo.OptionsButton`, `Logo.ExitButton` | 타이틀 메뉴 버튼 바탕(−14° 사선). Selected = 고른 메뉴(코발트 + 결) |
| `Button.Hovered.png` | 463×78 |  | 〃 |
| `Button.Pressed.png` | 463×78 | `Logo.StartButton.Pressed`, `Logo.EditorButton.Pressed`, `Logo.OptionsButton.Pressed`, `Logo.ExitButton.Pressed` | 〃 |
| `Button.Selected.png` | 463×78 | `Logo.StartButton.Hovered`, `Logo.EditorButton.Hovered`, `Logo.OptionsButton.Hovered`, `Logo.ExitButton.Hovered` | 〃 |
| `SelectionCursor.png` | 42×42 | `Logo.SelectionCursor` | 고른 메뉴 안 왼쪽의 금빛 깃털 커서 |

### `Logo/Player/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `ArtFrame.png` | 255×255 |  | 음악 플레이어 앨범 아트 틀(흰 테두리 + 아래 오른쪽 그림자 블록). 아트는 그 곡 패턴의 BG |
| `PlayButton.png` | 102×66 |  | 음악 플레이어 재생/일시정지 버튼 바탕(코발트 평행사변형). 아이콘은 Common/Icons/play · pause |
| `PlayButton.Hovered.png` | 102×66 |  | 〃 |
| `PlayButton.Pressed.png` | 102×66 |  | 〃 |
| `SeekFill.png` | 528×6 |  | 재생 위치 채움(지난 만큼 왼쪽부터 잘라 보임) |
| `SeekKnob.png` | 6×21 |  | 재생 위치 표시(남색 세로 막대, 채움의 오른쪽 끝) |
| `SeekTrack.png` | 528×27 |  | 재생 위치 막대 바탕 |

### `Logo/Sky/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Feather.png` | 18×51 |  | 하늘에 떠다니는 깃털 하나(타이틀 7개, 로딩 3개) |

## MusicSelect — 곡 선택 (게임 · 에디터)

### `MusicSelect/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `AddDifficulty.png` | 48×48 | `MusicSelect.AddDifficulty` | 에디터 선곡 카드의 + 버튼, 아이콘 포함. Highlight = 패턴 없는 곡(금빛 밑줄) |
| `AddDifficulty.Highlight.png` | 48×48 |  | 〃 |
| `AddDifficulty.Hovered.png` | 48×48 | `MusicSelect.AddDifficulty.Hovered` | 〃 |
| `Back.png` | 203×81 | `MusicSelect.Back` | Back: 왼쪽 아래 모서리 남색 블록. Focused = 키보드 포커스 |
| `Back.Focused.png` | 203×81 |  | 〃 |
| `Back.Hovered.png` | 203×81 | `MusicSelect.Back.Hovered` | 〃 |
| `Back.Pressed.png` | 203×81 | `MusicSelect.Back.Pressed` | 〃 |
| `BottomBar.png` | 1920×96 |  | 아래 띠(Back · 모드 · Play options · 옵션 · GO 뒤) |
| `Category.Hovered.png` | 131×51 |  | 폴더 탭 밑줄(고른 탭 · 마우스 올림). 탭 바탕은 없음 |
| `Category.Selected.png` | 34×51 |  | 〃 |
| `Difficulty.png` | 432×48 | `MusicSelect.Difficulty` | 펼친 카드 안 난이도 행 |
| `Difficulty.Hovered.png` | 432×48 | `MusicSelect.Difficulty.Hovered` | 〃 |
| `DifficultySelected.png` | 432×48 | `MusicSelect.DifficultySelected` | 고른 난이도 행(흰 면) |
| `Go.png` | 402×96 | `MusicSelect.Go` | GO: 오른쪽 아래에서 띠 위로 솟은 코발트 블록 |
| `Go.Disabled.png` | 402×96 | `MusicSelect.Go.Disabled` | 〃 |
| `Go.Focused.png` | 402×96 |  | 〃 |
| `Go.Hovered.png` | 402×99 | `MusicSelect.Go.Hovered` | 〃 |
| `Go.Pressed.png` | 402×99 | `MusicSelect.Go.Pressed` | 〃 |
| `GoError.png` | 480×57 |  | GO 위 오류 말풍선 바탕 |
| `ModeSelector.png` | 203×81 |  | 게임 모드 선택 칩 바탕 |
| `ModeSelector.Hovered.png` | 203×81 |  | 〃 |
| `ModeSelector.Pressed.png` | 203×81 |  | 〃 |
| `OptionSelect.png` | 96×81 | `MusicSelect.OptionSelect` | 옵션(톱니) 버튼, 아이콘 포함 |
| `OptionSelect.Hovered.png` | 96×81 | `MusicSelect.OptionSelect.Hovered` | 〃 |
| `OptionSelect.Pressed.png` | 96×81 | `MusicSelect.OptionSelect.Pressed` | 〃 |
| `ScrollbarHandle.png` | 18×96 | `MusicSelect.ScrollbarHandle` | 스크롤바 막대(보이는 폭 6 → 9px, 양옆 투명, 세로로 늘림) |
| `ScrollbarHandle.Hovered.png` | 18×96 | `MusicSelect.ScrollbarHandle.Hovered` | 〃 |
| `ScrollbarHandle.Pressed.png` | 18×96 | `MusicSelect.ScrollbarHandle.Pressed` | 〃 |
| `ScrollbarTrack.png` | 18×96 | `MusicSelect.ScrollbarTrack` | 스크롤바 레일(잡는 폭 12 → 18px, 세로로 늘림) |
| `SearchField.png` | 504×60 | `MusicSelect.SearchField` | 검색창(밑줄 입력, 돋보기 아이콘 포함). Focused = 코발트 밑줄 |
| `SearchField.Focused.png` | 504×60 |  | 〃 |
| `SongCard.png` | 519×87 | `MusicSelect.SongCard` | 곡 카드(오른쪽 끝까지 이어진 흰 막대) |
| `SongCard.Hovered.png` | 519×87 | `MusicSelect.SongCard.Hovered` | 〃 |
| `SongCardFocused.png` | 549×258 | `MusicSelect.SongCardFocused` | 선택한 곡 카드(왼쪽으로 튀어나온 코발트 + 금빛 왼쪽 줄). 난이도 수만큼 세로로 늘어남 |
| `SongCardPlaying.png` | 20×18 |  | 선택한 곡 제목 앞 재생 중 막대 3개 |
| `SortSelector.png` | 372×48 | `MusicSelect.SortSelector` | 정렬 드롭다운 상자(화살표 포함). Open = 펼친 상태 |
| `SortSelector.Focused.png` | 372×48 |  | 〃 |
| `SortSelector.Hovered.png` | 372×48 | `MusicSelect.SortSelector.Hovered` | 〃 |
| `SortSelector.Open.png` | 372×48 |  | 〃 |

### `MusicSelect/Detail/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `DifficultyBlock.png` | 196×48 |  | 곡 정보의 난이도 블록(코발트 평행사변형) |
| `Jacket.png` | 309×309 |  | 곡 정보 재킷 틀(그림 없음). 그림은 패턴 BG를 정사각으로 잘라 안에 |
| `JacketEmpty.png` | 309×309 |  | BG가 없는 곡의 재킷(곡 색 + 흐린 날개) |
| `LevelBadge.png` | 60×33 |  | 난이도 값 배지(새벽 금빛) |

### `MusicSelect/Panels/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Left.png` | 660×1080 | `MusicSelect.RecordPanel` | 왼쪽 기록 열 뒤 흰 막(화면 왼쪽 끝 → 오른쪽으로 옅어짐) |
| `Right.png` | 750×1080 | `MusicSelect.SongBrowser` | 오른쪽 곡 목록 뒤 흰 막(화면 오른쪽 끝 → 왼쪽으로 옅어짐) |
| `Transparent.png` | 1×1 | `MusicSelect.SongInformation` | 투명 1×1(가운데 곡 정보 패널을 숨김) |

### `MusicSelect/Records/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Best.png` | 432×84 |  | BEST 칸(왼쪽 코발트 줄) |
| `Row.png` | 432×78 |  | 기록 한 줄(아래 구분선) |
| `Status.Clear.png` | 62×30 |  | 기록 상태 칩 바탕: Clear · Failed · Practice · Retired |
| `Status.Failed.png` | 62×30 |  | 〃 |
| `Status.Practice.png` | 62×30 |  | 〃 |
| `Status.Retired.png` | 62×30 |  | 〃 |

## Loading — 로딩

### `Loading/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `BarFill.png` | 630×9 |  | 로딩 막대 채움 |
| `BarTrack.png` | 630×9 |  | 로딩 막대 바탕 |
| `Card.png` | 1920×240 |  | 로딩 가로 띠 바탕 |

## InGame — 플레이 · 일시정지 · 리플레이

### `InGame/Gear/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Band.png` | 1920×240 |  | 기어 바탕 띠(코발트 + 결, 위아래 흰 선). 화면 가로 전체 |

### `InGame/Hud/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `HitCountBadge.png` | 141×90 |  | 연타 남은 타격 수 배지(남색 블록 + 금빛 표지). 숫자는 게임이 그림 |
| `JudgeCountsPanel.png` | 744×114 |  | 오른쪽 아래 판정 수 판 |
| `ScorePanel.png` | 351×84 |  | 오른쪽 위 정확도 · 점수 판(왼쪽 사선) |
| `TimingBar.png` | 630×18 |  | 타이밍 막대(판정 구간 색, 가운데 0선) |
| `TimingMarker.png` | 9×39 |  | 타이밍 표시(마지막 타격의 오차 자리) |
| `TimingPanel.png` | 735×72 |  | 기어 아래 타이밍 인디케이터 판(오른쪽 사선) |

### `InGame/Hud/HpGauge/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `ClearMarker.png` | 15×41 |  | 클리어선(남색 선 + 틀 위로 나온 역삼각). 게이지 종류의 클리어 기준 자리 |
| `FillClear.png` | 681×27 |  | HP 채움 · 클리어 기준 이상(새벽 금빛). 잘라 보임 |
| `FillLow.png` | 681×27 |  | HP 채움 · 클리어 기준 전(코발트). HP만큼 왼쪽부터 잘라 보임, 늘이지 않음 |
| `Frame.png` | 735×36 |  | HP 게이지 틀(항상 같음). 왼쪽 48px = 자물쇠 받침, 흰 칸 681×27(10% 눈금) 시작 x = 48 |
| `Lock.png` | 45×45 |  | 자물쇠: 일시정지 메뉴나 Rewind를 쓴 뒤 그 시도의 HP가 꺼짐. 틀의 받침 위 |

### `InGame/Pause/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Backdrop.png` | 1920×1080 | `InGame.PauseBackdrop` | 일시정지 뒤 막(사선 결 남색 58%) |
| `PrepFill.png` | 746×9 |  | 준비 중 진행 띠의 움직이는 조각(코발트) |
| `PrepTrack.png` | 746×9 |  | 준비 중 진행 띠 바탕(PAUSED 아래) |
| `Row.png` | 799×78 | `InGame.PauseResume`, `InGame.PauseRewind`, `InGame.PauseRetry`, `InGame.PauseRetire` | 일시정지 메뉴 행. Selected = 고른 행(코발트 + 금빛 왼쪽 줄). 꺼진 행은 같은 바탕에 글자만 40% |
| `Row.Selected.png` | 799×78 | `InGame.PauseResume.Hovered`, `InGame.PauseRewind.Hovered`, `InGame.PauseRetry.Hovered`, `InGame.PauseRetire.Hovered` | 〃 |
| `Stepper.png` | 258×54 |  | Resume · Rewind 값 선택기(좌우 화살표 포함). Selected = 고른 행 안 |
| `Stepper.Selected.png` | 258×54 |  | 〃 |

### `InGame/Progress/`

| 파일 | 크기 | 키 | 부품 |
| --- | --- | --- | --- |
| `Background.png` | 1920×54 | `InGame.Progress.Background` | 맨 아래 진행 막대 띠 |
| `Fill.png` | 1670×9 | `InGame.Progress.Fill` | 진행 막대 채움(잘라 보임) |
| `Marker.png` | 21×21 | `InGame.Progress.Marker` | 진행 막대 현재 위치 표시 |
| `Track.png` | 1670×9 | `InGame.Progress.Track` | 진행 막대 레일 |

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
