# 첫 화면과 게임 표시 이름

`YOUMUGame`은 현재 Client의 게임 진입점이며, 첫 Scene은
`YOUMULogoScene`이다. 배경 clear color는 AliceBlue (`#F0F8FF`)다.
제품 표시 이름은 [GameIdentity.h](../Client/Texts/GameIdentity.h)의 `GameName`을 참조한다.
창/오류창/에디터 확인창의 이름 변경 방법과 내부 식별자와의 구분은
[문구 관리](TextManagement.md#게임-이름-변경)를 따른다.

화면은 [UI-02](DesignDecisions.md#ui-02-ui-리디자인) 설계([UI 리디자인](UIRedesign.md#타이틀))를 따른다.
하늘 배경·깃털·메뉴는 구현됐고, 메뉴 위 자리에 놓일 음악 플레이어와 그 키(←/→·Space)는 아직 없다.

## 화면과 배치

- `Visual2DCanvas`는 `1280×720`, `FixedHeight` 기준이다. 16:9부터 32:9까지는 논리 폭만 늘어나고 메뉴는 화면 가운데에 있다.
  16:9보다 좁은 화면은 16:9 배치 전체를 같은 비율로 줄이고 위아래에 `#04070D` 띠를 두며, 32:9보다 넓으면 좌우에 띠를 둔다.
- 배경은 스킨의 `[Logo] Background`(화면 전체 하늘)다. 그림이 없는 스킨은 단색 하늘색이다.
  그 위로 깃털(`Sky.Feather`) 일곱 개가 천천히 떨어지며 좌우로 흔들린다. 구름과 예전 YOUMU 로고 띠(세 장의 PNG)는 없다.
- 메뉴는 사선 막대 네 개(309×52, 간격 64, 위 344)다. 고른 항목은 오른쪽으로 12 밀리고 코발트 막대(`Menu.ButtonSelected`)
  안 왼쪽에 금빛 깃털 커서(`SelectionCursor`)가 들어온다. 나머지는 옅은 막대(`Menu.Button`과 `.Hovered`·`.Pressed`)다.
  글자는 Nunito 20(한글·일본어는 Noto Sans KR)이다.
- 문법·기본본 대체 규칙은 [skin.ini](BuiltInAssets.md#skinini와-씬별-이미지)를 따른다.

## 타이틀 메뉴 조작

- `Game Start`: YMM/YMP catalog를 표시하는 `MusicSelectScene`으로 이동한다.
- `Editor`: 같은 catalog 탐색을 사용하는 에디터 곡 선택 Scene으로 이동한다.
  기록 열만 없는 같은 화면이다.
- `Option`: 왼쪽 옵션 패널을 열고 닫는다.
- `Exit`: `SceneManager::Quit`을 요청해 게임을 정상 종료한다.
- 마우스 이동: hover한 버튼이 고른 항목이 되고 깃털 커서가 그 안으로 옮겨 간다.
- 마우스 왼쪽 클릭: 해당 버튼을 실행한다.
- `↑`/`↓` 또는 `W`/`S`: 선택 커서를 이동한다.
- `Enter` 또는 `Space`: 현재 선택한 버튼을 실행한다.
- `Ctrl+O`: 옵션 패널을 연다. 다시 누르면 0.2초 슬라이드로 닫힌다.
- `F7`: 모든 YOUMU Scene 위 우측 하단 성능 표시를 토글한다. FPS, UPS와 1초 동안의
  프레임당 평균·최대 draw 수(`Draws`)를 보여 준다.

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
- 로고 UI 수정: `YOUMULogoScene/Submodules/LogoView`, `LogoLayout`, `LogoInput`.
  Scene은 UI가 반환한 선택을 화면 전환으로 연결한다.
