# osu! 가져오기

에디터 곡 선택의 Import osu! folder는 osu!taiko 곡 폴더를 YOUMU 곡으로 가져옵니다. `.osu` 하나를 YMM·YMP·YME 세 파일로
바꾸는 일은 별도 저장소의 변환기가 하고(사용자 확정), 나머지는 모두 Client가 합니다. 이 문서는 Client 쪽 책임을 적습니다.
변환 규칙·명령줄·JSON·종료 코드·OD→JudgeLevel 대응·제한은
[변환기 README](https://github.com/4dblackhole/OsuTaikoConverter/blob/main/README.md)가 기준입니다.
화면 흐름(폴더 고르기·난이도 목록 창·실패 대화상자)은 [에디터 곡 선택](EditorSongSelect.md#osutaiko-가져오기)에 있습니다.

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
변환기는 직접 고칠 수 있는 외부 라이브러리처럼 다룹니다(사용자 확정): 독립 저장소로 두되 Client 솔루션이 함께 빌드합니다.

## 저장소와 빌드

`OsuTaikoConverter`는 Client와 같은 부모 폴더에 두는 별도 C++20 저장소입니다
(원격 <https://github.com/4dblackhole/OsuTaikoConverter>, 비공개). Client 폴더에서 처음 한 번 준비합니다.

```powershell
git clone https://github.com/4dblackhole/OsuTaikoConverter.git ../OsuTaikoConverter
```

- Client 솔루션은 변환기의 Core 정적 라이브러리·콘솔 EXE·테스트 프로젝트를 포함합니다. `YOUMU.Client`는 변환기 EXE를
  빌드 순서 의존으로만 참조하고, 같은 구성의 `OsuTaikoConverter.exe`를 자산 루트의 `Submodules/`(저장소 `Client/Assets/Submodules`, 출력 폴더의 `Asset/Submodules`)에 복사합니다. 링크하지 않습니다.
- 변환기는 엔진과 무관한 Chart/Rhythm 라이브러리와 Taiko의 저장 노트 ID·동작 계약만 재사용합니다. 도메인 라이브러리는
  변환기나 Client 실행 프로젝트를 역참조하지 않습니다([전체 구조](../Architecture/Overview.md#저장소와-프로젝트-의존)).
- 에디터 가져오기는 변환기 EXE의 `--output-dir` 지원이 필요하므로 Client와 변환기의 `main`을 함께 갱신합니다.

## Client가 변환기를 실행하는 방법

- `EditorOsuImport`가 백그라운드 worker에서 `<자산 루트>/Submodules/OsuTaikoConverter.exe`를 실행합니다. 경로는 Windows 인자 인용 규칙에 따라
  넘기고 `--output-dir`로 임시 폴더를 줍니다. 렌더 스레드에서 종료를 기다리지 않습니다.
- stdout(JSON)과 stderr를 함께 읽고, 종료 코드 0과 JSON `success`를 확인한 뒤 결과를 검사합니다.
- worker는 Scene·엔진을 참조하지 않으며 KeepAlive Scene이 future를 소유합니다. 취소하면 선택 화면에 남습니다.

## 검사와 배치

- `OsuImportValidation`은 Songs에 넣기 전에 변환기 출력 폴더에서 JSON v1·파일 참조·행 수를 확인하고, 저장된 패턴·효과를
  실제 Taiko 세션으로 읽어 논리 노트 수를 `sourceObjects`와 비교합니다. 단일 레인 겹침처럼 변환기가 보지 않는 플레이 조건도 여기서 걸립니다.
  변환된 JudgeLevel의 실제 판정 범위도 Client 규칙으로 계산됩니다.
- 사용자가 난이도 목록에서 고른 것만 `OsuSongPlacement`가 곡 폴더 이름을 정하고, 같은 음원은 다시 쓰고, 음원·배경을 복사하고,
  Songs를 잠근 채 게시하며 실패하면 되돌립니다. 끝나면 카탈로그를 갱신합니다.
  새 곡 폴더의 이름은 음원 SHA-256 64자리이므로, 제목·아티스트가 같고 속도만 다른 mp3도 서로 다른 곡으로 가져옵니다
  ([음원 식별](../Design/AudioIdentity.md#가져오기)).
- 변환된 YMP는 `HP: 9.00`을 가지며 판정·점수·롱노트 동작은 [Client 판정 규칙](Gameplay/Rules.md)을 따릅니다.
  커스텀 샘플·영상·스토리보드는 가져오지 않고 기본 스킨 히트사운드를 씁니다.

## 검증

- Client: `RunOsuImportValidationTests`(검사·배치), `RunMusicSelectViewTests`(실제 변환기·난이도 목록),
  전체 절차는 [Verification](../Contributing/Verification.md).
- 변환기 자체: `..\OsuTaikoConverter\bin\x64\<구성>\OsuTaikoConverter.Tests.exe`와 변환기 README의 검증 절.
- 실제 맵 복사본과 음원은 변환기 저장소의 git 무시 대상 `TestData/`에만 둡니다. 게임 기본곡이나 원격 저장소에 넣지 않습니다.
