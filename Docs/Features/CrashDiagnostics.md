# 크래시 진단 기록

게임이 꺼지거나 멈췄을 때 무슨 일이 있었는지 나중에 알 수 있도록, 실행마다 로그 파일을 남기고
죽거나 멈춘 순간에는 덤프 파일을 남깁니다.
기록은 그 PC에만 남고 어디로도 보내지 않습니다.

엔진 쪽 계약(받는 쪽 인터페이스, 그래픽 장치 끊김 보고서, 멈춤 감시, 덤프)은 엔진의
[엔진 진단 기록](../../Dependencies/PhantomCore/Docs/EngineDiagnostics.md)에 있습니다. 이 문서는 게임이 그것으로 무엇을 남기는지를 적습니다.

## 남는 파일

| 위치 | 내용 | 보관 |
| --- | --- | --- |
| `<게임 폴더>/Data/Logs/youmu-YYYYMMDD-HHMMSS.log` | 실행 한 번의 로그(UTF-8 텍스트) | 최근 10개. 시작할 때 오래된 것부터 지웁니다 |
| `<게임 폴더>/Data/Crash/youmu-YYYYMMDD-HHMMSS-<사유>.dmp` | 죽거나 멈춘 순간의 덤프(모든 스레드의 호출 스택) | 최근 5개 |
| `<게임 폴더>/Data/Crash/youmu-YYYYMMDD-HHMMSS-gpu.nv-gpudmp`와 같은 이름의 `.json` | 그래픽 장치가 끊겼을 때 드라이버가 남긴 [GPU 크래시 덤프](#gpu-크래시-덤프nvidia-aftermath). 그 기능이 든 빌드의 감시 모드에서만 | 지우지 않습니다(드물게 생깁니다) |

게임 폴더는 실행 파일이 있는 폴더입니다. 폴더에 쓸 수 없으면 로그나 덤프 없이 실행합니다(게임은 그대로 켜집니다).
문제가 생겼을 때는 두 폴더에서 이름의 시각이 가장 늦은 파일을 보면 됩니다. 게임을 다시 켜도 지난 실행의 파일은 남아 있습니다.
개발자에게 보낼 때는 로그 파일과, 있으면 같은 시각의 덤프 파일을 함께 보냅니다.

## 일반 실행과 감시 모드

실행 파일은 하나이고 띄우는 방법이 둘입니다. `Option.ini`는 어느 쪽에서도 바뀌지 않으며, 다음 실행은 띄운 방법만 따릅니다.

| 띄우는 방법 | 모드 | 남는 것 |
| --- | --- | --- |
| 실행 파일을 그대로 실행 | 일반 | 이 문서의 기본 기록: 로그, 덤프, 문제가 났을 때의 최근 2분 통계, 오류 창 |
| 게임 폴더의 `Run in watch mode.bat` | 감시 | 일반 기록에 더해 [장치 제거 추가 정보](#장치-제거-추가-정보dred)를 켜고, 1초 통계와 [메인 루프 밖 작업](#메인-루프-밖의-작업)의 시작·끝을 로그에 계속 적고, 메인 루프 멈춤을 10초가 아니라 3초에 알립니다 |

- 감시 모드는 실행 인자 `--diagnostics=watch`입니다. bat 파일은 같은 폴더의 실행 파일을 이 인자로 띄우는 한 줄입니다.
- 로그 첫머리의 `mode:` 줄이 어느 쪽인지 보여 줍니다(`mode: normal` 또는 `mode: watch (--diagnostics=watch): …`).
  감시 모드의 창 제목에는 게임 이름 뒤에 `[감시 모드]`가 붙습니다(언어 파일의 `Startup.watchTitle`).
- bat 파일 이름은 영문입니다. Windows 탐색기의 압축은 파일 이름을 그 PC의 언어 코드로 적어서, 한글 이름은 다른 언어의 PC에서 풀면 깨집니다.
- 감시 모드는 켜는 쪽으로만 합칩니다. `Option.ini`의 `DeviceRemovedData=1`은 일반 실행에서도 그대로 켜져 있고,
  감시 모드가 `Option.ini`에서 켠 것을 끄지는 않습니다. [그래픽 장치 시험 설정](#그래픽-장치-시험-설정)의 값은 건드리지 않습니다
  (화면 동작이 달라지면 일반 실행과 비교할 수 없습니다).
- 감시 모드의 멈춤 한도는 3초입니다(`GameDiagnostics::WatchedMainLoopStallSeconds`). 그래픽 드라이버가 멈추면 Windows가
  약 10초 뒤에 드라이버를 되살리므로, 10초 한도로는 그 사이에 모든 스레드의 덤프(`…-stall.dmp`)를 남기지 못합니다.
  창을 끌거나 대화 상자(폴더 선택 창 포함)가 떠 있는 동안에는 일반 실행과 같이 알리지 않습니다.
- 1초 통계 줄은 [최근 통계](#문제가-났을-때-적히는-것)와 같은 모양이며 나오는 대로 Windows에 넘깁니다.
  프로세스가 밖에서 끝나 아무 줄도 남기지 못한 실행에서도 마지막 1초까지의 상태가 남습니다.
  디스크에 닿을 때까지 기다리지는 않습니다(1초마다 게임 루프가 그만큼 서게 됩니다). 그래서 PC가 통째로 꺼지면 마지막 몇 초는 없을 수 있습니다.
- 통계 줄은 실행 한 번에 16MB(약 하루치)까지만 적고 그 뒤로는 그칩니다. 사건 줄은 계속 적습니다. 로그 파일 보관 개수는 일반 실행과 같습니다.
- 일반 실행은 1초마다 파일에 쓰지 않고, 작업의 시작·끝 줄도 적지 않습니다.

## 로그에 적히는 것

정상 실행은 수십 줄입니다. 한 줄은 `[시:분:초.밀리초] 내용`입니다.

```text
[09:21:32.933] YOUMU, built Nov 25 2026 09:14:18 (Release)
[09:21:32.933] windows 10.0.19045, 4 logical processors, 16296 MB memory
[09:21:32.933] start: scene YOUMU.Lobby, smoke test no, crash test none
[09:21:32.969] options: audio Automatic buffer 256, language ko, skin Default Skin, songs <game>\Asset\Songs, device removed data off
[09:21:32.969] graphics switches: validation 0, adapter Default, tearing 1, buffers 2, waitable 0, render rate override 0
[09:21:33.484] engine: graphics adapter: NVIDIA GeForce GTX 1060 3GB, vendor 10DE device 1C02, 2965 MB dedicated, driver 32.0.15.6094
[09:21:33.484] engine: swap chain: 1280x720, 2 buffers, tearing allowed
[09:21:33.484] engine: device removed data: off
[09:21:33.484] engine: graphics options: validation off, adapter default
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
| `graphics switches: …` | 시작 | `Option.ini`가 요청한 [그래픽 장치 시험 설정](#그래픽-장치-시험-설정). 이 빌드가 무시하면 `ignored by this build,`가 붙습니다 |
| `Option.ini was not read: …`, `texts: …` | 시작 | 설정 파일을 읽지 못했거나 언어 파일에 맞지 않는 항목이 있을 때 |
| `engine: …` | 그래픽 장치를 만든 뒤, 그 뒤로는 값이 바뀔 때 | 그래픽 어댑터·드라이버, 스왑 체인, 실제로 쓴 검증·어댑터 요청(`graphics options`), 창 크기·모드·주사율·렌더 속도. 검증을 켠 실행에서는 검증 메시지와 끝의 요약도 이 줄로 옵니다 |
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
      1s fps  315 ups   1787 draws  54/ 54 longest   69261 us vram   50/2520 MB memory   135 MB handles   463 text  3 pages    0 new  0 evicted YOUMU.Lobby
[09:21:38.117] fatal error: IDXGISwapChain4::Present failed with HRESULT 0x887A0005.
[09:21:38.119] exit code 1
```

| 경우 | 줄 | 함께 적히는 것 |
| --- | --- | --- |
| 그래픽 장치가 끊김 | `graphics device lost: …` | 실패한 호출과 결과, 제거 사유, 마지막 프레임 번호·화면 크기·draw 수, 그래픽 메모리, DRED 내용(켰을 때), 진행 중인 작업, 최근 통계 |
| 실행을 끝낸 예외 | `fatal error: …` | 예외 문구. 장치 끊김 뒤에 이어 오면 최근 통계를 다시 적지 않습니다 |
| 메인 루프가 10초(감시 모드는 3초) 동안 한 바퀴도 못 돎 | `main loop stalled: …` | 멈춘 시간, 메인 스레드 ID, 그 전까지 돈 횟수, 진행 중인 작업, 최근 통계, 덤프(`…-stall.dmp`). 풀리면 `main loop resumed after … s` |
| 게임이 죽음(잡히지 않은 예외) | `crash: …` | 사유, 예외 코드와 주소, 스레드 ID, 덤프 파일 이름, 최근 통계 |

최근 통계 한 줄은 1초이고 오래된 것부터 적습니다.

| 칸 | 뜻 |
| --- | --- |
| `1s` | 엔진이 시작한 뒤 지난 시간(초) |
| `fps`, `ups` | 그 1초의 렌더 프레임 수와 Update 수 |
| `draws 54/ 54` | 프레임당 draw call의 평균 / 최대 |
| `longest … us` | 그 1초에서 가장 길었던 메인 루프 한 바퀴(마이크로초). 한 번의 끊김이 여기에 보입니다 |
| `vram 50/2520 MB` | 게임이 쓰는 그래픽 메모리 / Windows가 허용하는 예산 |
| `memory`, `handles` | 프로세스가 잡은 메모리와 커널 핸들 수. 계속 늘면 해제되지 않는 것이 있다는 뜻입니다 |
| `text 3 pages 0 new 0 evicted` | 화면 글자 아틀라스의 장 수(한도 16), 그 1초에 새로 구운 글자 수, 다시 쓰려고 비운 장 수. 화면이 그대로인데 `new`·`evicted`가 계속 나오면 아틀라스가 모자라 글자를 되풀이해 굽는 것입니다 |
| 마지막 칸 | 그때의 Scene ID |

멈춤은 창을 끌거나 크기를 바꾸는 동안, 메뉴·대화 상자, 디버거 중단에는 알리지 않습니다(엔진이 구분합니다).
한도 10초는 곡 로딩 같은 정상 지연보다 넉넉히 길게 잡은 값입니다(`GameDiagnostics::MainLoopStallSeconds`). 감시 모드는 3초입니다.

## 메인 루프 밖의 작업

게임 루프가 아닌 스레드에서 도는 일입니다. 장치가 끊기거나 루프가 멈춘 순간에 무엇이 돌고 있었는지 로그만으로 알 수 있게 합니다.
표시는 엔진의 `BackgroundWork`로 하고(엔진 문서의 "메인 루프 밖의 작업 표시"), 로그에 적는 것은 `Client/Diagnostics/BackgroundWorkLog`입니다.

| 이름 | 하는 일 | 표시하는 곳 |
| --- | --- | --- |
| `read song list` | Songs 폴더를 읽어 곡 목록을 만듦 | SongCatalogLoader |
| `load chart` | 고른 채보를 읽고 플레이 세션을 만듦 | PlaySessionLoader |
| `read records` | 곡 선택의 기록 목록 조회(삭제 포함) | SongRecordView |
| `open record` | 기록 하나를 열어 결과 화면을 준비 | MusicSelectScene |
| `save record` | 결과 화면에서 기록 저장 | ResultScene |
| `convert osu! folder`, `place converted charts` | osu! 폴더 변환과 Songs로 옮기기 | EditorOsuImport |
| `analyze editor sounds`, `draw editor spectrum` | 편집기의 소리 분석과 스펙트럼 조각 | EditorAnalysisController |
| `prepare images`, `read sound`, `stretch sound` | 그림 디코딩, 소리 파일 읽기, 속도 변환 | 엔진 |

- 장치 끊김과 멈춤 기록에는 모드와 상관없이 한 줄이 들어갑니다: `work in progress: read sound (thread 1234, 15 ms); …`
  또는 `work in progress: none`. 괄호 안은 그 작업의 스레드 ID와 시작한 뒤 지난 시간입니다. 덤프의 같은 스레드 ID와 맞춰 볼 수 있습니다.
- 감시 모드는 작업마다 두 줄을 더 적습니다. 작업 스레드가 직접 쓰고, 1초 통계처럼 디스크에 닿을 때까지 기다리지 않습니다.

```text
[09:21:33.512] work started: read song list (thread 9120)
[09:21:33.640] work ended: read song list, succeeded after 127.6 ms (thread 9120)
```

- 끝난 모습은 `succeeded`, `failed`(예외가 났거나 실패 결과를 돌려줌), `cancelled`(중간에 그만두라는 요청을 받음)입니다.
- 이름에는 파일 경로를 넣지 않습니다(로그에 사용자 이름이 든 경로가 남지 않게).
- 동기 로드 모드의 소리 읽기는 게임 루프 스레드에서 돌고 그 스레드 번호로 적힙니다.
- 엔진의 표는 동시에 32개까지 담습니다. 일반 실행에서 작업 하나의 비용은 표에 올리고 내리는 atomic 쓰기 몇 번입니다.
- FMOD 자신의 스레드처럼 게임·엔진 코드가 만들지 않은 스레드의 일은 여기에 나오지 않습니다(덤프에는 나옵니다).

## 덤프 파일

엔진의 `CrashDumpService`가 씁니다. `wWinMain`이 로그 다음에 만들므로, 그 뒤의 어느 시점에 죽어도 남습니다.

| 파일 이름 끝 | 언제 | 로그의 줄 |
| --- | --- | --- |
| `-crash.dmp` | 접근 위반·스택 넘침 같은 잡히지 않은 구조적 예외 | `crash: unhandled exception, code 0xC0000005 at 0x…, thread …, dump written: …` |
| `-terminate.dmp` | `std::terminate`(아무도 잡지 않은 C++ 예외 등) | `crash: std::terminate, …` |
| `-purecall.dmp` | 순수 가상 함수 호출 | `crash: pure virtual call, …` |
| `-invalid-parameter.dmp` | C 런타임이 인자를 거부함 | `crash: the C runtime rejected an argument, …` |
| `-stall.dmp` | 메인 루프가 10초 멈춤. 게임은 계속 돕니다 | `dump requested, …, dump written: …` |

- 덤프에는 모든 스레드의 호출 스택과 로드된 모듈이 들어가고 힙은 들어가지 않습니다(수백 KB에서 수 MB).
- 로그의 줄에는 덤프 파일의 이름만 적고 폴더는 적지 않습니다.
- 덤프 안에는 실행 파일과 모듈의 전체 경로가 들어 있습니다. 사용자 폴더 아래에 게임을 두었다면 사용자 이름이 보입니다.
- 그래픽 장치 끊김과 `Run`이 잡는 예외(치명적 오류)는 프로세스가 죽는 것이 아니므로 덤프가 없고 로그만 남습니다.
- 잡지 못하는 경우와 내부 동작은 엔진 문서의 "크래시 덤프" 절에 있습니다.

### 덤프 읽기와 PDB 보관

덤프의 호출 스택을 함수 이름으로 보려면 **그 빌드의** 실행 파일과 PDB가 있어야 합니다. 다른 빌드의 PDB로는 맞지 않습니다.

- 배포 폴더를 만들 때, 같은 빌드의 `bin/x64/Release/Y's Orchestral Unison - Melodic Ultimatum.exe`와 같은 이름의 `.pdb`를
  배포본과 짝이 되는 보관 폴더에 함께 복사해 둡니다(예: `<보관 위치>/<배포 날짜>-<YOUMU 커밋>/`). 로그 첫 줄의 빌드 시각으로 짝을 찾습니다.
- PDB는 배포 폴더에 넣지 않습니다([자산·스킨](Assets.md#release-배포-절차)의 제외 목록 그대로).
- 읽을 때는 Visual Studio에서 `.dmp`를 열고 기호 경로에 보관 폴더를 넣거나, 아래처럼 봅니다.

```powershell
cdb -z youmu-20261125-101010-crash.dmp -y <exe와 PDB를 보관한 폴더> -c ".ecxr; k; q"
```

`-stall.dmp`에는 예외 정보가 없으므로 `.ecxr` 대신 `~*k`로 모든 스레드를 봅니다. 메인 스레드는 로그의 `main loop stalled` 줄에 적힌 스레드 ID입니다.

## 치명적 오류 창

`Run`이 잡은 예외로 게임이 끝날 때, 창이 있는 실행에서는 게임이 오류 창을 직접 띄웁니다. 엔진의 예외 문구 아래에
"이 실행의 기록이 아래 파일에 남았습니다."(언어 파일의 `Startup.logNote`)와 로그 파일의 전체 경로를 보여 줍니다.
숨김 실행(smoke)에서는 창을 띄우지 않습니다. 프로세스가 죽는 크래시에서는 창을 띄우지 않습니다(손상된 프로세스에서 창을 띄우지 않기 위해서입니다).

## 장치 제거 추가 정보(DRED)

`Option.ini`의 `[Diagnostics] DeviceRemovedData=1`로 켭니다. 기본은 `0`이고 옵션 창에는 항목이 없습니다.
[감시 모드](#일반-실행과-감시-모드)로 띄운 실행은 이 값과 상관없이 켭니다.
켜면 그래픽 장치가 끊겼을 때 명령 목록마다 GPU가 마지막으로 끝낸 동작이 `graphics device lost` 아래에 한 줄씩 붙습니다.
모든 그리기에 비용이 조금 붙으므로 그래픽 문제를 쫓을 때만 켭니다. 엔진은 시작할 때만 읽습니다.

## GPU 크래시 덤프(NVIDIA Aftermath)

DRED는 "어느 그리기에서 멈췄나"까지만 말합니다. NVIDIA 드라이버가 직접 남기는 덤프는 드라이버가 본 장치 상태,
그때 돌던 셰이더, 프레임 안의 위치(엔진이 남기는 표시)를 담습니다. 엔진 쪽 설명은
[엔진 진단 기록](../../Dependencies/PhantomCore/Docs/EngineDiagnostics.md)의 "GPU 크래시 덤프"에 있습니다.

- **켜지는 조건**: `/p:PhantomCoreAftermath=true`로 만든 빌드를 [감시 모드](#일반-실행과-감시-모드)로 띄우고, 실행 파일 옆에
  `GFSDK_Aftermath_Lib.x64.dll`이 있고, 그래픽 카드가 NVIDIA일 때입니다. 하나라도 아니면 지금까지처럼 DRED를 씁니다.
  로그의 `engine: gpu crash dumps: on (NVIDIA Aftermath)` 또는 `off (이유)` 줄이 어느 쪽인지 말합니다.
- **DRED와는 함께 쓰지 않습니다.** 덤프가 켜진 실행은 DRED를 켜지 않습니다. `Option.ini`의 `DeviceRemovedData=1`은
  "DRED를 쓰겠다"는 뜻으로 보아, 그 값이 1이면 감시 모드에서도 덤프를 요청하지 않습니다. 두 방식을 번갈아 볼 때 씁니다.
- **기본 빌드에는 없습니다.** 속성의 기본값은 `false`이고, 그 빌드에는 호출 자체가 없으며 DLL도 복사하지 않습니다.
  검증 스크립트와 러너는 기본 빌드를 씁니다.
- **빌드**: NVIDIA Nsight Aftermath SDK를 저장소 밖에 풀고 그 폴더를 `NSIGHT_AFTERMATH_SDK` 환경 변수나
  `/p:AftermathRoot=`로 가리킵니다(예: `msbuild YOUMU.sln /p:Configuration=Release /p:Platform=x64
  /p:PhantomCoreAftermath=true /p:AftermathRoot=D:\SDK\NsightAftermath`). Client 프로젝트가 DLL과 NVIDIA 라이선스
  (`ThirdPartyNotices/NsightAftermath/LICENSE.txt`)를 실행 파일 옆에 복사합니다. SDK의 헤더와 `.lib`는 NVIDIA 라이선스상
  배포할 수 없으므로 저장소에 넣지 않습니다. DLL은 게임에 넣어 배포할 수 있습니다.
- **남는 것**: 장치가 끊기면 로그에 `gpu crash dump: dump finished, … bytes, device now: …` 한 줄과 파일 이름이 붙고,
  `Data/Crash`에 `.nv-gpudmp`(NVIDIA Nsight Graphics로 여는 원본)와 `.nv-gpudmp.json`(같은 내용을 글로 푼 것)이 생깁니다.
  드라이버가 덤프를 끝낼 때까지 최대 8초 기다립니다. 덤프의 표시는 프레임 구간뿐 아니라 그리기 하나하나(`draw N`)까지 가리키고,
  `-gpu-frames.txt`는 마지막 여덟 프레임의 그리기와 그 사이에 GPU 자원에 일어난 일(`note:` 줄)을 담습니다.
  같은 이름의 `-process.dmp`는 그 순간 프로세스의 메모리 전체입니다(수백 MB). 그 빌드의 PDB로 열어 렌더러의 상태를 봅니다.
  개인 정보가 들어 있을 수 있으므로 밖으로 보내지 않습니다.
- GTX 10 세대(Pascal)에서는 NVIDIA 문서상 "제한된 정보"만 남습니다.
- NVIDIA의 사용 지침에 따라 진단용 실행(감시 모드)에서만 켭니다. 일반 실행은 DLL을 열지 않습니다.

## 그래픽 장치 시험 설정

그래픽 장치가 멈추는 원인을 가르는 시험용 스위치입니다. `Option.ini`의 `[Diagnostics]`에 적고, 게임을 켤 때만 읽습니다.
옵션 창에는 항목이 없습니다. 기본값은 모두 지금까지의 동작이므로, 적지 않으면 아무것도 바뀌지 않습니다.
엔진이 하는 일과 그 기록 줄의 형식은 엔진의 [그래픽 장치 시험 설정](../../Dependencies/PhantomCore/Docs/EngineDiagnostics.md#그래픽-장치-시험-설정)에 있습니다.

| 키 | 기본 | 값 |
| --- | --- | --- |
| `GraphicsValidation` | `0` | `0` 끔, `1` D3D12 디버그 레이어, `2` 디버그 레이어 + GPU 기반 검증(몇 배 느림). PC에 Windows 선택 기능 "Graphics Tools"가 없으면 검사 없이 실행하고 로그에 그렇게 적습니다 |
| `GraphicsAdapter` | `Default` | `Default`, `WARP`(소프트웨어 래스터라이저, 그래픽 드라이버를 거치지 않음, 느림), 숫자(어댑터 목록의 번호), 그 밖의 글자(이름에 그 글자가 든 첫 어댑터). 찾지 못하면 기본 어댑터를 씁니다 |
| `AllowTearing` | `1` | `0`이면 찢어짐 허용 없이 표시합니다. 창이 모니터를 혼자 덮는 경우에는 렌더와 Update가 화면 주사율에 묶일 수 있습니다 |
| `SwapChainBuffers` | `2` | `2` 또는 `3`. 스왑 체인의 back buffer 수 |
| `WaitableSwapChain` | `0` | `1`이면 표시 대기열을 1프레임으로 줄이고 프레임마다 스왑 체인을 기다립니다 |
| `RenderRateOverrideHz` | `0` | `0`은 모니터 주사율. `10`~`5000`이면 그 값을 렌더 기준 속도로 씁니다(`[Display] RenderRate`가 `display`면 그 속도, `double`이면 두 배, `unlimited`면 효과 없음). 예: `300`은 초당 300번 그리기 |

- 범위 밖의 값은 `DeviceRemovedData`처럼 `Option.ini` 전체를 읽지 못한 것으로 처리합니다(경고를 남기고 기본 설정으로 실행).
  잘못 적은 시험이 다른 설정으로 돌아가는 것보다 알아채는 편이 낫기 때문입니다.
- 로그의 `graphics switches:` 줄이 요청한 값, `engine: swap chain:`·`engine: graphics options:` 줄이 실제로 쓴 값입니다.
  검증을 켜면 경고·오류가 `engine: graphics validation …` 줄로, 끝날 때 살아 있는 객체와 요약이 `engine: graphics live object:`·
  `engine: graphics validation summary:` 줄로 남습니다. 같은 메시지는 세 번, 전체는 100줄까지 적고 나머지는 요약에서 셉니다.
  `RenderRateOverrideHz`는 엔진 시작 기록에 나오지 않습니다. `graphics switches:` 줄의 `render rate override`만 봅니다.
- 게임이 저장한 `Option.ini`의 `[Diagnostics]`에는 키마다 받는 값과 기본값을 적은 영어 주석 한 줄이 바로 위에 붙습니다.
  `DeviceRemovedData`도 같습니다. 주석의 형식은 [Option.ini](TextManagement.md#optionini)에 있습니다.

### 빌드에서 끄기

MSBuild 속성 `YoumuGraphicsSwitches`를 `false`로 빌드하면(`msbuild YOUMU.sln /p:YoumuGraphicsSwitches=false ...`)
그 빌드는 위 여섯 키를 무시하고 엔진 기본값으로 실행합니다. 로그 줄은 `graphics switches: ignored by this build, ...`가 됩니다.
누군가 `Option.ini`를 고쳐도 배포한 빌드가 검증이나 소프트웨어 어댑터로 돌지 않게 하기 위한 것입니다.

- 속성은 `Client/YOUMU.Client.vcxproj`가 전처리기 정의 `YOUMU_GRAPHICS_SWITCHES`(`1`/`0`)로 넘기고,
  `Client/Diagnostics/GraphicsSwitches.cpp`만 그 값을 봅니다.
- 기본값은 `true`(허용)입니다. 데모 빌드도 켠 채로 두고, 실제 배포 빌드만 `false`로 빌드합니다.
  `DeviceRemovedData`는 이 속성과 상관없이 늘 읽습니다.
- `Tests/Presentation/RunGraphicsSwitchesTests.ps1`이 테스트를 정의 `1`·`0`으로 두 번 빌드합니다.
  `0` 빌드는 `Option.ini`에 값이 있어도 엔진 설정이 기본값인지, 로그 줄이 무시한다고 말하는지 확인합니다.
  그리고 `YoumuGraphicsSwitches=false`가 정의 `0`으로 이어지는지 MSBuild로 확인합니다.

## 일부러 일으켜 보기

기록이 제대로 남는지 확인하는 숨은 실행 인자입니다. 옵션 창이나 `Option.ini`로는 켤 수 없습니다.

| 인자 | 하는 일 |
| --- | --- |
| `--crash-test=device` | 600프레임을 그린 뒤 엔진이 그래픽 장치를 일부러 제거합니다. 게임은 장치 끊김으로 끝납니다(종료 코드 1) |
| `--crash-test=gpu-hang` | 600프레임을 그린 뒤 엔진이 GPU가 끝내지 못하는 그리기 하나를 넣습니다. 약 2초 뒤 Windows가 그래픽 드라이버를 되살리고 게임은 장치 끊김(`DXGI_ERROR_DEVICE_HUNG`)으로 끝납니다. **화면이 잠깐 꺼지고 GPU를 쓰는 다른 프로그램도 장치를 잃을 수 있습니다.** GPU 크래시 덤프가 실제로 남는지 볼 때만, 사람이 직접 띄웁니다. 자동 테스트는 쓰지 않습니다 |
| `--crash-test=stall` | 시작 0.5초 뒤 Update 한 번이 3초 동안 돌아오지 않습니다. 이때만 멈춤 한도를 1초로 줄입니다 |
| `--crash-test=exception` | 시작 0.5초 뒤 Update가 예외를 던집니다. 엔진이 잡아 치명적 오류로 끝납니다(로그, 오류 창, 종료 코드 1). 덤프는 없습니다 |
| `--crash-test=crash` | 시작 0.5초 뒤 Update가 접근 위반을 일으킵니다. 아무도 잡지 않으므로 `-crash.dmp`가 남고 Windows가 프로세스를 끝냅니다(종료 코드 0xC0000005) |
| `--log-dir=<폴더>` | 로그를 `Data/Logs` 대신 준 폴더에, 덤프를 `<폴더>/Crash`에 씁니다(따옴표로 감싼 경로 가능). 시험이 사용자의 폴더를 건드리지 않게 합니다 |

`Tests/Presentation/RunCrashDiagnosticsTests.ps1`이 이 인자와 `--diagnostics=watch`로 게임을 숨김 창으로 여섯 번 돌려 로그 내용과 덤프 파일을 확인합니다.

## smoke의 예외 검사

smoke 경로(`--smoke-*`)는 로그를 연 바로 다음에 엔진의 `ThrownExceptionWatch`를 만듭니다. 실행이 성공으로 끝났는데 C++ 예외를
던졌다면 로그에 개수와 처음 16개의 스레드·형식·`what()`·스택(`<모듈>+0x<오프셋>`)을 적고 종료 코드 3으로 끝냅니다.
`--crash-test=`가 붙은 실행은 일부러 던지므로 만들지 않습니다. 보통 실행은 만들지 않아 비용이 없습니다.

## 코드

| 파일(`Client/Diagnostics`) | 맡은 일 |
| --- | --- |
| `GameLog` | 로그 파일 하나. 프로세스에 하나이며 `wWinMain`의 `game_log::Session`이 소유합니다. 어느 코드든 `game_log::Write`로 한 줄을 씁니다 |
| `RecentStatistics` | 1초 표본 120개 고정 배열. 할당하지 않습니다 |
| `GameDiagnostics` | 엔진의 받는 쪽(`IEngineDiagnosticSink`) 구현. 표본을 받아 보관하고 장치 끊김·치명적 오류·멈춤을 로그에 쓰며, 멈춤 때 덤프를 요청하고 치명적 오류 창을 띄웁니다 |
| `CrashDumpLog` | 덤프 관찰자(`ICrashDumpObserver`) 구현. 덤프가 써지면 이유와 파일 이름 한 줄을, 크래시면 최근 통계까지 로그에 씁니다. 통계는 읽기만 합니다 |
| `LaunchDiagnostics` | 숨은 실행 인자 읽기 |

- 한 줄은 추가 전용으로 연 파일에 `WriteFile` 한 번으로 씁니다. 그래서 스레드끼리 줄이 섞이지 않고 잠금이 없습니다.
- 멈춤은 엔진의 감시 스레드가 알립니다. 그 경로(`WritePlain`, `RecentStatistics::WriteToLog`)는 할당하지 않습니다. 멈춘 스레드가 힙을 쥐고 있을 수 있기 때문입니다.
- `wWinMain`이 로그 → `GameDiagnostics` → `CrashDumpLog` → `CrashDumpService` → `YOUMUGame` 순서로 만들고 반대 순서로 없앱니다. `YOUMUGame`은 `GameDiagnostics`를 빌려 쓰기만 합니다.
- 가리키는 방향은 한쪽입니다: `GameDiagnostics` → `CrashDumpService`(멈춤 덤프 요청) → `CrashDumpLog`(써진 덤프 통지) → 최근 통계(읽기).
- 크래시 통지(`OnCrashDump`)는 덤프 스레드에서, 죽어 가는 프로세스 안에서 불립니다. 그 경로도 할당하지 않고 미리 연 로그 핸들만 씁니다.
- `SkinSetSelection`과 `RhythmTestScene`의 저장 실패 문구는 아직 디버그 출력으로만 나갑니다.
