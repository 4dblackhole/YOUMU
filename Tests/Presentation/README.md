# Presentation 회귀 러너

실제 Client 오브젝트를 링크한 테스트 실행 파일을 만들어 화면·입력·오디오 동작을 검사하는 러너입니다.
러너는 솔루션 프로젝트가 아니라 `Run<이름>.ps1` 스크립트이며, 필요한 Client 오브젝트 목록을 프로젝트에서 읽어 직접 컴파일·링크합니다.
대부분 숨긴 창의 D3D12와 FMOD(NoSound 또는 실제 출력)로 돌고, 테스트 소스는 MSVC Code Analysis(`/analyze`)로 컴파일합니다.

## 러너 표

소요 시간은 이 PC(4코어)에서 솔루션 빌드가 끝난 뒤 한 번 잰 값이며 테스트 exe를 새로 빌드하는 시간을 포함합니다(Debug / Release, 초).
`Test.ps1`로 돌리면 입력이 바뀌지 않은 테스트 exe는 다시 빌드하지 않으므로 그만큼 짧아집니다([빌드와 검증](../../Docs/Contributing/Verification.md#검증-스크립트)).

| 러너 | 검사하는 것 | 소요 |
| --- | --- | --- |
| [AllNotesGameplayTests](#allnotesgameplaytests) | 전체 노트 채보를 실제 D3D12·FMOD로 끝까지 재생·판정(`-Effects`는 YME 효과 채보) | 44 / 63 |
| [AssetPathTests](#assetpathtests) | 개발·배포 폴더의 Songs·스킨·Option.ini 경로 | 11 / 18 |
| [EditorPresentationTests](#editorpresentationtests) | 에디터 화면·도구·메뉴·오디오 분석·재생·미리듣기·경고 대화상자 | 102 / 117 |
| [FailureBoundaryTests](#failureboundarytests) | 메모리 할당 실패 경계 | 9 / 9 |
| [GameplayAudioRetirementTests](#gameplayaudioretirementtests) | 플레이 오디오 라우터의 clip 해제·bus 음량 | 12 / 16 |
| [GameplayJudgementTests](#gameplayjudgementtests) | 플레이 HUD·판정 표시·타이밍 인디케이터·노트 표시·진행 막대·키빔 | 27 / 42 |
| [GameplayPauseTests](#gameplaypausetests) | 일시정지 메뉴·재개·되감기·Retry·Retire·재시작 키·Debug 조작 | 29 / 47 |
| [LanguageFileTests](#languagefiletests) | 외부 UTF-8 언어 파일·영어 기본값 | 14 / 15 |
| [MusicSelectViewTests](#musicselectviewtests) | 곡 선택·에디터 곡 선택·필터·즐겨찾기·기록·검색·플레이 옵션·F7 띠 | 59 / 80 |
| [NewDifficultyTests](#newdifficultytests) | 새 난이도 메타데이터와 파일 만들기 | 13 / 22 |
| [OptionsPanelTests](#optionspaneltests) | 옵션 서랍·키 설정·Option.ini·공용 툴팁·스크롤바·드롭다운·경고 대화상자 | 18 / 31 |
| [OsuImportValidationTests](#osuimportvalidationtests) | osu! 가져오기 결과 검사와 Songs 배치 | 13 / 24 |
| [ResultPresentationTests](#resultpresentationtests) | 결과 화면·그래프·랭크·HP 그림·실제 Scene 전환 | 27 / 46 |
| [SkinTests](#skintests) | 스킨 매니페스트와 화면별 그림 교체 | 15 / 39 |
| [TitlePlayerTests](#titleplayertests) | 타이틀 음악 플레이어 | 14 / 29 |
| [GameplayTimingProbe](#gameplaytimingprobe) | 측정 하네스(통과/실패 없음, `VerifyAll`에서 제외) | — |

## 실행

보통은 공유 잠금을 잡는 검증 스크립트로 돌립니다([빌드와 검증](../../Docs/Contributing/Verification.md)).

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Verify/Test.ps1 -Configuration Debug -Runners GameplayPauseTests,ResultPresentationTests
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Verify/Test.ps1 -Configuration Release -AllRunners
```

러너를 직접 부를 때는 솔루션을 빌드한 뒤 x64 Visual Studio 개발자 PowerShell에서 저장소 루트를 기준으로 실행하고,
`FMOD_ROOT`를 설치한 SDK로 설정하거나 `-FmodRoot`를 넘깁니다.

```powershell
./Tests/Presentation/RunGameplayJudgementTests.ps1 -Configuration Debug
./Tests/Presentation/RunGameplayJudgementTests.ps1 -Configuration Release
```

- 모든 러너는 Debug와 Release 두 구성으로 돌립니다.
- 실제 엔진을 띄우는 러너는 빌드나 다른 장치 테스트와 동시에 돌리지 않습니다(특히 GameplayPauseTests의 cold start·pause transport는
  고정된 준비 대기 한도를 씁니다).
- 테스트 데이터는 git이 무시하는 `build/` 아래의 고유 임시 폴더에 두고 끝나면 지웁니다(`ScopedTestFolder.h`). 사용자의 Songs·스킨·
  Option.ini·기록 DB·내장 자산 캐시는 바꾸지 않습니다.
- 러너는 합성한 입력 의도·그리기 기하·오디오 실행을 확인합니다. 실제 키보드·마우스 조작, 화면을 눈으로 보는 확인, 청음을 대신하지 않으므로
  완료 보고에서 구분합니다.
- `-SongFilter <카탈로그 메타데이터 경로의 일부>`는 EditorPresentationTests·AllNotesGameplayTests의 테스트 전용 선택이며 게임 명령줄 옵션이 아닙니다.

## AllNotesGameplayTests

```powershell
./Tests/Presentation/RunAllNotesGameplayTests.ps1 -Configuration Debug
./Tests/Presentation/RunAllNotesGameplayTests.ps1 -Configuration Debug -Effects
```

- 배포된 AngelDream의 `All Notes Verification` YMP와 실제 MP3를 읽어, QPC 리듬 타이머·DSP 예약 음악·실제 `GameplayPresenter`와
  오디오 라우터·D3D12 렌더링·실제 오디오 백엔드(NoSound 대체 없음)로 채보 전체를 실행합니다.
- 일반 노트는 완벽한 입력을, 촘촘한 동·캇 Buzz 헤드는 +50 ms GOOD 입력을 받습니다. 논리 노트 19개, 틱이 순서대로 한 번씩,
  최종 정확도가 로직 재생과 같아야 합니다.
- 노트 노드를 늦게 만들고, 지난 하위 트리를 지우고, Reset 뒤 첫 노트 시각에 다시 만드는지 확인합니다. 스킨 별칭은 실제처럼 샘플을 공유합니다.
- 진행 막대가 실제 재생 중에 나아가고 마지막 노트·꼬리에서 100%가 되는지 확인합니다.
- 창은 숨기고 입력 시각은 `PlaySession`에 직접 넣습니다. 게임에 자동 플레이 경로가 없고 옵션·사용자 곡·스킨을 바꾸지 않습니다.
  자산 루트는 실제 해석기(개발 중 `Client/Assets`, 배포본은 EXE 옆 `Asset`)를 쓰며 채보나 음원이 없으면 실패합니다.
- `-Effects`는 `TEST_YME_EFFECTS`로 컴파일해 AngelDream `YME Effects Verification`을 읽습니다(러너 옵션이며 게임 CLI 경로가 아님).
  YMP의 명시적 참조로 같은 난이도 폴더의 YME와 상대 사운드 표를 찾고, Whole/Separate 곡선·히트사운드 변경과 볼륨·싱코페이션 구간을 담은
  논리 노트 15개를 같은 실제 경로로 연주합니다. 촘촘한 Buzz 헤드는 일부러 늦게 쳐서 몸통 틱 37개 중 2개가 실패하고 35개가 한 번씩 성공해야 하며,
  최종 정확도는 97.8897%입니다. 판정 경계 확장·예외·모든 보간·위치 연속성·사운드 우선순위·Highlight 상태는 로직 테스트의 YmeEffectsTests가 따로 봅니다.
  들리는 음량 차이나 움직임의 모양은 확인하지 않습니다.
- `-SongFilter Omoiyoshino`처럼 곡을 고르면 원본 PCM의 첫 어택과 컴파일된 첫 노트 시각을 1 ms 안에서 비교하고, 처음 4초를 D3D12와 실제
  오디오 출력으로 그리고·판정하고·재생합니다. 첫 노트를 첫 어택에 맞춰 쓴 채보에 씁니다. 기본 전체 노트 재생을 대신하지 않습니다.
- `-FullResult -SongFilter Omoiyoshino`는 그 곡을 끝까지 실시간으로, 시각을 찍은 테스트 입력·보이는 D3D12 창·실제 FMOD 출력으로 돌린 뒤
  실제 결과 화면에 모은 결과를 60초 동안 보여 줍니다. 개인 기록을 쓰지 않고 게임 EXE에 자동 플레이를 넣지 않습니다.

## AssetPathTests

- 실제 자산 라이브러리와 `OptionSettings`를 링크합니다. 같은 실행 파일을 격리한 개발·배포 폴더 트리에 복사하고 작업 폴더를 반대쪽 트리로 둔 채
  실행해, 외부 Songs·기본 스킨 경로와 Option.ini가 EXE 옆에 남는지 확인합니다.
- 저장된 Songs 덮어쓰기는 작업 폴더가 다른 트리여도 EXE 기준으로 남고, 빈 값은 기본 탐색으로 돌아갑니다.
- AngelDream의 YMP/YME 짝은 카탈로그 로직 테스트가 따로 봅니다.

## EditorPresentationTests

```powershell
./Tests/Presentation/RunEditorPresentationTests.ps1 -configuration Debug
./Tests/Presentation/RunEditorPresentationTests.ps1 -configuration Debug -SongFilter Omoiyoshino
```

에디터 오브젝트 목록은 Client 프로젝트에서 가져오므로 지운 오브젝트를 링크하지 않습니다. GPU를 초기화하지 않고, 기록 렌더러에서만 불변
사각형 묶음을 펼쳐 그리기를 검사합니다.

- **화면과 분석**: 실제 곡·히트 스펙트럼의 그리기 패킷, 음수 노트·음악 시간 범위, 슬라이더 입력, 탭 표시, 언어, 크기 변경, Canvas 정리.
  커밋된 Ogg Vorbis 톤 예제를 실제 분석 worker와 Audio 탭으로 돌려 파형·음악 FFT·히트 FFT 사각형을 확인합니다(설치된 Vorbis 코덱이나
  FFmpeg 불필요). EditorAudioAnalysisIntegrationTests는 실제 WAV/Ogg/MP3의 길이·주파수/레벨·반대 위상 스테레오·Unicode/이름 바꾼 경로·
  제한된 취소·잘린 스트림 오류·내장 히트사운드를 봅니다(엔진 없는 도메인 오디오 테스트는 PCM을 주입해 알고리즘만 봄).
- **범위와 타일**: 실제 Canvas 수신자를 끌어(좁게 줄인 화면 포함) 순서대로인 원본 시각·언어별 ms 표시·세 덮개를 확인하고, FFT 타일 재사용과
  채보 revision 불변을 봅니다. 앞서 본 범위로 돌아가면 끝난 타일을 다시 쓰고, 타일 캐시는 가장 오래 전에 보인 타일부터 버려 바이트 한도 안에 머뭅니다.
- **재생 속도**: 슬라이더 끝값 0.25/2.00, 결정적인 DSP·cue 시각, 음수 프리롤, 재생 중 변경, 이미 들린 히트와 대기 중인 히트, 일시정지·재개,
  잘못된 값. 실제 FMOD NoSound로 MP3를 여러 번 멈추고·옮기고·다시 재생하며 속도를 바꿉니다.
- **도구와 편집**(`EditorModeTests.cpp`): Taiko 도구 배치·스냅·삭제·취소와 예상 cue, Compose 패널의 Delete/Add/Apply(delay·Exclude·Audio FX bus),
  Score 배치와 Preview, 새 난이도 초안·Create·거부된 Create. 테스트 전용 대체 모드와 문서 어댑터를 넣어 도구·배치·좌표·메타데이터·사운드 대상·
  Save 전달·marker revision 캐시가 공통 에디터를 통해 동작하는지 봅니다. 실시간 레인 경계, 공백·대소문자 무관 Buzz 옵션, 보라 노트와 시각/명시
  사운드의 플레이 일치, 잘못된 탐색 보존, 효과·타이밍 명령, 실패한 편집 되돌리기, 그리기 실패 복구.
  Whole·사용자 Separate 곡선 유무에 따라 다섯 확대 단계·다섯 시각에서 실시간 기하를 비교합니다(BPM 변경·롱노트 몸통 포함). 삭제와 배치는 같은 균일 시간 격자를 씁니다.
  다섯 배율의 시간 확대, 레인·헤드 크기 유지와 확대 스냅, 채보 시각·revision 불변, 연속 Z/X, 마지막 꼬리·음악 끝 목적지, 길이 없음·낡은 길이 거부,
  재시도와 수동 탐색·방향 초기화. Q는 끝나지 않은 배치를 취소하고 노트를 고르며, 검사기 순환은 여섯 도구 묶음 안에서만 돌고 짝 끝점 ID를 유지하며
  다른 묶음 초안은 채보를 바꾸지 않고 거부합니다.
  실제 Taiko 끌기 범위, 빈 여백, 빈칸 유무에 따른 노트 덮어쓰기, 덮어쓴 원본을 되살리는 묶음 Undo/Redo, Ctrl/Shift 타이밍·효과 행 선택,
  메타데이터 이력. 실시간 휠 이동은 박 격자를 따르며 채보 revision을 바꾸지 않습니다. 32단계 상한·저장 상태 추적·redo를 남기는 실패 편집·
  롱노트 짝 복원은 도메인 테스트가 봅니다.
- **메뉴**(`EditorMenuTests.cpp`): Save만 있는 Canvas 메뉴, 한국어/영어와 미저장 표시, 네 화면 크기의 포인터 대응, 마우스·키보드 Save 한 번,
  Escape·바깥·오른쪽 클릭 닫기, 입력 소비, observer 정리. 합친 화면에서 메뉴와 아래 띠의 공존과 메뉴 크기 변경을 봅니다.
- **재생**(EditorPlaybackTests): 실제 비동기 MP3 열기·재생(FMOD NoSound), 정확한 음악 길이와 끝 근처 탐색, 에디터 DSP transport,
  일시정지·재개·음수 프리롤, 촘촘한 cue 마감, 별칭·voice 재사용, 지난 cue 건너뛰기, 곡·히트사운드 voice의 플레이어 음악/효과음 음량, 정리.
  실제 MP3 일시정지·재개 12회로 재사용 스트림 초기화와 목표 탐색을 따로 봅니다(음수 프리롤, 멈춘 동안의 탐색 포함). 두 준비가 끝날 때까지 시간축은 멈춥니다.
  미리듣기 실패 재시도 억제, 선택 복구, 소유 voice 정리, 연속 취소된 분석 작업 8개 뒤 실제 오디오 분석 성공도 확인합니다.
- **메트로놈**(EditorMetronomeTests): 격자 캐시 생성, 템포·박자·오프셋·delay 변경, 재생 클릭과 원본 히트사운드 분석 marker의 분리.
- **경고 대화상자**: 미저장 변경에서 Escape는 Discard/Cancel/Save를 묻고, 그동안 에디터 단축키는 기다리며, Escape는 취소, Discard는 저장 없이 나갑니다
  (내장 채보는 저장하지 않음). 저장에 성공하면 쓴 YME를 게시합니다.
- **곡 미리듣기**: 내장 곡을 음원 경로로 `SongPreviewController`에서 재생합니다. 포커스한 곡은 다시 읽지 않고, 일시정지·정지는 voice를 유지하며,
  제자리 탐색, 곡 끝은 한 번 보고(타이틀) 또는 처음부터 다시(곡 선택), 실패한 파일은 매 프레임 다시 열지 않고, 해제된 오디오는 돌아오면 다시 엽니다.
- **곡 고르기**: 기본은 개발 Songs 폴더에서 추적하는 내장 곡을 골라 분석 마감이 사용자의 가져온 음악과 무관합니다. `-SongFilter`는 같은 폴더의
  다른 곡을 고르며, 첫 노트가 파형 어택의 10 ms 구간에 드는지 확인한 뒤 평소 스펙트럼 기하·transport 검사를 돕니다. 문서는 저장하지 않습니다.

## FailureBoundaryTests

자기 단일 스레드 프로세스 안에서만 C++ 할당을 바꿔 끼웁니다. 임시 문자열을 할당하지 못하면 수 파서가 false를 돌려주고, 오류를 저장하는 것까지
실패하면 자산 초기화가 빈 오류 문자열과 함께 false를 돌려주는지 확인합니다. RCDATA 묶음을 일부러 빼므로 사용자 캐시를 설치하거나 읽기 전에
초기화가 실패합니다. 게임 시작 경로가 아닙니다.

## GameplayAudioRetirementTests

실제 라우터 오브젝트를 공개 SDK와 링크합니다. FMOD는 NoSound이고, 주입한 테스트 clip·voice로 성공·실패·예외를 던지는 Stop을 시험합니다.
마지막 라우터 별칭과 voice가 Loading 상태의 clip을 놓으면 Ready가 될 때까지 은퇴 상태로 남았다가 정확히 한 번 해제되어야 합니다.
bus 검사 백엔드로 플레이어 음악 음량이 Music bus를, 효과음 음량이 나머지 bus를 키우고, YME 음량 자동화가 그 위에 곱해져 음량을 바꿔도 남으며,
Reset하거나 다시 만든 bus도 플레이어 음량을 유지하는지 봅니다. 실제 오디오 백엔드를 추가하지 않습니다.

## GameplayJudgementTests

실행 파일이 실제 Presenter·JudgementView·키빔 오브젝트를 링크하고 실제 대체 PNG 메타데이터를 읽습니다. GPU·오디오 장치는 시작하지 않습니다.

- **판정 표시**: 판정 색 띠, JudgeLevel에 따른 길이, 싱코페이션 중 고정된 기본 창, ±10 ms 여유, 이른·늦은·0·잘린 마커, 글자만 있는 판정 그림,
  입력 시각을 지어내지 않는 MISS, 빨간 빔 거부에서만 나오는 POOR, 범위 밖·뗌·판정 없는 틱 억제, 200 ms 페이드·재시작·Reset, 물려받은 판정원 위
  6에 가운데 맞춘 글자, 크기 변경, 노드·그림 재사용, 받아들인 타격이 수동 MISS보다 우선.
- **타이밍 인디케이터**: 타격 막대는 12px 막대 위 높이 48이고 최대 50개가 살아 있으며(풀 64개) 각각 5초에 걸쳐 사라지고, 밀려난 막대는 100 ms에 사라집니다.
  넘침·수명·크기 변경·노드 재사용을 봅니다. 반투명 `Hud.TimingPanel`은 색을 입히지 않은 다섯 조각(고정된 날개와 가운데, 늘어나는 기둥)으로 그려
  JudgeLevel 폭(최소 300)을 따르고 날개에 EARLY/LATE가 있습니다. 평균 오차 탭은 명시적인 빈 값, 지금 그려진 막대의 부호 있는 0.1 ms 평균
  (`+`는 늦음, U+2212는 이름, 0은 부호 없음)을 보이며, 다 사라진 막대·50개 제한으로 밀려나는 막대·Reset으로 지운 막대는 빠집니다.
- **HUD**: 숫자 HUD 글자, 키 레이아웃 안에서 키와 겹치지 않는 콤보, 정확도 → 1억 점 점수, 실시간 판정 수 8개, 되감기·Reset 복원, 좁은·넓은 화면 줄바꿈,
  숫자 노드·그림 재사용. 키 오버레이 깜빡임은 동·캇의 빨강/파랑을 씁니다.
- **HP**: 틀 위치, 채움 잘라 보이기, 클리어 기준에서 그림 전환, 클리어 표시, 자물쇠, 크기 변경. 유효 노트가 없는 채보 진입 차단은 GameplayPauseTests,
  수치·롱노트·POOR·되감기·종료 판정과 형식·기본값은 로직 테스트의 HpGaugeTests가 봅니다.
- **플레이 화면(UI-02)**: 2px·50% 마디선, Highlight 판정원(곡 시간 250 ms 페이드·박 맥동·뒤로 이동하면 바로 바뀜·레인 크기로 잘린 빛), 공용 화면 오류 줄
  (종류·경로와 행·원인, 위 20 가운데, 최대 폭에서 한 줄로 잘림, 좁은 화면 여백, 비우면 숨김), 남은 타격 배지.
- **노트와 기어**: 공유 `ScrollGear` 부모를 옮기면 모든 자손이 함께 움직이고 화면 HUD는 고정되며, 크기를 바꿔도 바깥 기어 아래가 중심선에 남습니다.
  Separate 속도 변경과 뒤로 탐색에서 시간 순서대로 노트를 쌓고, 노트는 판정원과 그 덮개 앞에 있습니다. 횟수 롤·큰 롤·풍선·뎅뎅의 남은 타격 표시·감소·
  넘치는 타격 고정·완료 숨김·Reset을 봅니다. Whole·Separate에서 여러 시각·화면 크기로 노트와 마디 중심이 겹치는지, 횟수만 세는 롤 헤드 네 종류의
  타이밍 피드백 억제·흰 빔·처리 비율 판정 글자(다 치면 MAX, 하나도 못 치면 MISS) 완료를 확인합니다. 뎅뎅은 두 시작 색 모두 반복 입력 거부(횟수·다음 입력 유지, POOR·타이밍 마커 없음, 흰 빔), Reset,
  MAX 완료를 봅니다. 자동화 진단을 공용 오류 HUD에 보이고 지우면 숨기는지도 봅니다.
- **배경**: 실제 PNG 메타데이터로 네 화면 크기에서 비율을 지키는 cover와 2D 오프셋, 전체 화면 BG·오프셋 회귀.
- **Debug 타이머**: Debug 빌드의 부호 있는 us/ms 표시, 멈춘 시간과 1 ms 변경, Release에서 표시 없음.
- **진행 막대**: 마지막 롱노트 꼬리 길이, 프리롤·포스트롤 고정, 채움 UV와 마커 위치, 현재/전체 시간, 공유 `RhythmTimer`의 일시정지·재개, Reset,
  네 화면 크기, 컴파일된 BPM·delay 시각, 빈·0·음수 길이, 노드·그림 재사용.
- **키빔**: 러너가 이어서 `LaneKeyBeamTests.cpp`를 키빔 단위만으로 빌드해 돌립니다. 자유 입력, 판정 색과 보간, 입력 거부, 롤 틱, 놓친 노트 넘김,
  0/100/200 ms 알파, 재시작, 스프라이트 재사용, 물려받은 Lane 회전·배율, 크기 변경 잘림과 Reset(오디오 장치·렌더러 없음).

연관 로직 테스트: PlayResultTests(논리 노트 콤보, 롱노트 일곱 종류, BAD/POOR, 종료·거부 이벤트 순서, Retire, 최대 콤보 전달).

## GameplayPauseTests

실제 일시정지·메뉴·컨트롤러·오디오 오브젝트를 링크합니다.

- **메뉴**: 새 디자인의 행 크기·간격, `PAUSED` 제목과 준비 중 띠, 키보드 포커스, 범위 안 조정, 포인터 Resume/Retry/Retire, 값만 바꾸는 선택기 클릭,
  언어 변경, 세로·넓은 화면 크기 변경, 준비 중 막기, 소유 Canvas 정리. 로딩 준비 실패 때의 메뉴 모양(진행 띠 없음·값 유지·Retry 선택)은 TestMenu,
  컨트롤러 상태(`PreparationFailed`·`AttemptStarted`·`ErrorSource`)는 실패 자원 회귀가 봅니다.
- **transport(숨긴 D3D12)**: 시계 멈춤, 판정을 지키는 Resume, 박자·BPM 변경을 낀 네 마디 Rewind, 점수 복원, Retry 자격, Retire의 MISS 수와 제어 기록.
  clip 백엔드 검사로 원본 탐색 위치와 채보 0 이전의 DSP 예약을 보고, 40 ms 비동기 탐색 검사로 이른 재개 거부·멈춘 시간·한 번만 받는 명령·QPC/DSP
  선행 정렬·히트사운드 재생 없음·실패 처리·Retry 자원 재사용을 봅니다. 실제 FMOD NoSound MP3로 원본 0·양수·끝 근처 탐색과 실제 끝을 반복 확인합니다.
- **Debug 조작**: Debug 전용 Space 정지, 같은 Raw Input 묶음과 멈춘 동안의 노트 입력, 1x/1.5x Accelerate에서 실제 1초당 채보 1초로 움직이는 1/2 길게
  누르기 탐색, 한 번씩인 3/4 밀리초 단계, 음악 재개 정렬, ESC 메뉴 경계. Release에서는 Space와 1–4가 멈추거나 탐색하지 않습니다.
  뒤로 탐색하면 지난 MISS 기록을 MISS 피드백 반복 없이 되살립니다.
- **재시작 키**: 기본 백쿼트를 0.5초 누르고 있으면 재시작합니다. 짧게 누르기, 예전 R 키, 재시작을 넘어 계속 누른 키는 무시하고 새로 0.5초 누르면 재시작합니다.
- **포커스**: 키 이벤트 없이도 늦은 Bad 노트가 다음 Good 노트에 밀려 자동 MISS가 되는지 두 구성 모두에서 봅니다.
- **모드 독립**: 테스트 전용 두 번째 모드가 같은 `GameplaySessionController` 수명과 다형 결과를 쓰고, Ending이 결과를 정확히 한 번 넘깁니다.
- 재생이 계속되는지는 두 시계 읽기 사이의 실제 경과 시간과 비교하므로 바쁜 PC에서도 실패하지 않지만, 250 ms 탐색은 여전히 그 범위를 벗어납니다.
  숨긴 cold start·pause transport는 고정된 준비 대기 한도(15초·10초)를 쓰므로 빌드나 다른 장치 테스트와 함께 돌리지 않습니다.
- Retire는 남은 노트를 미처리 논리 노트 없이 모두 MISS로 만듭니다.

연관 로직 테스트: PlayPauseTests(Buzz 안 되감기, 반복 취소, 입력 기록 보존, 롱노트 일곱 종류의 0점 MISS 강제). 이 검사는 자동 입력 의도와
그리기·오디오 확인이며 실제 ESC·화살표 키, 청음, 화면 확인이 아닙니다.

## LanguageFileTests

문자 그대로의 UI 키에 기준 영어 기본값이 있는지 확인한 뒤 `/W4 /analyze`로 컴파일해, BOM·Unicode·이스케이프·잘못된 UTF-8, 항목 격리, 두 외부 파일이
없을 때의 영어 대체, 일부만 있는 영어·한국어 묶음, 잘못된 형식 인자, 유효한 인자 순서 바꾸기, 불변 string-view 소유, 다음 카탈로그 생성 때의 교체를 봅니다.
영어 표시 문구와 값 형식, 선택한 EN/KO/JA 설명, 일본어 설명 키의 완전성, 기존 불변 묶음·대체 계약도 확인합니다. 일본어는 같은 INI의 Language 칸에 `ja`를
더하며 저장된 ko/en은 그대로 유효합니다. 주입된 카탈로그가 실제 메뉴·옵션·에디터·결과·플레이 화면에 쓰이는지는 각 화면 회귀가 봅니다.

## MusicSelectViewTests

```powershell
./Tests/Presentation/RunMusicSelectViewTests.ps1 -Configuration Debug
```

실제 화면·상태 오브젝트를 링크합니다. 곡·옵션 파일을 쓰지 않습니다. 게임 곡 선택과 에디터 곡 선택 두 용도를 모두 돌립니다.

- **목록**: 메모리 곡 5,000개에서 포커스·검색·크기 변경을 거쳐도 보이는 카드만 만들고, 실제 카탈로그 로드·캐시 재사용·오류 결과를 polling합니다.
  다른 난이도를 누르면 선택만, 선택된 난이도를 누르면 정확히 한 번 시작합니다. 실제 드롭다운으로 Base BPM 정렬을 고르고 수 순서와 같은 값의 안정 순서·
  곡 포커스 유지를 본 뒤 언어 전환과 검색을 봅니다. 곡 목록의 공유 스크롤바를 첫 카드에서 끌어냈다 되돌리고, 재생 중 표시가 그 곡이 들릴 때만 포커스한
  곡명 21px 앞에 보이며, 이름이 여러 개인 카드 제목에는 모든 이름을 힌트로 보이고 맞게 들어가는 아티스트 하나에는 아무것도 보이지 않습니다.
  접힌 카드에는 썸네일 노드나 BG 요청이 없고, 곡명·아티스트는 한 줄이며, 선택한 채보의 재킷 그리기는 그대로입니다.
- **틀**: 좁은·보통·넓은 화면에서 세 열은 패널 스프라이트 없는 노드이고, 한 면이 x 362에서 그림자 띠와 함께 시작해 화면 오른쪽 끝까지 가며(두 용도 모두,
  넓은 화면에서만 늘어남), 정보와 목록 사이에 가는 줄이 있고, 막과 ALL 바는 없으며, 기록 줄·안내·페이지 조작은 화면 왼쪽 끝에서 같은 너비의 판 위에 있습니다.
  16:9 이상 배치를 지키고 16:9 미만에서는 레터박스 띠와 함께 통째로 줄며, 곡 목록은 오른쪽 끝에 남습니다.
- **대표 이름**: 첫 번째 이름·아티스트·제작자만 정보와 카드에 한 줄씩 보이고, 카드 제목 위 휠은 목록을 스크롤하며, 전역 옵션은 그 스크롤을 막고,
  숨긴 D3D12 통합 실행이 한 줄 제목을 그립니다. 흐르는 글자의 Unicode 보존, 반복 글자 공유, 픽셀 이동, 충분히 넓어지면 멈춤도 봅니다.
- **필터**: 원래 번호를 지닌 보이는 패턴 투영, 모달 이동·시작 격리, 반응하는 필터·정렬 조작. 숨긴 네이티브 창 실행에서 실제 범위 칸을 고치고 IME와 Enter를
  구별하며, 원자적 Apply/Reset/Close와 잘못된 경계, 모드 팝업 아래 칸 숨김, Scene 숨김 정리를 봅니다. 런타임 값을 모두 모르면 Difficulty는 비어 있고 꺼져 있습니다.
- **즐겨찾기 폴더**: 실제 폴더 탭, 가로 휠 스크롤, 투영된 키보드 이동, 트랜잭션 소속, 네이티브 Unicode 만들기·이름 바꾸기·취소 대화상자, 활성 폴더 삭제,
  빈 선택, 독립 폴더 목록 스크롤, 언어 변경. 숨긴 D3D12 실행은 검색 숨김과 모달의 Escape·옵션 키 격리도 봅니다. 저장·잘못된 JSON/Unicode·원자적 교체 실패·
  채보 보존은 로직 테스트가 봅니다.
- **선택한 BG**: 두 용도 모두 전체 화면 그림 재사용, 크기 변경에 따른 비율·오프셋 맞춤, 같은 경로의 오프셋 변경, 지우기·대체.
- **기록**: 기록 내역은 실제 SQLite 페이지와 별도 기록 필터 모달로 정해진 패널 안에 남습니다. 그 모달로 옛 직접 조건을 고쳐 늦은 Apply, 실제로 맞는 행,
  패널 경계 유지, 채보 변경을 봅니다. 숨긴 네이티브 창 실행은 기록 범위, 잘못된 경계, IME·Enter 격리, 확정 값 복원, Close·Escape·Scene 숨김 취소,
  콤보 팝업 가림, 곡 검색·선택 보존을 봅니다. RecordFilterTests(로직)는 격리된 DB에서 포함 AND 조건·uint64 시드·직접 OR/NOT 묶음·잘못된 입력을 봅니다.
- **HP 정보**: 두 용도 모두 정확한 0.01 단위, 9.00 기본값, 패턴·언어 변경, 빈 상태 지우기, 좁은·표준·넓은 화면에서 정보 열 안에 나란한 수치.
- **검색**(`SongSearchInputTests`): 실제 화면을 숨긴 D3D12/NoSound 창에서 돌려 실제 Unicode 편집 컨트롤에 영어·한국어·일본어를 넣고, 한 단어와 AND 결과,
  편집·IME 키가 Scene 명령을 부르지 않음, 옵션 표시와 네이티브 컨트롤 정리를 봅니다. 사용자 클립보드를 쓰거나 IME 후보를 손으로 고르지 않습니다.
  옵션 버튼 입력은 이미 열린 패널을 닫습니다. 카탈로그 새로 고침은 마지막 Songs 루트만 줄 세우고 낡은 결과를 버리며 로드 도중 되돌아가는 것도 처리합니다.
- **플레이 옵션**(`PlayOptionsPanelTests.cpp`): 옵션·uint64 왕복, Done·Esc가 한 번만 적용, Reset 기본값, 잘못되거나 숨은 칸 고치기, 조건부 조작, HP 게이지
  선택과 `Clear 60%` 설명, 바뀐 옵션 수 배지, 평평한 모달 크기와 4:3 Canvas에서의 축소, 네이티브 입력 표시, 낡은 동작 거부. 실제 Win32·엔진 경로로
  하단 띠 진입과 배지, 수 편집, Esc 격리, Reset/Done, 선택·검색 유지, 그리기와 정리를 봅니다. 값·범위, 시작 스냅샷, Retry 계약은 도메인·플레이 테스트가 봅니다.
- **F7 띠**: 측정 전 대시, `FPS 144 | UPS 1000`, Debug 빌드에서만 draw 수, 오른쪽 아래 모서리.
- **에디터 전용**: 곡 카드의 `+` 버튼과 만들기 의도. 실제 변환기 프로세스를 osu! 곡 폴더에 돌려(훑기·고르기·배치) 난이도 목록을 조작합니다.

## NewDifficultyTests

실제 만들기·대화상자 오브젝트를 `/W4 /analyze`로 링크합니다. 실행마다 이름이 다른 `build/new-difficulty-tests` 아래 Songs 예제를 쓰고 끝나면 지웁니다.
Unicode 메타데이터, 비어 있는 기본 Taiko 타이밍, 옆의 YME와 Songs 기준 YMM 경로, 덮어쓰기 없음, 잘못된 이름·BPM, 0.01 단위 JudgeLevel 1.00~100.00, 취소,
같은 네이티브 대화상자에서 잘못된 칸 고치기, `SongCatalog`의 다시 찾기를 봅니다. 현재 스레드의 창 열거로 자기 대화상자를 찾고, 숨겨지고 비활성인 상태에서
잘못된 메타데이터 고치기를 확인하므로 데스크톱의 활성 창에 기대지 않습니다. 타이머는 자기 검사에 맞춰지고, 대화상자가 없거나 시간이 지나면 실패로 끝납니다.
타이머가 소유 대화상자를 자동으로 조작하므로 실제 마우스·키보드 검증은 아닙니다.

## OptionsPanelTests

실제 패널·설정 오브젝트를 링크합니다. 테스트 파일은 고유 임시 폴더에 두며 사용자의 Option.ini와 스킨을 바꾸지 않습니다.

- **서랍**: 폭 432 서랍과 남색 분류 막대, 막대 클릭으로 섹션을 맨 위로 스크롤, 스크롤하며 섹션 추적(마지막 섹션은 맨 아래), Up/Down 이동, 늦게 펼쳐지고
  접히는 막대, 팝업·입력 정리, 본문 밖 하단 안내, 다시 열 때 스크롤 위치 유지.
- **음량**: 끌기는 놓을 때 한 번 저장하고 0~100 밖의 값을 거부합니다.
- **키 설정**: 마우스를 올릴 때만 보이는 지우기 버튼과 옅은 `+`가 있는 키 칸, 메인·보조 키 입력, 기본 키 충돌, 예약된 게임 키, ESC 취소, 정수 글로벌 오프셋,
  잘못된 초안, 모드별 프로필 저장. 먼저 Client 모드 표현 등록부를 등록된 플레이 모드와 맞춥니다: 모드마다 한 항목, 레이아웃 바인딩과 기존 문구 키를 가리키는
  바인딩 이름, 사운드 ID가 겹치지 않는 히트사운드 파일, 바인딩마다 키 오버레이 그림 하나.
- **설정 저장**: UTF-8 설정 왕복, 패널·칸 대비, ESC로 펼친 팝업 닫기, 애니메이션 중 반복 ESC, 첫 애니메이션 전 즉시 닫기, 낡은 선택 거부와 다시 열기,
  잘못된 값, 오디오 ComboBox 세 개, 띄엄띄엄한 드라이버 번호, 출력별 목록, 실패한 전환, 읽기 전용 INI가 저장을 막을 때의 되돌리기. 장치·키·해상도 저장과
  일본어 왕복. 해상도 변경은 창 서비스를 바로 쓰고 INI 저장이 실패하면 되돌립니다. Songs 설정은 UTF-8 경로, 저장, 기본 루트, EXE 기준 해석을 봅니다.
  네이티브 폴더 선택 대화상자는 손으로 확인해야 합니다. ASIO 전환·실패는 테스트 백엔드로 보고, 따로 실제 FMOD로 Automatic에서 WASAPI로 바꾸고 현재 드라이버를
  고르며 로드한 샘플을 유지하는지 봅니다(소리 재생·실제 ASIO 장치·마우스·청음 확인은 아님).
- **공용 툴팁**: 0.5초 머묾, 가리킨 자식이 등록된 조상으로 풀림, `Ctrl+O`·`Esc / Enter` 키 칩, 옆 조작으로 바로 바뀜, 누르면 포인터가 옮길 때까지 사라짐,
  호스트 억제, 아래 끝 조작 위로 열기. 포인터를 쥐는 조작(카드 제목) 안의 툴팁 줄은 그 조작 안을 가리킬 때만 보이고, 지우면 모든 힌트를 잊으며,
  긴 이름은 폭 300 판 안에서 줄을 바꿉니다.
- **공용 스크롤바**: 가운데 6px 막대가 있는 12px 띠(가로 8/4), 넘치지 않으면 숨김, 최소 32px, 올리면 진해지고 누르면 꽉 찬 색. 끌기는 잡은 점을 유지하고
  레일을 누르면 막대가 가운데로 오며, 이벤트를 따라가는 호스트(드롭다운 목록)는 띠 자신의 포인터 이벤트로 끌기와 휠을 받습니다.
- **공용 드롭다운**(`UiDropdown`): 목록 행이 그 아래 칸보다 먼저 클릭을 받고, 열린 목록 바깥을 누르면 닫기만 합니다.
- **공용 경고 대화상자**: 배치, Enter/Esc 응답, 버튼 클릭, 막는 덮개, 진단 스크롤. 결과 화면은 실패한 기록 저장을 이것으로 다시 시도하고,
  곡 선택은 열린 동안 Enter·Esc를 화면에서 막습니다.

연관 로직 테스트: OptionTimingTests(해석된 입력·유지 매핑, 노트·틱·BPM/delay·마디·YME에 걸친 us 시각 변환, 저장 채보 불변).
엔진 SDK `--window-control`은 숨긴 네이티브 창의 실제 크기 변경과 렌더러 알림을 봅니다.

## OsuImportValidationTests

Client 소유의 `OsuImportValidation`·`OsuSongPlacement`와 공유 도메인 라이브러리를 링크합니다. 변환기 출력 폴더에서 JSON·파일 참조와 실제 세션의 노트 수를 확인한 뒤,
Songs 배치(이름, 다시 쓴 참조 세 줄, 바이트가 같은 음원·배경 재사용, 기존 파일을 지켜야 하는 충돌, 가져오기 잠금, 중간에 실패한 게시의 되돌리기)를 봅니다.
실제 변환기 출력은 `OsuImportValidationTests.exe <result.json> <출력 폴더>`로 검사합니다. 플레이 가능 검증과 배치는 변환기가 아니라 Client 몫입니다.
실제 변환기 프로세스를 쓰는 경로는 MusicSelectViewTests가 봅니다.

## ResultPresentationTests

실제 결과 Scene·화면·팩토리·그래프와 플레이 오브젝트를 링크합니다. 사용자 옵션·곡·스킨을 쓰지 않습니다.

- **판정 비율 원**: 조각 순서, 면적 비율, 띠 반지름, 포인터 찾기.
- **랭크**: 실제 내장 랭크 PNG 메타데이터를 읽어 랭크 11개를 소수 여섯 자리 정확도 21개(정확히 경계·바로 아래, 100% Y, 99.999999% X, E/F 경계)로 확인하고,
  크기 변경과 Canvas 정리 중에도 원 안에서 그림 비율을 지킵니다.
- **글자와 그래프**: 소수 둘째 자리 버림 정확도(99.999999%는 99.99%), 한국어/영어 문구, 부호 있는 산점도 축, 1 ms 히스토그램 반올림·간격, 산점도 점과 같은
  등급별 누적 색, 최고 합계, 드문 이상치, 구간을 합치지 않는 최소 픽셀 폭, 빈 표본, 좁은·넓은 배치, 포인터 라우터의 돌아가기·Retry, 꺼진 Replay,
  카탈로그 메타데이터 스냅샷, 표본 10,003개의 불변 그래프 묶음 하나. 숫자 스프라이트 UV를 풀어 최종 점수와 최대 콤보가 실제 결과와 같은지 봅니다.
- **툴팁·배경**: Back(키만)·점수·옵션 칩의 툴팁, 랭크에는 없음, 화면과 함께 숨김. BG 값 스냅샷, 기준 높이 오프셋 변환, 크기·언어 다시 만들기, 그림 실패 대체,
  플레이가 파기된 뒤 실제 D3D12 유지.
- **HP 그림**: 실제 곡선 기하의 HP 그림, 그림 안의 쓰인 것만 보이는 Pause/Rewind 요약, 언어 다시 만들기, 없음과 유효한 빈 기록의 구분, 비활성 시점 잘라내기,
  유한한 잘린 기하. 실패 표시는 HP 통과 경계, 실패 자격, 높은·낮은 HP Retire, 일시정지·Rewind로 꺼진 완주, HP 없음을 보며 실패만 아래 HP 그림 안에 표시가 붙고
  결과 HP·기록은 다시 쓰지 않습니다. HpGaugeTests(로직)는 메뉴 일시정지·Rewind에서 HP·기록이 멈추고 Resume·되감기·Retire를 지나 값으로 넘어가며 Retry에서 초기화되는지 봅니다.
- **실제 Scene 전환**: 숨긴 창의 D3D12/FMOD와 노트 하나짜리 회귀 세션으로, 실제 플레이 Scene이 자동 판정 뒤 3초를 기다려 결과를 지연 결과 팩토리로 넘기고
  자기 Canvas를 파기하는지 봅니다. Retry를 누르면 결과와 그 Canvas를 파기한 뒤 새 플레이 세션을 만들고, 돌아가기 경로는 플레이를 정리합니다.
  ESC 경로는 실제로 연결돼 있고 중단 점수와 같은 us 입력 순서는 도메인 테스트가 보며, 실제 ESC 키 검사라고 하지 않습니다.

## SkinTests

실제 `SkinManifest`·선택·바인딩과 Logo·MusicSelect 화면을 격리한 배포 폴더에서 링크하며 사용자 스킨·Option.ini를 쓰지 않습니다.
UTF-8·BOM·CRLF·Unicode 경로, 화면 폴더·그림 상대 경로, 잘못된 경로·인코딩·중복, 부품별 내장 대체, 일부만 있는 스킨, 캐시·다시 읽기, 버튼 상태,
전체 화면 로고 교체, 곡 선택 면·버튼 교체와 복원(선택한 스킨이 면·버튼·카드 그림을 바꾸고 기본 스킨으로 돌아오면 내장 부품이 돌아옴),
실제 개발 Songs 카탈로그를 쓴 가상화 곡 카드 바인딩, 옵션 분류 막대 아이콘이 스킨 그림을 유지한 채 색만 켜진 섹션을 따르는지 봅니다.
화면 캡처와 수동 입력은 따로 보고합니다.

## TitlePlayerTests

실제 `LogoView`·`TitlePlayerView`를 링크해 합성한 창 입력으로 조작합니다. Space·Left·Right가 재생/일시정지·이전·다음 곡을 요청하고 Enter만 메뉴를 실행하는지,
재생/일시정지·정지·다음 버튼과 진행 막대 클릭이 의도를 보고하는지, 끌기는 위치를 미리 보여 주고 놓을 때만(막대 밖으로 끌어도) 탐색하는지,
길이를 모르면 탐색하지 않는지, 빈 재생 목록은 글자를 비우고 빈 아트 상자를 보이며 모든 조작과 키를 끄는지, 앨범 아트를 정사각형으로 자르는지,
옵션 패널이 플레이어 키를 유지하는지 봅니다. 오디오 transport(일시정지·탐색·정지·곡 끝)는 EditorPresentationTests가 실제 NoSound FMOD 재생으로 봅니다.
청음과 실제 입력은 손으로 확인합니다.

## GameplayTimingProbe

측정 하네스이며 통과/실패 테스트가 아니고 `VerifyAll`에 들어가지 않습니다. 실제 `RhythmTestScene`, FMOD 출력, 보이는 1600×900 창으로 채보 하나를 재생하므로
혼자 돌립니다(동시 빌드나 엔진 검사가 측정값을 바꿉니다). 게임은 probe를 설정하지 않으며, 하네스가 `GameplayLaunchRequest::timingProbe`
(`Client/GameFlow/GameplayTimingProbe.h`)를 넘겨 시계 기준점, 실행 중 플레이 시계 읽기, `PlayNow` 앞뒤의 즉시 cue만 관찰합니다.

```powershell
./Tests/Presentation/RunGameplayTimingProbe.ps1 -Configuration Release --pattern "all notes test" --replay --label buzz
./Tests/Presentation/RunGameplayTimingProbe.ps1 -Configuration Release --songs-root <Songs> --pattern "<채보 경로 일부>" --pause-every 4 --pause-count 30 --max-seconds 150 --label pause
```

그 밖의 옵션: `--accelerate <0.5..2>`, `--keep-pitch 0|1`, `--minimize-at <s> --minimize-for <s>`, `--max-seconds <s>`, `--out <폴더>`(기본 `build/timing-probe`).
`--replay`는 모든 노트를 제시간에 누르고 Buzz는 헤드부터 끝까지 누르고 있습니다.

요약(`<out>/<label>/summary.txt`, 옆에 CSV)은 다음을 보고합니다.

- 시계 차이 d: QPC로 얻은 채보 시각에서 함께 읽은 날 mixer 시계(`AudioSystem::DspClock()`)의 채보 시각을 뺀 값을 컨트롤러의 기준점 스냅샷에 대해 실제 ms로,
  mixer 블록마다 첫 읽기를 써서 냅니다. 기준점의 DSP 값은 엔진의 평활 추정값이므로 날값 읽기는 그와 무관하게 mixer를 관찰하며, 평활 추정값이 준비되기 전에
  잡힌 기준점 수도 셉니다. 기준점(시작·Resume·Rewind·Retry)마다, 그리고 20초 이상 구간의 시작·중간·끝 중앙값과 기울기를 냅니다.
- Buzz마다, 구간마다: 틱 시각 대비 틱 소리 요청의 늦음, 오디오 라우팅 시간, Update 빈도와 시간, 주 루프 반복 시간, FPS, Render 시간, 그려진 프레임 사이 최대 간격.
- 5 ms 이상의 주 루프 멈춤을 Update, Render 전 엔진 단계(FMOD 서비스·BeginFrame 대기), Render, 그 뒤 단계(Present·메시지)로 나눕니다.
