# 크래시 진단 기록

게임이 꺼지거나 멈췄을 때 무슨 일이 있었는지 나중에 알 수 있도록, 실행마다 로그 파일을 남깁니다.
기록은 그 PC에만 남고 어디로도 보내지 않습니다.

엔진 쪽 계약(받는 쪽 인터페이스, 그래픽 장치 끊김 보고서, 멈춤 감시, 덤프)은 엔진의
[엔진 진단 기록](../../Dependencies/PhantomCore/Docs/EngineDiagnostics.md)에 있습니다. 이 문서는 게임이 그것으로 무엇을 남기는지를 적습니다.

## 남는 파일

| 위치 | 내용 | 보관 |
| --- | --- | --- |
| `<게임 폴더>/Data/Logs/youmu-YYYYMMDD-HHMMSS.log` | 실행 한 번의 로그(UTF-8 텍스트) | 최근 10개. 시작할 때 오래된 것부터 지웁니다 |

게임 폴더는 실행 파일이 있는 폴더입니다. 폴더에 쓸 수 없으면 로그 없이 실행합니다(게임은 그대로 켜집니다).
문제가 생겼을 때는 이 폴더에서 이름의 시각이 가장 늦은 파일을 보면 됩니다. 게임을 다시 켜도 지난 실행의 파일은 남아 있습니다.

## 로그에 적히는 것

정상 실행은 수십 줄입니다. 한 줄은 `[시:분:초.밀리초] 내용`입니다.

```text
[09:21:32.933] YOUMU, built Nov 25 2026 09:14:18 (Release)
[09:21:32.933] windows 10.0.19045, 4 logical processors, 16296 MB memory
[09:21:32.933] start: scene YOUMU.Lobby, smoke test no, crash test none
[09:21:32.969] options: audio Automatic buffer 256, language ko, skin Default Skin, songs <game>\Asset\Songs, device removed data off
[09:21:33.484] engine: graphics adapter: NVIDIA GeForce GTX 1060 3GB, vendor 10DE device 1C02, 2965 MB dedicated, driver 32.0.15.6094
[09:21:33.484] engine: swap chain: 1280x720, 2 buffers, tearing allowed
[09:21:33.484] engine: device removed data: off
[09:21:33.484] engine: window: 1280x720 Windowed, display 360 Hz, render rate Display
[09:21:33.970] scene: YOUMU.Lobby
[09:21:34.116] exit code 0
```

| 줄 | 언제 | 뜻 |
| --- | --- | --- |
| `YOUMU, built …` | 시작 | 빌드한 날짜·시각과 구성 |
| `windows …` | 시작 | Windows 빌드 번호, 논리 프로세서 수, 메모리 |
| `start: …` | 시작 | 첫 화면, smoke 실행 여부, 일부러 일으키는 시험 여부 |
| `options: …` | 시작 | 오디오 출력·버퍼, 언어, 스킨, Songs 폴더, 장치 제거 추가 정보(DRED) |
| `Option.ini was not read: …`, `texts: …` | 시작 | 설정 파일을 읽지 못했거나 언어 파일에 맞지 않는 항목이 있을 때 |
| `engine: …` | 그래픽 장치를 만든 뒤, 그 뒤로는 값이 바뀔 때 | 그래픽 어댑터·드라이버, 스왑 체인, 창 크기·모드·주사율·렌더 속도 |
| `scene: …` | 화면이 바뀔 때 | 지금 화면의 Scene ID |
| `exit code N` | 끝 | `Run`이 돌려준 종료 코드. 0이 정상입니다 |

평소에는 이 줄들만 씁니다. 1초마다 디스크에 쓰는 것은 없습니다.

### 폴더 이름 가리기

로그는 사용자가 다른 사람에게 건넬 수 있으므로, 줄을 쓸 때 두 폴더 이름을 바꿔 적습니다.

- 게임 폴더 → `<game>`
- Windows 사용자 프로필 폴더(`C:\Users\<이름>`) → `%USERPROFILE%`

그래서 경로에 든 Windows 사용자 이름이 로그에 남지 않습니다. 대소문자까지 같은 경우만 바꿉니다.
그 밖의 위치(다른 드라이브의 Songs 폴더 등)는 그대로 적힙니다.

## 문제가 났을 때 적히는 것

게임은 1초 표본을 최근 120개(2분)까지 메모리에 들고 있다가, 아래 세 경우에만 로그에 쏟습니다.

```text
[09:21:38.015] graphics device lost: IDXGISwapChain4::Present failed with 0x887A0005, reason DXGI_ERROR_DEVICE_REMOVED (0x887A0005)
[09:21:38.016]   frame 601, back buffer 1280x720, 54 draw calls in the last finished frame
[09:21:38.016]   graphics memory 50 of 2520 MB, 30 MB in system memory
[09:21:38.016]   device removed data was off (Option.ini [Diagnostics] DeviceRemovedData=1 turns it on)
recent statistics, 1 seconds, oldest first:
      1s fps  315 ups   1787 draws  54/ 54 longest   69261 us vram   50/2520 MB memory   135 MB handles   463 YOUMU.Lobby
[09:21:38.117] fatal error: IDXGISwapChain4::Present failed with HRESULT 0x887A0005.
[09:21:38.119] exit code 1
```

