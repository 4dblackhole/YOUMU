# osu!taiko 변환기

변환기는 `.osu` 파일 하나를 같은 이름의 YMM·YMP·YME 세 파일로 바꾸는 일만 합니다(사용자 확정).
음원·배경 복사, 곡 폴더 이름과 배치, 같은 음원 재사용, Songs 잠금, 게시·되돌리기, 플레이 검증은 모두
Client가 맡습니다([책임 경계](#책임-경계)).

`OsuTaikoConverter`는 Client와 같은 부모 폴더에 두는 별도 C++20 저장소입니다.
원격: <https://github.com/4dblackhole/OsuTaikoConverter> (비공개).

```text
부모 폴더/
  YOUMU/
  OsuTaikoConverter/
```

Client 폴더에서 최초 한 번 다음 명령으로 준비합니다.

```powershell
git clone https://github.com/4dblackhole/OsuTaikoConverter.git ../OsuTaikoConverter
```

이미 준비한 작업 폴더는 Client와 변환기 양쪽 `main`을 갱신한 뒤 빌드합니다.
에디터 가져오기는 변환기 EXE의 `--output-dir` 지원이 필요하므로 두 저장소를 함께 갱신합니다.

## 책임 경계

| 일 | 변환기 | Client |
| --- | --- | --- |
| `.osu` → YMM·YMP·YME 내용 변환, 격자·시간·직렬화 왕복 검사 | ○ | |
| 세 파일 쓰기(`.osu` 옆 또는 `--output-dir`), 기존 파일을 덮어쓰지 않기 | ○ | |
| osu! 곡 폴더 고르기, 폴더 안 `.osu`마다 변환기 실행(임시 폴더) | | ○ `EditorOsuImport` |
| JSON v1·파일 참조·행 수·실제 모드 세션의 노트 수 검사 | | ○ `OsuImportValidation` |
| 가져올 난이도 고르기 | | ○ `OsuImportPanel` |
| 곡 폴더·파일 이름, 같은 음원 재사용, 음원·배경 복사, Songs 잠금, 게시·되돌리기 | | ○ `OsuSongPlacement` |

변환기는 음원과 배경 파일을 열지도 복사하지도 않고, Songs 폴더와 곡 폴더 배치(`SongLayout`)를 모릅니다.
변환기는 직접 고칠 수 있는 외부 라이브러리처럼 다룹니다(사용자 확정): 독립 저장소로 두되 Client 솔루션이
함께 빌드하고 EXE를 게임 옆에 복사합니다.

## 빌드와 책임

Client 솔루션은 변환기의 Core 정적 라이브러리, 콘솔 EXE, 테스트 프로젝트를
포함합니다. `YOUMU.Client`는 변환기 EXE를 빌드 의존성으로 참조하고 같은 구성의
`OsuTaikoConverter.exe`를 `Y's Orchestral Unison - Melodic Ultimatum.exe` 옆에 복사합니다. EXE를 링크하지 않습니다.

변환기는 엔진 비종속 Chart/Rhythm 라이브러리와 Taiko의 저장 노트 ID·동작 계약만 재사용합니다.
도메인 라이브러리가 변환기나 Client 실행 프로젝트를 역참조하지 않습니다.
공통 YMP writer를 사용해 `[Difficulty]`의 JudgeLevel 바로 아래 `HP: 9.00`을 출력합니다.
osu! HPDrainRate는 매핑하지 않으며 JudgeLevel 변환 규칙은 유지합니다. 에디터 곡 카드 +의
Import osu! folder는 Client의 EditorOsuImport가 EXE를 백그라운드에서 실행합니다.
선택/완료 UI는 [EditorSongSelect](EditorSongSelect.md)가 설명합니다.

- `Core/Submodules/OsuDocument`: 원본 문법, 수치·모드 검증.
- `Core/Submodules/TimingMap`: BPM 기준점, 최근접 틱, omit 마디 길이.
- `Core/Submodules/OutputWriting`: BPM·속도 반올림과 소수점 셋째 자리까지의 파일 출력.
- `Core/Converter`: 노트/효과 대응, 파서 저장 왕복과 선택한 틱의 시간 정밀도 검증.
  실제 Taiko 세션 생성·노트 수·판정 범위 검증은 Client가 담당합니다.
- `Core/ConversionFiles`: `.osu` 읽기와 YMM·YMP·YME 쓰기.
- 배경 이미지는 복사하지 않습니다. YMP의 `BG:`에는 `.osu`가 적은 상대 경로를 그대로 적고,
  `offset:`은 osu!의 `(x, y)`를 1280×720/엔진 2D 좌표의 `(1.5*x, -1.5*y)`로 변환합니다.
- `App/Main`: UTF-16 Windows 인자, UTF-8 JSON stdout, stderr/종료 코드.

## 곡명·아티스트 표기

YMM의 첫 표기는 osu!의 `TitleUnicode`·`ArtistUnicode`(없으면 `Title`·`Artist`)입니다. osu!의 로마자
`Title`·`Artist`는 첫 표기와 다를 때만 `Romanized Music Name`·`Romanized Artist` 줄에 적고, 같으면 그 줄을
비워 둡니다. osu!에는 난이도명·제작자의 로마자 표기가 없으므로 YMP의 두 로마자 줄은 비어 있습니다.
줄의 뜻과 표시 규칙은 [YMM](../Formats/Ymm.md#로마자-줄과-표시)에 있습니다.

## OD와 JudgeLevel

osu!taiko의 100/OK 판정 폭에 가장 가까운 YOUMU GOOD 판정 폭을 갖도록 YMP의
`JudgeLevel`을 설정합니다. 기존 1.00..100.00 범위와 0.01 단위를 유지합니다.
목표 반폭은 `floor(OD <= 5 ? 120 - 8*OD : 110 - 6*OD) - 0.5` ms이며,
GOOD 반폭 `54.5 * 50 / JudgeLevel`을 실제 마이크로초로 반올림한 결과를 비교합니다.
계산은 공식 DifficultyRange의 부동소수 연산 순서를 따릅니다. 오차가 같으면 역산
JudgeLevel에 가까운 값을, 그것도 같으면 더 작은 값을 선택합니다.
기존 정밀도 때문에 완전히 같은 값이 없을 수 있습니다. 예를 들어 OD 7은
`JudgeLevel: 40.37`, GOOD ±67.501ms로 원본 목표 ±67.500ms보다 1µs 넓습니다.
다른 판정 등급과 점수·롱노트 규칙은 YOUMU의 기존 규칙을 사용합니다.
OD는 스피너를 DengDeng으로 바꿀 때의 요구 타격 횟수 계산에도 사용합니다.

판정 원본은 [osu! 공식 TaikoHitWindows](https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Taiko/Scoring/TaikoHitWindows.cs)를
기준으로 하며, 변환 후 저장된 JudgeLevel의 실제 범위 검증은 Client에서 수행합니다.

## 실행 계약

```powershell
.\bin\x64\Release\OsuTaikoConverter.exe --input "D:\Maps\song\map [Oni].osu"
.\bin\x64\Release\OsuTaikoConverter.exe --input "D:\Maps\song\map [Oni].osu" --output-dir "D:\Out" --extra-divisors "40,48"
```

필수 인자는 `--input` 하나입니다. 출력은 입력과 같은 이름의 `map [Oni].ymm`, `map [Oni].ymp`,
`map [Oni].yme` 세 파일이며, `--output-dir`가 없으면 `.osu` 옆에, 있으면 그 폴더에 씁니다(없으면 만듭니다).
상대 경로는 프로세스의 작업 폴더 기준입니다. 이 세 파일 말고는 아무것도 만들거나 바꾸지 않습니다.

세 파일 중 하나라도 이미 있으면 아무것도 쓰지 않고 실패하며(종료 코드 5) 덮어쓰기 옵션은 없습니다.
그 밖의 실패에서는 이번 실행이 쓴 파일을 지워 일부만 남기지 않습니다.

YMM의 `File`과 YMP의 `BG`에는 `.osu`가 적은 경로(`AudioFilename`, 배경 이벤트)를 `.osu` 폴더 기준 그대로
적습니다. 출력 폴더가 달라도 같습니다. 변환기는 그 파일을 열지 않으므로 없어도 변환됩니다. 다만 두 경로는
맵 폴더 안의 안전한 상대 경로여야 하며 아니면 종료 코드 4입니다. YMP의 `Effect file`은 같은 폴더의 YME입니다.
YMP에는 YMM 참조를 적지 않습니다(곡 폴더 위치로 결정).

stdout은 JSON 객체 한 개입니다(`version: 1`, `success`, 생성 경로 `files.ymm/ymp/yme`, 원본 노트 수,
출력 행 수, 격자 분모, 보정 이벤트 수/최대 이동 ms, 경고). 실패 시에는
`code`, `line`(원본 행, 해당하면), `error`가 들어갑니다. 사람이 읽는 오류는 stderr에도
출력합니다. `--help`는 예외적으로 도움말 텍스트를 출력합니다.

| 종료 코드 | 의미 |
| --- | --- |
| 0 | 변환 완료 |
| 2 | 인자/격자 오류 |
| 3 | 미지원 모드·데이터·범위 |
| 4 | 원본 파일/문법/참조 경로 오류 |
| 5 | 출력 충돌·파일 시스템 오류 |
| 6 | 저장 왕복·시간 검증 실패 |

Client는 EXE 경로를 직접 지정하고 Windows 인자 인용 규칙에 따라 경로를 전달합니다. 렌더 스레드에서
종료를 기다리지 않고 stdout/stderr를 함께 소비하며, exit 0과 JSON success를 확인한 뒤 결과를 검사합니다.
Client가 osu! 곡 폴더를 받아 Songs에 넣는 과정은 [에디터 가져오기](EditorSongSelect.md#osutaiko-가져오기)가 설명합니다.

## 확정된 변환 규칙

- `Mode: 1`인 native Taiko만 지원합니다. 현재 입력 형식 범위는 osu! v5~v14입니다.
- 출력 `Pattern Offset`은 **첫 빨간 타이밍포인트 ms + 25ms**입니다. 원본 시각으로
  격자·마디를 계산한 뒤 한 번만 적용하여 노트·롱노트 끝점·BPM·효과가 함께 뒤로 이동합니다.
  음수/소수 오프셋과 모든 지원 음원 형식에 동일하게 적용하며 음원은 읽지도 바꾸지도
  않습니다. 박자 위치·길이·스냅 통계는 유지합니다. 예: 727 → 752ms, -500 → -475ms.
  기존 변환 파일은 자동 수정하지 않습니다.
- 격자 `1/N`은 **4분음표 한 박을 N등분**합니다. 기본 분모는 1~32의 모든 정수이고,
  `--extra-divisors`로 1~1024 범위의 정수 분모를 추가합니다. 중복은 제거합니다.
- 노트 시작/끝과 효과 시각은 시간상 가장 가까운 틱으로 이동합니다. 최대 허용 오차는
  없고 동률이면 앞 틱을 택합니다. BPM 경계 양쪽의 유효한 후보도 비교합니다.
- Base BPM·변경 BPM·최종 스크롤 속도 배율은 소수점 넷째 자리에서 반올림하여
  셋째 자리까지만 기록합니다. 끝의 불필요한 0은 생략합니다. BPM 반올림은 격자와
  마디 위치 계산에도 적용하여 저장된 BPM과 일치시키며, BPM 반올림 자체는 빨간 포인트의
  원본 ms를 바꾸지 않습니다. 전체 재생 시각의 +25ms 보정은 이 계산 뒤에 적용합니다.
  슬라이더 원본 길이는 반올림 전 값으로 계산한 뒤 격자에 맞춥니다.
  SV·BPM 비율 계산은 정밀도를 유지하고 최종 속도 배율만 반올림합니다.
  속도가 0으로 반올림되면 Client의 양수 속도 조건에 따라 변환을 거부합니다.
- 빨간 포인트는 원본 시각의 BPM·격자 기준점입니다. omit이 꺼진 중간 빨간 포인트는
  앞 마디를 짧게 종료합니다. omit이 켜지면 앞 부분과 새 박자표의 한 마디를 합칩니다.
  예: 2.5박 뒤 3/4 + omit이면 5.5박(`11/8`) 마디, 이후 3박 마디입니다.
  격자는 합친 마디 시작이 아닌 새 빨간 포인트에서 다시 시작합니다.
- YMP/YME 저장 위치는 기존대로 **온음표 기준**입니다. 격자 한 박은 파일의 `1/4`입니다.
- Don/Kat/큰 노트는 hitsound 비트로 구분합니다. 모든 슬라이더는 TickRoll 또는
  BigTickRoll, 스피너는 DengDeng입니다. 슬라이더 종료는 head의 BPM/SV/길이/반복 횟수로
  계산하고 시작·끝을 각각 보정합니다. tick rate 3은 `TickDivision=12`, 그 외는 16입니다.
- DengDeng의 HitCount는 원본 지속 시간과 osu! OD의 요구 입력률에서 계산합니다.
  판정 레벨은 Client 기본값이며 osu! 점수/판정창을 복제하지 않습니다.
- 스크롤은 Separate이며 `(SliderMultiplier / 1.4) × SV × (현재 BPM / Base BPM)`입니다.
  SV는 Taiko의 0.01~10 범위를 적용하며 빨간 포인트에서 1로 초기화합니다.
  같은 원본 시각에서는 빨간 포인트 적용 후 초록 포인트가 상태를 덮어씁니다.
- TimingPoints 볼륨은 HitSound/TickSound/UserInputFeedback에 `volume / 100`으로 기록합니다.
  Kiai는 YME ON/OFF 상태로 기록합니다. 현재 Client의 Kiai는 상태만 제공하고 연출은 없습니다.
- 효과는 종류·대상별 현재 값과 비교해 변화가 있을 때만 기록합니다. 초기 속도 1·볼륨
  100%·Kiai OFF를 다시 지정하는 명령도 생략합니다. 여러 원본 포인트가 같은 틱으로
  보정되면 각 대상의 마지막 상태만 비교합니다. 60%→80%→60%처럼 실제로 되돌아가는
  변경은 유지합니다. 볼륨 세 대상은 서로 다른 사운드이므로 각각 기록합니다.

## 제한과 검증

커스텀 샘플 뱅크/파일·노트별 샘플 override, 영상·스토리보드·break는 내보내지
않으며 JSON 경고로 알립니다. 기본 Client 스킨 히트사운드를 사용합니다.
판정·점수·틱 끝점·롱노트 입력 동작은 [Client 판정 규칙](Gameplay/Rules.md)이 설명을 소유합니다.
변환기의 README와 JSON 안내도 Client `main`의 같은 문서를 링크하며 세부 플레이 규칙을
별도로 복제하지 않습니다. 변환기에는 객체 대응과 내보내는 파라미터의 설명만 유지합니다.
osu! 판정/점수를 재현하는 변환은 아니며, 원작과 다른 대응이나 미지원 데이터는 경고로 알립니다.

스냅 후 같은 위치의 일반 노트는 유지합니다. 롱노트 길이가 0이 되거나 기존 단일
Taiko lane이 노트를 누락시킬 겹침은 실패합니다. 첫 빨간 포인트 이전 노트,
24시간 범위 밖 시각, 100,000마디 초과, 1024회 초과 DengDeng은 미지원입니다.
원본 숫자는 유한해야 하며 입력 크기/객체 수 제한도 검사합니다.

빨간 기준점은 온음표 `1e-9` 단위로 수치 표현합니다(음악 격자 보정과 별개).
선택된 틱과 기존 MusicalTimeline이 복원한 시각의 차이는 최대 2µs여야 합니다.
변환기는 저장 왕복과 파서 진단까지 확인합니다. 실제 Taiko 세션의 생성과 원본 객체 대비
플레이 노트 개수 검증은 Client의 OsuImportValidation이 변환 결과 폴더에서, Songs에 배치하기 전에 수행합니다.
변환기 테스트는 `..\OsuTaikoConverter\bin\x64\<구성>\OsuTaikoConverter.Tests.exe`입니다.
전체 Client 검증 명령은 [Verification](../Contributing/Verification.md)을 사용합니다.

실제 맵 복사본과 음원은 변환기 저장소의 gitignore 대상 `TestData/`에만 둡니다.
게임 기본곡이나 원격 저장소에 추가하지 않습니다.
