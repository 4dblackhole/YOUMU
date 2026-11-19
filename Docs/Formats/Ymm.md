# YMM 음악 정보

코드: `YOUMU.Chart/Parsing/Submodules/MusicParser.cpp`, `Model/Submodules/MusicDocument.h`.
검증: `Tests/Rhythm/Submodules/SongCatalogTests.cpp`.

UTF-8 텍스트이며 빈 줄과 `//`로 시작하는 줄을 무시합니다. 경로에는 `\\`와 `/`를 사용할 수 있습니다.

## YMM: 음악 메타데이터

YMM은 한 음원에 대한 정보입니다. 섹션 없이 `이름: 값` 형식으로 작성합니다.
[곡 폴더](../Features/Assets.md#곡-폴더-배치)의 `Music`에 하나만 두며 그 곡 폴더의 모든 YMP가 이 YMM에 속합니다.
파일 이름은 `<첫 아티스트> - <첫 곡명>.ymm`이 관례이고 이름으로 구분하지는 않습니다.

```text
Version: 1
File: angel dream hand shaking.mp3

Romanized Music Name:
Music Name Count: 1
Music Name 1: Angel Dream Handshaking

Romanized Artist:
Artist Count: 1
Artist Composer: Takahashi yoko

Tags: taiko no tatsujin,test
```

| 필드 | 의미 |
| --- | --- |
| `Version` | 정수 형식 버전입니다. 생략하면 1입니다. |
| `File` | 필수 음원 경로입니다. YMM 파일이 있는 폴더를 기준으로 해석합니다. |
| `Romanized Music Name` | 곡명의 로마자 표기 한 줄입니다. 곡명 묶음의 맨 윗줄에 두며 비워 둘 수 있습니다. |
| `Music Name ...` | 한 개 이상의 표시 곡명입니다. `Music Name Count` 자체는 개수 안내용이며 현재 파서는 사용하지 않습니다. |
| `Romanized Artist` | 아티스트의 로마자 표기 한 줄입니다. 아티스트 묶음의 맨 윗줄에 두며 비워 둘 수 있습니다. |
| `Artist ...` | 한 개 이상의 표시 아티스트입니다. `Artist Count` 자체는 현재 파서가 사용하지 않습니다. `Artist Vocal Romanized:`처럼 `Artist`로 시작하는 줄은 모두 아티스트 표기입니다. |
| `Tags` | 쉼표로 구분한 태그입니다. |

## 로마자 줄과 표시

곡명·아티스트 묶음마다 맨 윗줄이 로마자 전용 줄입니다(사용자 확정). 화면에 무엇을 보일지는 옵션의
`Song text`(Option.ini `[Display] Notation=first|romanized`)가 정합니다.

- `First notation`: 묶음의 첫 번째 비어 있지 않은 표기.
- `Romanized`: 로마자 줄. 비어 있으면 묶음에서 이미 로마자인 첫 표기가 대신하고, 그런 표기도 없으면 첫 표기.

"이미 로마자"는 모든 글자가 U+0280 미만(영문·숫자·ASCII 기호·악센트 붙은 라틴 문자)인 글입니다.
그래서 첫 표기가 이미 로마자인 곡은 로마자 줄을 비워 둬도 로마자 표시에서 그대로 보입니다.
규칙은 `Model/Submodules/NotationText.h`가 소유하고 YMP의 난이도명·제작자도 같은 방식입니다.
로마자 줄은 어느 표시에서나 검색 대상이며, 곡명·아티스트 정렬은 표시 중인 표기를 따릅니다.

`File`이 없으면
파싱 오류이며, `Version`이 정수가 아니거나 실제 음원 파일이 없으면 catalog
검증에 실패합니다. 손상된 YMM 하나는 catalog 전체를 중단시키지 않고 해당 곡만
제외합니다. `Music`에 YMM이 없거나 둘 이상인 곡 폴더도 오류와 함께 그 곡만 제외합니다.
곡 ID나 해시 항목은 없습니다.