| 경우 | 줄 | 함께 적히는 것 |
| --- | --- | --- |
| 그래픽 장치가 끊김 | `graphics device lost: …` | 실패한 호출과 결과, 제거 사유, 마지막 프레임 번호·화면 크기·draw 수, 그래픽 메모리, DRED 내용(켰을 때), 최근 통계 |
| 실행을 끝낸 예외 | `fatal error: …` | 예외 문구. 장치 끊김 뒤에 이어 오면 최근 통계를 다시 적지 않습니다 |
| 메인 루프가 10초 동안 한 바퀴도 못 돎 | `main loop stalled: …` | 멈춘 시간, 메인 스레드 ID, 그 전까지 돈 횟수, 최근 통계. 풀리면 `main loop resumed after … s` |

최근 통계 한 줄은 1초이고 오래된 것부터 적습니다.

| 칸 | 뜻 |
| --- | --- |
| `1s` | 엔진이 시작한 뒤 지난 시간(초) |
| `fps`, `ups` | 그 1초의 렌더 프레임 수와 Update 수 |
| `draws 54/ 54` | 프레임당 draw call의 평균 / 최대 |
| `longest … us` | 그 1초에서 가장 길었던 메인 루프 한 바퀴(마이크로초). 한 번의 끊김이 여기에 보입니다 |
| `vram 50/2520 MB` | 게임이 쓰는 그래픽 메모리 / Windows가 허용하는 예산 |
| `memory`, `handles` | 프로세스가 잡은 메모리와 커널 핸들 수. 계속 늘면 해제되지 않는 것이 있다는 뜻입니다 |
| 마지막 칸 | 그때의 Scene ID |

멈춤은 창을 끌거나 크기를 바꾸는 동안, 메뉴·대화 상자, 디버거 중단에는 알리지 않습니다(엔진이 구분합니다).
한도 10초는 곡 로딩 같은 정상 지연보다 넉넉히 길게 잡은 값입니다(`GameDiagnostics::MainLoopStallSeconds`).

덤프 파일(`Data/Crash`)과 오류 창의 파일 위치 안내는 아직 없습니다. 다음 변경에서 넣습니다.

## 장치 제거 추가 정보(DRED)

`Option.ini`의 `[Diagnostics] DeviceRemovedData=1`로 켭니다. 기본은 `0`이고 옵션 창에는 항목이 없습니다.
켜면 그래픽 장치가 끊겼을 때 명령 목록마다 GPU가 마지막으로 끝낸 동작이 `graphics device lost` 아래에 한 줄씩 붙습니다.
모든 그리기에 비용이 조금 붙으므로 그래픽 문제를 쫓을 때만 켭니다. 엔진은 시작할 때만 읽습니다.

## 일부러 일으켜 보기

기록이 제대로 남는지 확인하는 숨은 실행 인자입니다. 옵션 창이나 `Option.ini`로는 켤 수 없습니다.

| 인자 | 하는 일 |
| --- | --- |
| `--crash-test=device` | 600프레임을 그린 뒤 엔진이 그래픽 장치를 일부러 제거합니다. 게임은 장치 끊김으로 끝납니다(종료 코드 1) |
| `--crash-test=stall` | 시작 0.5초 뒤 Update 한 번이 3초 동안 돌아오지 않습니다. 이때만 멈춤 한도를 1초로 줄입니다 |
| `--log-dir=<폴더>` | 로그를 `Data/Logs` 대신 준 폴더에 씁니다(따옴표로 감싼 경로 가능). 시험이 사용자의 폴더를 건드리지 않게 합니다 |

`Tests/Presentation/RunCrashDiagnosticsTests.ps1`이 이 인자로 게임을 숨김 창으로 세 번 돌려 로그 내용을 확인합니다.

## 코드

| 파일(`Client/Diagnostics`) | 맡은 일 |
| --- | --- |
| `GameLog` | 로그 파일 하나. 프로세스에 하나이며 `wWinMain`의 `game_log::Session`이 소유합니다. 어느 코드든 `game_log::Write`로 한 줄을 씁니다 |
| `RecentStatistics` | 1초 표본 120개 고정 배열. 할당하지 않습니다 |
| `GameDiagnostics` | 엔진의 받는 쪽(`IEngineDiagnosticSink`) 구현. 표본을 받아 보관하고 위 세 경우를 로그에 씁니다 |
| `LaunchDiagnostics` | 숨은 실행 인자 읽기 |

- 한 줄은 추가 전용으로 연 파일에 `WriteFile` 한 번으로 씁니다. 그래서 스레드끼리 줄이 섞이지 않고 잠금이 없습니다.
- 멈춤은 엔진의 감시 스레드가 알립니다. 그 경로(`WritePlain`, `RecentStatistics::WriteToLog`)는 할당하지 않습니다. 멈춘 스레드가 힙을 쥐고 있을 수 있기 때문입니다.
- `wWinMain`이 로그 → `GameDiagnostics` → `YOUMUGame` 순서로 만들고 반대 순서로 없앱니다. `YOUMUGame`은 `GameDiagnostics`를 빌려 쓰기만 합니다.
- `SkinSetSelection`과 `RhythmTestScene`의 저장 실패 문구는 아직 디버그 출력으로만 나갑니다.
