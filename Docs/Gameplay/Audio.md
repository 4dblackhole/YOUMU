# 플레이 오디오

파일 등록/기본 cue: `GameFlow/Submodules/GameplayAudio.cpp`.
규칙별 정책: `YOUMU.Modes/Taiko/Submodules/TaikoSoundPolicy.cpp`, 공통 ID: `TaikoSoundIds.h`.
실제 재생: `Client/Audio/GameplayAudioRouter.*`. 엔진 API 변경 때만 엔진 Audio 문서를 읽습니다.

파일 등록은 라우터의 `RegisterSound`가 SoundId와 경로를 연결하고
`AudioSystem::LoadSound`로 Clip을 로드합니다. 플레이 음악은 StreamAsyncAccurate, 히트사운드는
SampleAsync 방식이며 전역 파일 자동 검색이나 캐시는 없습니다. 압축 파일의 디코딩은
엔진의 공통 DecodeAudioPcm이 맡고 FMOD는 준비한 원본 sample rate의 float PCM을 재생합니다.
MP3의 Info/LAME 패딩은 여기서 한 번 제거하며 파형 분석도 같은 함수를 사용합니다.
Stream 계열도 현재 음원 전체 PCM을 메모리에 준비하므로 파일 크기보다 메모리 사용량이 큽니다.
음악/히트사운드의 0ms는 이 유효 PCM의 샘플 0이며 의도된 무음은 보존합니다. 같은 Sample을 가리키는
SoundId 별칭은 하나의 활성 Voice를 공유합니다. 이전 재생이 끝나기 전에 같은
Sample을 다시 요청하면 새 Channel을 만들지 않고 기존 Channel의 위치를 처음으로
되돌려 재생하며, 끝난 뒤 요청한 경우에만 새 Voice를 만듭니다. 재생 관리자는 재생 중
Clip/Bus의 공유 소유권을 유지합니다. 오디오 장치와 FMOD system은 `Run`이
소유하는 AudioSystem 하나가 관리합니다.

YME 표의 실제 참조 항목만 세션의 HitSoundFiles에 모읍니다. 비동기 Clip을 등록한 뒤
이미지 준비와 함께 PCM 로딩을 진행하며, cold 샘플과 음악 Voice 준비가 끝나 실제
Gameplay가 시작한 뒤 플레이 화면을 표시합니다. 소유권과 캐시 무효화 규칙은
[자원 로딩](../ResourceLoading.md)을 참고합니다.
Controller는 2ms 주기로 Loading/Ready/Failed를 확인하며, 준비가 끝난 시점에 QPC/DSP를 고정하고
음악을 예약합니다. 종료 Clip은 엔진 RetireClip으로 안전한 해제를 기다립니다.
히트사운드/음악 로딩 실패는 시작·재개를 막고 오류와 ESC 메뉴를 표시합니다.
실패 후 Retry는 파일을 다시 로드하고 새 시도를 시작하며, 정상 Retry는 기존 자산을 재사용합니다.
라우터는 성공한 자동화 값이 같으면 native setter를 반복하지 않고, Voice 종료 확인은
공통 PlaybackManager에 맡깁니다. SceneGameClient의 native 유지관리 주기는 2ms이며
입력 판정이나 DSP 예약 시각은 이 주기로 양자화하지 않습니다.

풍선 완료 cue는 선택한 스킨의 `HitSounds/TaikoMode/pop.wav`를 사용하며 파일이
없으면 내장 기본 스킨의 같은 파일로 대체합니다.

