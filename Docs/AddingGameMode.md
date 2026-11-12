# 게임 모드 추가

새 모드의 판정과 세션 구성은 `YOUMU.Modes`에, 화면·음원·번역 연결은 `Client`에 둡니다. Taiko는 현재 이 경로를 사용하는 모드입니다.

## 세션과 입력

1. `IPlayGameMode`의 `Id`, `InputLayout`, `LoadSession`, `CreateSession`을 구현합니다. `LoadSession`은 원본 차트와 효과 데이터를 읽고, `CreateSession`은 같은 모드 규칙으로 세션을 구성합니다. 기존 구현은 `TaikoMode`와 `TaikoSessionBuilder`에서 확인할 수 있습니다. [모드 계약](../YOUMU.Modes/Mode/PlayGameMode.h) · [Taiko 세션 구성](../YOUMU.Modes/Taiko/Submodules/TaikoSessionBuilder.cpp) `LoadSession`·`CreateSession`의 `std::stop_token`은 파서·컴파일 경로에 전달하고, 취소 예외를 읽기 오류 진단으로 바꾸지 않습니다. 완성된 세션의 런타임 상태에는 취소 토큰을 보관하지 않습니다. [로딩 계약](ResourceLoading.md)
2. `ScrollGear`에 레인과 노트를 구성하고 레인을 확정한 뒤 `SetInputLayout`을 호출합니다. 각 `ModeInputBinding`에 안정적인 바인딩 ID, 표시 이름, 행동, 물리 키, 0부터 시작하는 `laneIndex`를 지정합니다. 행동마다 레인은 하나이며 여러 행동을 한 레인에 연결할 수 있습니다. 세션은 존재하지 않는 레인이나 한 행동의 상충하는 레인 지정을 준비 단계에서 거부합니다. [입력 데이터](../YOUMU.Modes/Mode/ModeInputLayout.h) · [세션 구성과 라우팅](../YOUMU.Modes/Mode/Submodules/PlaySession.cpp)
3. `ResolveInputLayout`으로 저장된 키를 검증한 뒤 `SetInputMapping`으로 물리 키와 행동을 연결합니다. 키 정책은 레이아웃에 선언합니다. Taiko는 `allowUnassignedAuxiliaryKeys`로 빈 보조 슬롯을 허용하며 주 키는 필수입니다. 라이브 입력과 되감기는 동일한 레인으로 전달됩니다. [키 정책](../YOUMU.Modes/Mode/ModeInputLayout.cpp) · [되감기](../YOUMU.Modes/Mode/Submodules/PlaySessionSeeking.cpp)
4. 노트 규칙과 사운드 정책에서 `NoteEvent`와 `AudioCueRequest`를 생성합니다. 기본 타격 사운드 ID와 자유 입력 cue는 세션에 연결합니다. 타격 음원 파일과 엔진 장치 호출은 Client에서 관리합니다. [Taiko 사운드 정책](../YOUMU.Modes/Taiko/Submodules/TaikoSoundPolicy.cpp) · [Client 사운드 연결](../Client/GameFlow/Submodules/GameplayAudio.cpp)

## 등록과 표현

| 등록 위치 | 등록할 것 |
| --- | --- |
| [PlayGameModeFactory](../YOUMU.Modes/Mode/PlayGameModeFactory.cpp) | 모드 ID와 생성 함수. 같은 표로 지원 여부와 설정 화면용 입력 레이아웃을 조회합니다. |
| [ModePresentation](../Client/Presentation/ModePresentation.h) | 효과음 종류, 키 편집 UI 종류, 옵션에서 차지하는 높이, 바인딩 번역 키. 현재 Taiko의 실제 음원 등록은 GameplayAudio, 키 화면 구성은 [OptionsKeys](../Client/Presentation/OptionsPanel/Submodules/OptionsKeys.cpp)가 담당합니다. |
| [EditorModeFactory](../Client/EditorScene/Submodules/Modes/EditorModeFactory.cpp) | 에디터에서 사용할 `IEditorMode` 구현을 등록합니다. |
| [ResultViewFactory](../Client/GameScene/ResultScene/Submodules/ResultViewFactory.cpp) | 모드 결과를 표시할 화면을 등록합니다. [IPlayResult](../YOUMU.Modes/Mode/Results/IPlayResult.h)와 [IPlayResultCollector](../YOUMU.Modes/Mode/Results/IPlayResultCollector.h)를 모드의 결과 데이터와 집계 규칙에 맞춰 구현합니다. |

새 모드는 레인 배치와 노트 표시, 키 입력 표시, 결과 화면, 에디터 및 옵션 텍스트를 준비해야 합니다. 선곡 기본 모드와 새 패턴 생성, 디버그 샘플 차트에는 별도의 제품 정책이 있습니다. [선곡 기본값](../Client/GameScene/MusicSelectScene/Submodules/SongSelectionState.h) · [새 패턴 생성](../Client/GameScene/MusicSelectScene/Submodules/EditorPatternCreation.cpp) · [디버그 세션 구성](../Client/GameFlow/Submodules/GameplaySessionFactory.cpp)

## 현재 범위와 검증

다중 레인 화면과 위치 기반 판정 입력은 다른 게임 모드를 설계할 때 확장합니다.
[IScrollGearPresentation](../YOUMU.Rhythm/Scroll/ScrollGear.h)은 그때 함께 판단하도록
선언을 유지합니다. 현재 구현·호출은 없으며 새 모드의 필수 구현 계약으로 사용하지 않습니다.

- 입력 라우팅은 여러 레인을 지원합니다. 플레이 화면의 `GameplayLayout`은 Taiko용 단일 레인 루트를 배치합니다. 4키·7키 화면에는 레인별 배치·노트·키 표시 구현이 필요합니다. [플레이 화면](../Client/GameScene/RhythmTestScene/Submodules/GameplayLayout.cpp)
- 판정용 입력에는 시각, 행동, 물리 키와 눌림·뗌이 들어갑니다. osu!standard의 커서 위치와 osu!catch의 이동 위치에는 별도의 입력 계약이 필요합니다. [입력 이벤트](../YOUMU.Rhythm/Note/Submodules/NoteTypes.h)
- BMS 지원에는 파일 해석과 해당 채보를 플레이할 모드가 각각 필요합니다. 입력 레인 매핑은 플레이 세션을 구성하는 부분입니다.
- 모드 도메인 테스트는 차트 로드, 대상 레인, 동시 입력, Hold·release, 되감기와 결과를 검증합니다. 옵션·키 설정, 플레이 화면, 결과, 에디터는 관련 Presentation과 smoke로 확인합니다. [레인 회귀](../Tests/Rhythm/Submodules/LaneRoutingTests.cpp) · [검증 절차](Verification.md)
