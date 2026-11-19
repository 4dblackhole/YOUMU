# 첫 화면과 게임 표시 이름

`YOUMUGame`은 현재 Client의 게임 진입점이며, 첫 Scene은
`YOUMULogoScene`이다. 배경 clear color는 AliceBlue (`#F0F8FF`)다.
제품 표시 이름은 [GameIdentity.h](../../Client/Texts/GameIdentity.h)의 `GameName`을 참조한다.
창/오류창/에디터 확인창의 이름 변경 방법과 내부 식별자와의 구분은
[문구 관리](TextManagement.md#게임-이름-변경)를 따른다.

화면은 [UI-02](../Design/Decisions.md#ui-02-ui-리디자인) 설계([UI 리디자인](../Design/UI.md#타이틀))를 따른다.
하늘 배경·깃털·메뉴와 메뉴 위 음악 플레이어가 구현돼 있다. 주파수 시각화와 음량 조절은 두지 않는다(음량은 옵션 패널의 Music volume이 타이틀 플레이어에도 적용된다).

## 화면과 배치

- `Visual2DCanvas`는 `1280×720`, `FixedHeight` 기준이다. 16:9부터 32:9까지는 논리 폭만 늘어나고 메뉴는 화면 가운데에 있다.
  16:9보다 좁은 화면은 16:9 배치 전체를 같은 비율로 줄이고 위아래에 `#04070D` 띠를 두며, 32:9보다 넓으면 좌우에 띠를 둔다.
- 배경은 스킨의 `[Logo] Background`(화면 전체 하늘)다. 그림이 없는 스킨은 단색 하늘색이다.
  그 위로 깃털(`Sky.Feather`) 일곱 개가 천천히 떨어지며 좌우로 흔들린다. 구름과 예전 YOUMU 로고 띠(세 장의 PNG)는 없다.
- 메뉴는 사선 막대 네 개(309×52, 간격 64, 위 344)다. 고른 항목은 오른쪽으로 12 밀리고 코발트 막대(`Menu.ButtonSelected`)
  안 왼쪽에 금빛 깃털 커서(`SelectionCursor`)가 들어온다. 나머지는 옅은 막대(`Menu.Button`과 `.Hovered`·`.Pressed`)다.
  글자는 Nunito 20(한글·일본어는 Noto Sans KR)이다.
- 문법·기본본 대체 규칙은 [skin.ini](Assets.md#skinini와-씬별-이미지)를 따른다.

## 음악 플레이어

메뉴 위(논리 320,112 크기 640×160, 넓은 화면에서는 메뉴처럼 가운데)에 놓인다.

- 앨범 아트(160×160): 그 곡에서 선택된(없으면 첫) 패턴의 BG를 정사각형으로 가운데 잘라 채운다. BG가 없거나 읽을 수 없으면
  빈 상자다. 틀은 `[Logo] Player.ArtFrame`(흰 테두리 + 오른쪽·아래 그림자 10)이다.
- 음표 아이콘 뒤 아티스트(Nunito 15/700), 곡명(Nunito 30/900). 둘 다 대표(첫) 이름 한 줄이다.
- 지난 시간 · 진행 막대(352×18, `Player.SeekTrack`) · 길이(Barlow 15/700, `m:ss`). 채움(`Player.SeekFill`)은 지난 만큼
  왼쪽부터 잘라 보이고 오른쪽 끝에 남색 막대(`Player.SeekKnob`)가 있다. 길이는 음원이 열린 뒤 표시하며 그 전에는 `-:--`다.
  막대를 누르면 끌기가 시작돼 표시만 따라가고, 놓은 자리로 이동한다(막대 밖으로 끌어도 계속 따라간다). 길이를 모르면 이동하지 않는다.
- 버튼은 재생/일시정지(`Player.PlayButton`과 `.Hovered`·`.Pressed` 위 `Common Icons.play`/`pause`) · 정지 · 다음 곡(`Icons.next`)
  세 개다. 정지는 멈추고 처음으로 되돌린다(일시정지는 위치를 유지). 스킨에 정지 아이콘이 없어 작은 사각형을 그린다.
- 재생 목록은 게임 곡 선택의 현재 표시 목록(검색·필터·정렬·폴더 반영)이고 양 끝에서 반대쪽으로 이어진다.
  게임을 켜고 타이틀에 처음 곡이 생기면 그 목록에서 임의의 곡을 골라 바로 재생한다. 곡이 끝나면 다음 곡으로 넘어간다.
  음원을 열지 못한 곡은 멈춘 채로 두고 다음 곡으로 넘기지 않는다(재생 버튼이나 다음 곡으로 다시 시도).
- 표시 목록이 비면 아트는 빈 상자, 글자는 모두 비우고 버튼·막대·플레이어 키를 끈다. 다른 곡으로 대신 채우지 않는다.
- 플레이어는 게임 곡 선택과 같은 곡 목록·선택 상태·미리듣기를 쓴다([곡 선택](SongSelect.md#미리듣기와-타이틀-플레이어)).
  타이틀의 현재 곡이 곧 곡 선택의 선택 곡이다.

## 타이틀 메뉴 조작

- `Game Start`: YMM/YMP catalog를 표시하는 `MusicSelectScene`으로 이동한다.
- `Editor`: 같은 catalog 탐색을 사용하는 에디터 곡 선택 Scene으로 이동한다.
  기록 열만 없는 같은 화면이다.
- `Option`: 왼쪽 옵션 패널을 열고 닫는다.
- `Exit`: `SceneManager::Quit`을 요청해 게임을 정상 종료한다.
- 마우스 이동: hover한 버튼이 고른 항목이 되고 깃털 커서가 그 안으로 옮겨 간다.
- 마우스 왼쪽 클릭: 해당 버튼을 실행한다.
- `↑`/`↓` 또는 `W`/`S`: 선택 커서를 이동한다.
- `Enter`: 현재 선택한 버튼을 실행한다. `Space`는 메뉴를 실행하지 않는다.
- 플레이어 버튼에 포인터를 올리면 이름과 단축키 툴팁이 나온다(재생/일시정지 `Space`, 다음 곡 `→`).
- `Space`: 재생/일시정지. `→`: 다음 곡. `←`: 3초 넘게 들었으면 그 곡을 처음부터, 아니면 이전 곡(버튼은 없다).
  곡이 없거나 옵션 패널이 열려 있으면 플레이어 키는 아무 일도 하지 않는다.
- `Ctrl+O`: 옵션 패널을 연다. 다시 누르면 0.2초 슬라이드로 닫힌다.
- `F7`: 모든 YOUMU Scene 위 성능 띠를 토글한다. 화면 오른쪽 아래 모서리에 붙은 남색 띠(왼쪽만 사선)에
  `FPS 144 | UPS 1000`처럼 옅은 이름과 기울인 숫자로 보인다. 첫 측정 전 숫자는 `—`다. Debug 빌드는 같은 띠에
  1초 동안의 프레임당 평균·최대 draw 수(`Draws 87 peak 120`)를 덧붙인다(사용자 결정, Release에는 없음).

옵션 패널은 16:9 논리 폭의 1/4인 320 단위, 화면 전체 높이 720 단위다.
휠이나 가운데 버튼 드래그, 빈 영역 왼쪽 버튼 드래그로 스크롤한다.
옵션 항목과 문구·글꼴 소유권은
[문구·언어·글꼴 관리](TextManagement.md)에 설명한다.

커서 기본 자산은 `YOUMU.Assets/Assets/Skins/Default Skin/Logo/Menu/SelectionCursor.png`이며
투명 PNG다.
마우스가 정지된 상태에서는 키보드로 옮긴 선택을 다시 빼앗지 않도록, 새 mouse
event가 발생했을 때만 pointer hover를 선택 상태에 반영한다.

## 소스

- 실제 게임: `Client/App/YOUMUGame.*`, `Client/GameScene/YOUMULogoScene/`,
  `Client/GameScene/MusicSelectScene/`, `Client/GameFlow/YOUMUSceneIds.h`
- 로고 UI 수정: `YOUMULogoScene/Submodules/LogoView`, `LogoLayout`, `LogoInput`, 플레이어 표시·조작은 `TitlePlayerView`.
  View는 메뉴 선택과 플레이어 의도(`TitlePlayerRequest`)를 돌려주고 Scene이 화면 전환과 `SongLibrary` 호출로 연결한다.
- 공유 곡 목록·재생: `Client/GameFlow/SongLibrary.*`, 재생 상태는 `MusicSelectScene/Submodules/MusicPlayerState.*`(엔진 없이 검사),
  오디오는 `SongPreviewController.*`.