일반 플레이의 음악은 `PrepareMusicAt`으로 일시정지 Voice를 준비한 뒤
Voice의 PreparationState가 Ready인지 확인하고 `StartPreparedMusic`으로 차트 0ms의 소스 시작을 예약하며,
`RhythmTimer::ToDspClock`과 FMOD `setDelay`로 mixer sample clock에 연결합니다.
Pattern Offset은 노트 시각에 더하고 음악 시작 시각은 옮기지 않습니다.
플레이 타이머는 항상 최소 −2000ms에서 시작하며 첫 박자와 노트 이동 거리에 따라
더 이른 시각에서 시작할 수 있습니다. [Pause](Pause.md)의 Resume/Rewind와 Debug의 Space 재개는
살아 있는 배경음악 Voice를 Pause로 유지하고 히트사운드·틱 음성만 정지합니다.
Resume/Rewind/정상 Retry는 같은 음악 Voice의 위치를 재정렬하고 새 QPC/DSP 기준으로 재예약합니다.
히트사운드·틱·입력 피드백의 이전 Voice는 재생하지 않습니다. 현재 배경음악은 Music.Track 하나입니다.
Raw Input 히트사운드는 timestamp를 판정에만 사용하고 즉시 재생합니다.
장시간 QPC/DSP 드리프트가 입력음을 미래로 예약하여 지연시키지 않도록 분리합니다.
기존 음악 채널이 종료된 경우에만 새 paused Voice를 생성합니다. 내부 초기 탐색과 요청 위치의
탐색 순서는 엔진 Voice가 관리하고 Client는 PreparationState의 Loading/Ready/Failed를 확인합니다.
여러 탐색 요청은 native 탐색을 겹치지 않게 처리하며 최신 목표 위치로 준비합니다.
비동기 탐색 중에는 게임 시각을 고정하며, 완료 후 새 clock snapshot으로 두 시계에 동일한
20ms 준비 여유를 예약합니다. 예약 시점 전 입력은 판정하지 않고 그 이후 입력부터 처리합니다.
음악 준비/예약 실패는 Pause에서 오류를 표시하며 자동으로 게임 시간만 진행시키지 않습니다. 양수 시각은 같은 us의
음원 소스 위치로 탐색하고 음수 시각은 소스 0을 새 차트 0ms의 DSP 시각까지 기다리게 합니다.
정확한 MP3 탐색표는 비동기 로딩 중에 준비합니다. 음악 길이 이후의 재개는 음악 없이 진행합니다.
이동/Retry 시 히트사운드 bus 볼륨과 DSP bypass를 기본 상태로 돌린 뒤 이동 위치의 자동화를
적용하여 이전 위치의 효과가 남지 않게 합니다. 음악/UI 볼륨은 변경하지 않습니다.

## 상태 기반 히트사운드

노트 규칙은 직접 FMOD를 호출하지 않습니다. 상태 전이인 `NoteEvent`를 만들고
`INoteSoundPolicy`가 이를 `AudioCueRequest`로 변환합니다. binding은 event,
이전/다음 상태, 판정 등급, hit index, tick index를 조건으로 사용할 수 있습니다.

- 큰 노트는 Good 이상인 첫 번째 accepted hit(`hitIndex == 0`)에서만 전용
  사운드를 냅니다.
- 범위 밖 입력은 노트 전용 사운드가 아니라 `UserInputFeedback` bus의 일반
  Don/Kat 사운드를 냅니다.
- 홀드 tick은 `TickAccepted`마다 한 번만 `TickSound` bus로 전달됩니다.
- 뎅뎅의 `HitAccepted`는 횟수의 홀짝이 아니라 실제 `inputAction`으로 Don/Kat를 선택합니다.
  캇으로 시작해도 Kat 사운드가 나오며, YMP의 명시적 히트사운드 지정은 그대로 우선합니다.
  교대 거부는 노트 전용 cue를 만들지 않고 기존의 일반 입력 피드백을 사용합니다.

이 분리 덕분에 같은 노트 규칙을 유지한 채 스킨별 파일, 볼륨, pitch, pan,
우선순위와 bus를 바꿀 수 있습니다.

## 실시간 오디오 효과

YME Sounds의 `#Volume`, `#ReverbSend`, `#LowPassCutoff`, `#HighPassCutoff` 자동화는
`PlaySession::EvaluateAutomation`에서 timeline 값으로 평가한 후
`GameplayAudioRouter`가 엔진 bus/effect에 적용합니다. 명령은 종류/대상별 시간 인덱스로
최신 값만 조회하고 같은 시각의 Controller/Presenter 조회는 평가 결과를 공유합니다.
대상이 같은 실제 출력에 연결되는 경우에도 원래 명령 순서대로 적용합니다.
Reverb/필터 DSP는 세션 진입에 미리 만들고 bypass 상태로 두며 첫 자동화부터 활성화합니다.
향후 compressor,
delay 또는 모드 전용 효과는 차트 명령과 라우터 mapping을 추가하되 노트
규칙에서는 FMOD 타입을 참조하지 않습니다.

기본 히트사운드는 수락한 입력에 맞는 Don/Kat이며 명시적 hitsound도
지원합니다. 이는 게임별 라우터를 거치며 새 오디오 채널 정책을 추가하지 않습니다.

`#Volume`은 히트사운드 크기를 제어하며 대상은 HitSound/TickSound/UserInputFeedback입니다.
음악과 UI 볼륨은 이 명령으로 변경할 수 없습니다. 히트사운드 표는 YME가 소유하고
등록할 파일은 YME 폴더 기준으로 해석합니다. YMP는 Effect file과 노트별 표 인덱스만
참조합니다. 지점/Area 및 보간식 문법은 [YME](../Formats/Yme.md)가 기준입니다.

표시 배치·키빔·노트 이미지와 스킨 경로는 [Presentation](Presentation.md)이 소유합니다.
