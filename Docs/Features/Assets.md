# 내장·외부 자산과 배포

`YOUMU.Assets`는 배포에 필요한 기본 자산을 한 팩으로 만들고 RCDATA 101로
`Y's Orchestral Unison - Melodic Ultimatum.exe`에 링크하는 전용 프로젝트입니다. 런타임 로더는 기존 PNG,
글꼴, FMOD 파일 로더를 바꾸지 않고 실제 파일 경로를 제공합니다.

## 포함 범위

- `Assets/Skins/Default Skin` 전체
- `Assets/Fonts`의 글꼴과 각각의 OFL 라이선스: Noto Sans KR, Nunito, Barlow Semi Condensed(Medium·Bold·ExtraBold·
  BoldItalic·ExtraBoldItalic). Nunito와 Barlow Semi Condensed는 UI 리디자인 글꼴이며
  [Google Fonts 저장소](https://github.com/google/fonts/tree/main/ofl)의 파일을 수정 없이 넣었습니다.

`Default Skin`의 그림은 확정 리디자인(UI-02)의 부품입니다. 세부는 [스킨 부품](../Design/UI.md#스킨-부품)입니다.

AngelDream Handshaking MP3 1개·YMM 1개·YMP/YME 5쌍과 이펙트 테스트 WAV 2개는
EXE에 넣지 않습니다. 추가 WAV는 기본 스킨의 Don/Kat 원본을 재사용한 테스트 파일입니다.
저장소의 Songs 폴더는 `Client/Assets/Songs` 하나이며, 이 곡은 그 안의 `Takahashi yoko - Angel Dream Handshaking`에서
Git이 직접 추적합니다. 빌드는 곡을 복사하지 않고 `YOUMU.Assets`에는 곡이 없습니다.
`.gitignore`가 `Client/Assets/Songs`의 다른 곡 폴더를 모두 제외하므로 사용자가 추가한 곡은
추적되지 않습니다. 내장 곡 폴더 안에서 고친 파일과 새로 만든 차트 세트는 Git 변경으로 보이므로
커밋할 것만 고릅니다. 곡은 개발·배포 모두 외부 파일입니다.

## 외부 자산 루트

`UserAssetRoot`는 EXE 위치 기준 `../../../Client/Assets`가 디렉터리로 존재하면
그 경로를 선택하고, 없으면 EXE 옆 `Asset`을 선택합니다. 선택은 프로세스마다
한 번 수행합니다. 작업 폴더나 Debug/Release 구성에 의존하지 않으며 개발자의
절대 경로를 EXE에 기록하지 않습니다.

- 저장소의 `bin/x64/Debug` 또는 `bin/x64/Release`에서 실행: `Client/Assets`
- 다른 폴더로 EXE를 복사해 실행: `<EXE 폴더>/Asset`
- 곡 기본값: `<자산 루트>/Songs`, 옵션 패널/Option.ini의 General/SongsPath로 변경 가능.
  빈 값은 기존 자동 기본 경로이고, 상대경로는 EXE 폴더 기준입니다. 사용자 스킨은 `<자산 루트>/Skins`입니다.
- 편집 가능한 UTF-8 UI 문구: `<자산 루트>/Languages/{ko,en}.ini`
- 외부 프로그램: `<자산 루트>/Submodules/OsuTaikoConverter.exe`(저장소에서는 `Client/Assets/Submodules`, git 무시.
  빌드가 마지막으로 빌드한 구성의 변환기를 복사합니다)
- `Option.ini`: 자산 루트와 별개로 항상 EXE 옆
- 게임이 쓰는 사용자 데이터: EXE 옆 `Data/`(기록 DB는 `Data/Result/plays.sqlite3`, [PlayOptions](Gameplay/PlayOptions.md); 개별 오프셋은 `Data/LocalOffsets.txt`, [Audio](Gameplay/Audio.md#개별-오프셋)). 배포 묶음에 넣지 않고 실행 중 만들어집니다.

개발용 폴더의 존재 여부로 구분하므로 배포 폴더는 저장소의 빌드 위치와 분리합니다.
내장 글꼴과 누락 스킨의 기본본은 기존 RCDATA 캐시에서 읽습니다.

Release 배포 폴더에는 `Y's Orchestral Unison - Melodic Ultimatum.exe`와 같은 위치의 `fmod.dll`이 필요합니다.
기본 곡을 제공할 때는 개발용 Songs에서 제공할 곡만 골라 `Asset/Songs`에 넣습니다. 로고·기본 스킨·
글꼴만 필요하면 Songs 없이도 시작할 수 있으나 곡 목록은 비어 있습니다.
한국어 문구/외부 편집 기능을 제공하려면 원본 `Client/Texts/Languages`의 INI를
`Asset/Languages`에 함께 제공합니다. 언어파일 없이도 내장 영어 기본 문구로 실행됩니다.
영어 기본본은 Client 빌드의 생성 헤더로 포함하며 RCDATA 자산 팩에는 넣지 않습니다.
자세한 키/이스케이프/영어 fallback 규칙은 [TextManagement](TextManagement.md#언어파일-교체)를 참고합니다.
`Client/Assets/Unused`의 예제 PNG는 어떤 구성에서도 복사하거나 로드하지 않습니다.
기존 빌드 출력에 남아 있는 파일까지 빌드가 삭제하지는 않으므로 배포 파일은
깨끗한 폴더에 필요한 파일만 복사해서 만듭니다.

다른 곡과 사용자 스킨은 팩에 자동 포함하지 않습니다. 빌드 스크립트의 명시적인
source group만 포함되므로 사용자 데이터가 우연히 EXE에 들어가지 않습니다.

## Release 배포 절차

Visual Studio x64 개발자 PowerShell에서 FMOD SDK 경로를 설정하고 빌드합니다.

```powershell
$env:FMOD_ROOT = 'C:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows'
msbuild YOUMU.sln /m /p:Configuration=Release /p:Platform=x64
```

게임 실행에 필요한 직접 배포 파일은 bin/x64/Release의 Y's Orchestral Unison - Melodic Ultimatum.exe와 fmod.dll입니다.
PhantomCore 등 정적 라이브러리는 이미 EXE에 링크되므로 .lib/.pdb/테스트 EXE/fmodL.dll은 제외합니다.
에디터의 Import osu! folder 기능을 제공하려면 같은 빌드의 OsuTaikoConverter.exe도 `Asset/Submodules/`에
동봉합니다(게임 EXE 옆이 아닙니다). 기본 플레이 실행에는 필요하지 않지만 가져오기 버튼은 이 파일을 실행합니다.
빌드는 변환기를 `bin/x64/<구성>/Asset/Submodules/`에도 복사하므로 그 `Asset` 폴더째 배포 폴더에 넣으면 됩니다.

곡 없는 기본본의 배치는 다음과 같습니다. 사용자 파일 폴더는 비워 둬도 됩니다.

```text
demo/
  Y's Orchestral Unison - Melodic Ultimatum.exe
  fmod.dll
  Option.ini                 처음 실행 시 기본값으로 자동 생성 가능
  README.txt
  Asset/                     Assets가 아닌 단수 Asset
    Languages/en.ini
    Languages/ko.ini
    Submodules/OsuTaikoConverter.exe   에디터의 Import osu! folder에 필요
    Songs/<곡 폴더>/Music/     YMM 하나와 음원
    Songs/<곡 폴더>/Chart/     차트 세트 폴더별 YMP·YME 쌍
    Songs/<곡 폴더>/BG/        그 곡의 YMP가 참조하는 배경 이미지
    Skins/                   사용자 스킨셋
    Collections/             컬렉션(곡 선택 즐겨찾기) 파일. 게임이 만들며 유저끼리 주고받을 수 있음
  Licenses/                  배포 폰트 고지
```

기존 배포본을 갱신할 때 이번처럼 변환기도 바뀌었다면 두 EXE를 함께 교체합니다.
FMOD SDK/런타임 버전이 같으면 기존 fmod.dll을 유지할 수 있으며, 버전이 바뀌면
새 Release 빌드의 DLL도 함께 갱신합니다. Option.ini, 사용자 Songs/Skins/Collections와 편집한 언어파일은
보존합니다. 새 번역을 배포할 때는 원본 언어파일의 추가 키를 사용자 수정본과 병합합니다.

언어파일은 Client/Texts/Languages의 원본 두 개를 복사합니다. 개발 PC의 Option.ini는 장치명/키 등
개인 설정일 수 있으므로 그대로 배포하지 말고 새 기본 설정을 사용합니다. 기본 스킨/글꼴의 외부
복사본은 필요하지 않습니다. 배포할 곡이 생기면 [곡 폴더](#곡-폴더-배치)째 Asset/Songs에 추가합니다.
음악·이미지의 배포/사용 권한은 해당 계약으로 확인합니다.

Release 빌드는 C/C++ 런타임을 정적으로 링크(`/MT`)하므로 게임 EXE와 `OsuTaikoConverter.exe`는
`VCRUNTIME140.dll`·`MSVCP140.dll` 없이 실행되고 Visual C++ 재배포 패키지가 필요 없습니다.
`Directory.Build.props`가 Release 프로젝트 전체에 `/MT`를 지정하며 Debug는 `/MDd`입니다. `cl`을 직접
부르는 `Tests/Presentation/Run*.ps1`도 Release에서 `/MT`를 씁니다. 프로젝트가 섞이면 링커가 LNK2038을
보고합니다. Release EXE의 의존성은 `dumpbin /dependents`로 확인합니다(`fmod.dll`과 Windows 시스템 DLL만
남아야 합니다). 이 때문에 `Prerequisites/VC_redist.x64.exe`는 동봉하지 않아도 됩니다.

FMOD DLL을 동봉할 수 있는 조건과 크레딧/로고는 [FMOD 라이선스](https://www.fmod.com/licensing)와
[표기 조건](https://www.fmod.com/attribution)을 확인합니다. 파일 구성과 실행 성공이 공개 배포에 필요한
라이선스/표기 조건 충족을 대신하지 않습니다.

배포 위치에서 --smoke-test와 --smoke-lobby로 기본 시작/빈 목록을 확인합니다.
--smoke-editor는 실제 외부 채보가 필요한 검증 경로이므로 곡 없는 기본본에서는 사용하지 않습니다.

## 빌드와 실행 흐름

1. `YOUMU.Assets/Scripts/GenerateAssetPack.ps1`이 상대 경로, 크기, offset과 원본 bytes를
   결정적인 순서로 `BuiltInAssets.fdpak`에 기록합니다. 기존 캐시와 같은 FNV hash를
   빌드 시 계산해 작은 RCDATA 102로 함께 넣으므로 실행 때 팩 전체를 hash하지 않습니다.
2. 생성된 RC를 컴파일한 `YOUMU.Assets.res`를 `YOUMU.Client`가 링크합니다.
3. `wWinMain`이 엔진 초기화 전에 RCDATA를 검사합니다.
4. 팩은 `%LOCALAPPDATA%/YOUMU/BuiltInAssets/<pack hash>`에 임시 디렉터리를
   거쳐 원자적으로 설치됩니다. 같은 hash의 파일 크기와 완료 marker가 유효하면
   기존 캐시를 재사용합니다.
5. 설치 실패나 리소스 누락은 시작 오류로 처리해 불완전한 기본 자산으로 게임을
   계속 실행하지 않습니다.

`InitializeBuiltInAssets`는 실패 시 false를 반환합니다. 오류 메시지 저장 중에도
메모리 할당이 실패하면 메시지를 비우고 false를 유지하며 예외를 밖으로 내보내지 않습니다.
자원 없는 별도 테스트 실행 파일로 이 실패 경계를 검사하므로 사용자 캐시는 건드리지 않습니다.

## skin.ini와 씬별 이미지

스킨 루트에 UTF-8 `skin.ini`를 두고 씬별 폴더를 같은 레벨에 둡니다.
전체 이미지 키와 기본 경로는
[`Default Skin/skin.ini`](../../YOUMU.Assets/Assets/Skins/Default%20Skin/skin.ini)가 원본입니다.
사용자 스킨은 `<자산 루트>/Skins/<스킨 이름>`에 INI를 복사하고 필요한 PNG를 넣습니다.
기본 스킨과 INI는 EXE에 포함되므로 외부 파일이 없어도 실행됩니다.

```text
My Skin/
  skin.ini
  Logo/sky.png
       Menu/bar.png
  InGame/circle.png
  MusicSelect/background.png
  Result/sky.png
```

```ini
[Folders]
Logo=Logo
InGame=InGame
MusicSelect=MusicSelect
Result=Result

[Logo]
Background=sky.png
Menu.Button=Menu/bar.png
Menu.Button.Hovered=Menu/bar-hover.png

[InGame]
note=circle.png

[MusicSelect]
Background=background.png

[Result]
Background.Sky=sky.png
```

`Folders.Logo`는 **skin.ini 폴더 기준**, `Logo.Background`는 **지정된 Logo 폴더 기준**입니다.
씬 폴더도 `Images/Title`처럼 바꿀 수 있습니다. 절대/드라이브 경로와 `..`는 허용하지 않습니다.
`/`와 역슬래시를 모두 경로 구분자로 사용할 수 있습니다. 섹션/키는 대소문자를 구분합니다.
경로는 한글·공백을 지원하고 따옴표로 감쌀 수 있습니다. 역슬래시는 이스케이프하지 않습니다.
UTF-8 BOM과 CRLF/LF를 허용합니다. `;`/`#`로 시작하는 전체 줄만 주석이며,
파일명 안의 기호는 그대로 보존합니다. 중복 키는 첫 항목을 사용합니다.
잘못된 경로/항목은 진단 후 해당 요소의 기본본을 사용하고 잘못된 UTF-8은 파일 전체를 무시합니다.

1. 선택 스킨의 씬 폴더·파일 설정을 사용합니다. 빠진 설정은 내장 INI에서 가져옵니다.
2. 이 조합의 외부 파일이 있으면 사용합니다.
3. 파일이 없으면 **같은 요소의 내장 INI 경로와 PNG**로 대체합니다. 사용자 파일명을 내장 경로에 붙이지 않습니다.
4. 내장 INI도 비어 있는 선택 항목은 기존 색상 패널/버튼 디자인을 사용합니다.

| 섹션 | 이미지 항목 |
| --- | --- |
| `Logo` | `Background`(화면 전체 하늘), `Sky.Feather`, `Menu.Button`, `Menu.ButtonSelected`, `SelectionCursor`, 음악 플레이어의 `Player.ArtFrame`·`Player.PlayButton`·`Player.SeekTrack`·`Player.SeekFill`·`Player.SeekKnob` |
| `InGame` | 노트·오버레이·롱노트·틱·특수 노트, 레인·판정원·키/불빛, `Judgements.MAX`~`POOR`, `Judgements.Recovery.MAX`~`GOOD`, `Progress.Background/Track/Fill/Marker`, `Hud.HpGauge.Frame/FillLow/FillClear/ClearMarker/Lock`, `Hud.ScorePanel/JudgeCountsPanel/TimingPanel`, `PauseBackdrop`, `Pause.Row[.Selected]`, `Pause.Stepper[.Selected]`, `Pause.PrepTrack/PrepFill` |
| `MusicSelect` | `Background`, `Panels.Sheet/SheetEdge`, `Detail.Jacket/JacketEmpty/DifficultyBlock/LevelBadge`, `Records.Selector`, `Records.Row`, `Records.Band`, `Category.Selected/Hovered`, `SearchField`, `SortSelector`, `SongCard/SongCardFocused`, `Difficulty/DifficultySelected`, `AddDifficulty`, `BottomBar`, `Back`, `ModeSelector`, `PlayOptions`, `OptionSelect`, `Go`, `GoError` |
| `Result` | `Background.Sky`, `Panels.Left/Right`, `Banner`, `BottomBar`, `ModeBadge`, `Mode.Taiko`, `Rank.Ring`, `Rank.Y/X/SS/S/AA/A/B/C/D/E/F`, `Options.Tile`, `Options.Accelerate/Mirror/Random/JudgeLevel/ScrollSpeed/IgnoreScrollEffects/MaxStrokes`, `BackButton/RetryButton/ReplayButton` |
| `Loading` | `Card`(가로 띠), `BarTrack`, `BarFill` |
| `Options` | `Drawer`(서랍 바탕), `Rail`, `RailButton`·`RailButton.On`(켜진 분류), `KeyCard`, `Keys.Slot.Main/Sub/Empty/Waiting` |
| `Common` | `Icons.<이름>` 37개(흰색 36px 선 아이콘, 게임이 글자색을 곱해 씀) |

표의 `/`는 여러 키를 줄여 쓴 것입니다. 정확한 키는 내장 INI를 참조합니다.
버튼/패널 PNG는 기존 영역에 맞춰 표시하고 배치·입력 영역·런타임 텍스트는 유지합니다.
`키.Hovered`, `키.Pressed`, `키.Disabled`로 상태별 이미지를 지정할 수 있습니다.
생략한 상태는 기본 이미지를 사용하고 원래 RGB와 기존 불투명도를 적용합니다.
곡 선택·옵션 서랍의 부품과 타이틀 메뉴 막대(`BindSkinPart`)는 그림의 불투명도를 그대로 씁니다. 그림이 없을 때 쓰는 대체 색의 알파는 그림에 곱하지 않으며,
꺼진 상태 그림이 없는 부품만 꺼졌을 때 0.45로 흐리게 그립니다. `BindSkinImage`로 연결한 나머지 그림은 대체 색의 알파를 그림에 곱합니다.
기본 이미지가 없는 키는 상태 이미지만으로 활성화되지 않습니다. 언어별 문구는 PNG에 굽지 않는 것을 권장합니다.
부모 패널에 이미지를 지정하면 내부 단색 채움만 숨겨 PNG가 가려지지 않게 합니다.
타이틀 `Background`는 화면 전체에 표시하고 메뉴 네 항목은 같은 막대 그림을 씁니다(고른 항목은 `Menu.ButtonSelected`).

`SkinSetSelection`은 INI와 파일 경로를 선택 revision당 한 번 캐시합니다.
프레임마다 파일을 읽거나 탐색하지 않습니다. 살아 있는 로고/곡 선택의 버튼·패널은 노드가
`SkinVisualComponent`를 소유하며 스킨 변경 시 이미지/기본 디자인을 갱신합니다.
INI를 편집했다면 재실행하거나 다른 스킨을 선택했다 돌아오면 다시 읽습니다.
플레이·에디터·리절트는 화면 진입 시 같은 매핑을 사용합니다. 에디터 노트도 `InGame` 키를 공유합니다.

옵션의 목록과 `Option.ini`의 `General/SkinSet`은 계속 스킨 **폴더 이름**을 사용합니다.
`Default Skin`은 외부 폴더가 없어도 표시하며 사라진 선택 폴더는 기본본으로 대체합니다.
이미지 설정과 히트사운드의 기존 상대 경로는 독립적입니다.

풍선 완료 효과음은 `HitSounds/TaikoMode/pop.wav`입니다. 다른 스킨에서 이 파일을
제공하면 해당 파일을 사용하고, 없으면 내장 기본 스킨의 소리로 대체합니다.
판정 글자 7종도 `InGame/Judgements/MAX.png`부터 `POOR.png`까지 같은 파일별
fallback을 사용합니다. 인디케이터의 색 구간은 별도 요소이고, 감싸는 반투명 판만 `InGame/Hud/TimingPanel.png`입니다.
판정별 파일명과 표시 계약은 [Presentation](Gameplay/Presentation.md)에 있습니다.
하단 진행바는 `InGame/Progress/{Background,Track,Fill,Marker}.png` 네 파일을 사용합니다.
각 요소는 따로 교체할 수 있고 누락 파일만 내장본으로 대체합니다. 시간 문구는 PNG에
포함하지 않고 런타임에 표시합니다. 제작 크기와 시간 기준은 같은 Presentation 문서에 있습니다.

## Scene별 PNG

내장 원본은 `YOUMU.Assets/Assets/Skins/Default Skin` 아래 실제 폴더/VS 필터로 정리합니다.
플레이는 `InGame`, 로고는 `Logo`, 곡 선택은 `MusicSelect`, 리절트는 `Result`를 사용합니다.
각 폴더는 루트의 `skin.ini`와 같은 레벨입니다. 곡 선택의 부품은 UI 시안에서 1080p 기준(논리 × 1.5)으로 뽑은 PNG이며
`MusicSelect` 아래 `Detail`(재킷 틀·빈 재킷·난이도 블록·값 배지), `Panels`(곡 정보·곡 목록 아래의 면과 가장자리 그림자 띠),
`Records`(기록 선택 상자·기록 줄 판·상태 띠) 폴더로 나뉩니다. `Panels/Sheet.png`(1377×1080)는 논리 x 362부터 16:9 화면의 오른쪽 끝까지,
`Panels/SheetEdge.png`(54×1080)는 논리 x 352에 놓이고 `Records/Row.png`(504×78)는 화면 왼쪽 끝에서 시작하는 기록 줄 판입니다.
`Records/Selector.png`(372×48)는 정렬 선택 상자와 같은 모양에 바탕만 불투명한 스킨 바탕색입니다.
`Records/Band.png`(504×78)는 기록 줄 판과 같은 크기의 흰 그림으로, 판의 비스듬한 오른쪽 끝을 따라 폭 9의 띠만 그려져 있습니다.
게임이 기록 상태 색(초록·남색·빨강)으로 칠해 판 위에 겹칩니다.
`Background`와 `PlayOptions`는 내장 그림이 없어 단색 배경과 글자 버튼으로 그립니다.
옵션 서랍의 `Options` 폴더는 서랍 바탕(`Drawer.png` 648×1080), 분류 막대(`Rail.png` 96×1080, 펼치면 가로로 늘림),
분류 버튼(`RailButton[.Hovered/.On].png` 276×90, 펼친 폭 기준), Input 카드(`KeyCard[.Hovered].png` 480×96)와
키 설정 칸(`Keys/Slot.{Main,Sub,Empty,Waiting}.png`, `Slot.Sub.Hovered.png`)입니다. 아이콘·글자·키 그림은 런타임 요소입니다.
일시정지의 `InGame/Pause/Backdrop.png`는 사선 결이 있는 남색 58% 덮개(1920×1080)이며 화면 전체에 확장하여
Sprite의 알파 합성으로 덮습니다. 행(799×78)·값 선택기(258×54, 화살표 포함)·준비 중 띠(746×9)도 같은 폴더의 PNG입니다.

리절트의 부품은 UI 시안(`rhythmgameUI/redesign`)에서 1080p 기준(논리 × 1.5)으로 뽑은 PNG입니다.
텍스트·실제 통계·그래프·판정 비율 조각은 런타임 요소로 남겨 언어/값/해상도에 대응합니다.

| 리절트 상대 경로 | PNG 크기 | 용도 |
| --- | --- | --- |
| `Result/Background/Sky.png` | 1920×1080 | 곡 BG가 없을 때의 하늘 |
| `Result/Panels/{Left,Right}.png` | 690×1080, 768×1080 | 양옆 열 뒤 흰 막 |
| `Result/Banner.png`, `Result/BottomBar.png` | 1920×108, 1920×96 | 위 머리 띠와 아래 띠(가로로 늘림) |
| `Result/ModeBadge.png`, `Result/Mode/Taiko.png` | 139×45, 48×48 | 모드 배지 바탕과 흰색 드럼 아이콘 |
| `Result/{BackButton,ReplayButton,RetryButton}[.Hovered/.Pressed].png` | 203×96, 188×96, 360×126 | Back 모서리 블록, Replay 글자 버튼, 솟은 Retry |
| `Result/Rank/Ring.png` | 516×516 | 랭크 원 틀(남색 띠와 12시 금빛 눈금) |
| `Result/Rank/{Y,X,SS,S,AA,A,B,C,D,E,F}.png` | 각 708×354 | 정확도별 랭크(코발트 글자·흰 외곽), 투명 배경·원본 비율 유지 |
| `Result/Options/Tile.png` | 69×75 | 모드·옵션 타일(모서리 두 곳 깎은 흰 판, 게임이 남색·금빛·코발트로 칠함) |
| `Result/Options/{Accelerate,Mirror,Random,JudgeLevel,ScrollSpeed,IgnoreScrollEffects,MaxStrokes}.png` | 각 36×36 | 옵션 아이콘(흰 선: 배속 `>>`, 미러 화살표와 점선, 주사위, 과녁, 왼쪽 화살표와 줄, 물결에 사선, 흰 원과 오른쪽으로 옅어지는 잔상 호 둘) |

외부 스킨은 `skin.ini`의 상대 경로로 각각 교체합니다. 글꼴은 내장 `Assets/Fonts/NotoSansKR.ttf`이며,
원본 Google Fonts의 가변 굵기 파일과 `NotoSansKR-OFL.txt`(SIL OFL 1.1)를 함께 배포합니다.
원본 파일: https://github.com/google/fonts/tree/main/ofl/notosanskr . 글꼴 bytes는 수정하지 않았습니다.

곡 선택은 선택한 자산 루트의 `Songs`만 읽습니다. 곡을 캐시에서 다시 생성하지
않으며, YMP의 `Effect file`이 참조하는 YME도 사용자가 직접 편집할 수 있습니다.
히트사운드 표의 경로는 YME 폴더 기준입니다. 내장 글꼴은 캐시 경로에서
직접 읽으며 라이선스도 같은 팩에 보존됩니다.

### 판정 글자 그림

경로: `YOUMU.Assets/Assets/Skins/Default Skin/InGame/Judgements`.
각 PNG는 판정 글자 하나만 담은 RGBA이며 1080p 기준 크기입니다(높이 27~30px, 폭은 글자 길이를 따름).
예전의 1254×1254 링·문구 그림에서 글자 상자만 잘라 판정원 지름 96에 맞던 크기 그대로 줄였습니다.
링은 없앴습니다. 예전 그림은 링 중심이 그림 중심보다 18px(1.4%) 아래에 있어, 판정원 1.4배 크기로 그리면
화면에서 판정원보다 약 2.7 아래로 어긋났습니다.

| 파일 | 색상 |
| --- | --- |
| MAX.png | 흰색 |
| PERFECT.png | 밝은 청록색(cyan) |
| GREAT.png | 초록 |
| GOOD.png | 노랑 |
| BAD.png | 보라 |
| MISS.png | 회색 |
| POOR.png | 빨강 |
| Recovery.MAX.png ~ Recovery.GOOD.png | 같은 등급 글자의 면 아래쪽(높이의 35%부터)이 보라(#B24AFF)로 물듦, 테두리는 그대로 |

`Recovery.*`는 [BAD 복귀 판정](Gameplay/Rules.md#bad-복귀-판정)의 GOOD~MAX 글자입니다(스킨 키 `Judgements.Recovery.MAX`~`GOOD`).
`YOUMU.Assets/Scripts/MakeRecoveryJudgements.ps1`이 같은 폴더의 MAX~GOOD PNG에서 다시 만듭니다(`-Preview`는 확대 비교 그림).
사용자 스킨에 이 키가 없으면 다른 요소처럼 내장 INI의 경로와 PNG를 씁니다.

교체 시 정확한 대문자 문구, 짙은 남색 글자 윤곽과 투명 알파를 유지하고 글자 둘레에 여백을 두지 않습니다.
런타임은 1080p 기준 크기(픽셀 ÷ 1.5)로 그리고 글자 아래 끝을 판정원 위 끝보다 6 위에 둡니다.
표시 조건·크기·수명은 [Presentation](Gameplay/Presentation.md#판정-인디케이터와-판정-글자)이 기준입니다.

### 진행 막대 그림

경로: `YOUMU.Assets/Assets/Skins/Default Skin/InGame/Progress`.
네 요소는 투명 배경의 RGBA PNG이며 시간 문구는 런타임 텍스트로 표시합니다.

| 파일 | 크기 | 형태·색상 |
| --- | --- | --- |
| Background.png | 1920×54 | 화면 아래 끝 전체 폭의 옅은 띠 |
| Track.png | 1670×9 | 옅은 남색 레일 |
| Fill.png | 1670×9 | 코발트 채움(잘라 보임) |
| Marker.png | 21×21 | 흰 원형 마커 |

UI 시안에서 1080p 기준(논리 × 1.5)으로 뽑은 부품입니다.
교체 시 비율·투명 여백·요소 분리를 유지하고 문구·그림자·장식을 이미지에 합치지 않습니다.
색상은 제작 기준이며 모든 픽셀을 강제하는 값은 아닙니다.
배치·진행률·현재/전체 시간 계약은 [Presentation](Gameplay/Presentation.md#하단-플레이-진행바)을 따릅니다.

## 곡 폴더 배치

Songs의 바로 아래 폴더 하나가 곡 하나입니다([FMT-01](../Design/Decisions.md#fmt-01-파일-형식과-편집-원본)).
코드: `YOUMU.Chart/Catalog/SongLayout`, `SongCatalog`.

```text
Songs/
  <곡 폴더>/                           곡 폴더(osu! 가져오기는 음원 SHA-256 64자리)
    Music/
      <첫 아티스트> - <첫 곡명>.ymm    YMM은 하나만 둡니다
      <음원 파일>                      YMM의 File:은 YMM 폴더 기준
    Chart/
      <첫 제작자> - <난이도명>/        차트 세트 폴더
        <이름>.ymp, <이름>.yme         같은 이름에 확장자만 다른 한 쌍
        Sounds/...                     YME 히트사운드(YME 폴더 기준)
    BG/
      bg1.png                          YMP의 BG: bg1.png
```

- YMP는 자기 폴더에서 두 단계 위 곡 폴더의 `Music`에 있는 YMM 하나에 속합니다. YMP 안에는 YMM을 가리키는
  항목이 없고, 카탈로그는 Songs 전체를 검색해 연결하지 않습니다. 폴더 이름 `Music`·`Chart`·`BG`는 고정이며
  대소문자는 구분하지 않습니다.
- `Music`에 YMM이 없거나 둘 이상이면 그 곡은 오류와 함께 목록에서 빠지고 그 곡의 YMP도 읽지 않습니다.
- YMP는 `Chart/<차트 세트>/` 바로 아래에서만 읽습니다. `Chart` 바로 아래의 YMP와, `Music`·`Chart`가 모두 없는
  폴더(예전 `Songs/Music`·`Songs/Patterns`·`Songs/BG` 포함)는 경고만 남기고 건너뜁니다.
- 한 차트 세트 폴더에 YMP 여러 개를 둘 수 있습니다. 효과가 없는 난이도에도 빈 YME를 두고 YMP의
  `Effect file`에 같은 폴더의 YME 파일명을 적습니다.
- 곡 폴더·YMM·차트 세트 이름은 사람이 찾기 쉽게 하는 관례이며 프로그램은 뜻을 해석하지 않습니다. 에디터의
  새 난이도와 osu! 가져오기가 이 규칙으로 이름을 만듭니다. 제작자가 없으면 `Unknown`, 파일명에 쓸 수 없는 문자는
  `_`입니다. osu! 가져오기는 YMM·음원 이름에 [로마자 표시 규칙](../Formats/Ymm.md#로마자-줄과-표시)의 곡명 `Title`,
  YMP·YME 이름에 `Title [난이도명]`을 쓰고, 경로가 Windows 한계(폴더 247자, 파일 259자)를 넘을 이름은 줄여 끝에 `...`를 붙입니다
  ([이름 줄이기](../Design/AudioIdentity.md#이름-줄이기)).
- osu! 가져오기가 새로 만드는 곡 폴더의 이름은 음원 파일 바이트의 SHA-256(소문자 16진수 64자리)입니다(사용자 결정 2026-11-23,
  [음원 식별](../Design/AudioIdentity.md)). 제목·아티스트가 같고 음원이 다른 곡은 다른 곡 폴더가 됩니다. 전부터 있던 곡 폴더와
  손으로 만든 곡 폴더는 이름이 무엇이든 그대로 읽으며, 카탈로그는 폴더 이름이 해시인지 검사하지 않습니다.
- 곡·YMM·YMP·YME 파일에는 해시나 곡 ID를 적지 않습니다.

내장 AngelDream은 아래와 같습니다. 제작자가 없는 검증 채보는 `Unknown`을 씁니다.

```text
Songs/
  Takahashi yoko - Angel Dream Handshaking/
    Music/Takahashi yoko - Angel Dream Handshaking.ymm
          angel dream hand shaking.mp3
    Chart/
      Konpaku Sariel - Dream/angeldream.ymp, angeldream.yme
      Konpaku Sariel - measure test/angeldream [measure test].ymp, .yme
      Konpaku Sariel - test/angeldream [test].ymp, .yme
      Unknown - All Notes Verification/angeldream [all notes test].ymp, .yme
      Unknown - YME Effects Verification/angeldream [effects test].ymp, .yme
                                         Sounds/don.wav, Sounds/kat.wav
```

예전 배치(`Songs/Music`, `Songs/Patterns`, `Songs/BG`)의 곡은 읽지 않으며 자동으로 옮기지도 않습니다.
옮기는 도구는 없습니다. 기록·즐겨찾기는 YMP 절대 경로로 이어지므로 곡 폴더를 옮기거나 이름을 바꾸면
끊깁니다([PLAN-01](../Design/Decisions.md#plan-01-제안과-미구현-범위)). 에디터는 선택한 쌍을 같은 위치에 저장합니다.

## 자산 변경

기본 자산은 `YOUMU.Assets/Assets`에서 수정합니다. 새 배포 자산 종류를
추가하려면 프로젝트의 `BuiltInAsset` 목록과 `GenerateAssetPack.ps1`의 명시적
source group을 함께 갱신합니다. Scene에는 물리 경로를 새로 하드코딩하지 않고
`Client/App/AssetPaths.h` 또는 `YOUMUAssets.h`의 resolver를 사용합니다.

현재 캐시는 새 팩마다 새 hash 디렉터리를 만들며 이전 버전 디렉터리를 자동으로
삭제하지 않습니다. 사용자 곡과 스킨 선택 UI는 Client의 책임입니다.

기본 스킨의 사용되지 않는 `JudgeImage` PNG 7개와 resolver는 제거했습니다.
현재 판정 자산은 `InGame/Judgements`가 소유합니다. 사용자 외부 스킨은 삭제하지 않습니다.

## 에디터 메트로놈

Default Skin/Editor의 `MetronomeBeat.wav`와 `MetronomeAccent.wav`는 내장 클릭 샘플입니다.
48kHz 모노 PCM16, 50ms의 짧은 감쇠 정현파(일반 1400Hz / 마디 첫 박자 2200Hz)이며 외부
소스/라이선스 의존성이 없습니다. AssetPaths의 EditorSound로 선택 스킨의 같은 상대 경로를
찾고 없으면 내장 기본본을 사용합니다. 에디터의 재생 cue 전용이며 FFT 분석에는 넣지 않습니다.

Editor/SelectionTool.png는 선택 도구용 투명 PNG(128×128)입니다.
흰색 커서와 분홍색/흰색 지우개를 한 아이콘에 배치했으며 프로젝트에서 직접 제작했습니다.
