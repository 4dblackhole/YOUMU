# 에디터 곡 선택 화면

타이틀의 `Editor` 버튼은 `YOUMU.EditorSongSelect` Scene으로 이동합니다.
이 화면은 별도의 곡 목록 구현을 복제하지 않고 `MusicSelectScene`을
`SongSelectPurpose::Editor`로 생성합니다.

## 공유하는 동작

- `SongCatalog`에서 실제 YMM/YMP 관계를 로드합니다.
- 폴더 탭, 검색 필드, 곡 정렬 콤보박스, 곡 카드와 펼쳐진 난이도 목록은
  일반 곡 선택 화면과 같습니다.
- 좌우키는 곡, 상하키는 난이도를 이동하며 마우스 선택과 미리듣기도 같습니다.
- Back 또는 Escape는 타이틀로 돌아갑니다.

곡 목록·선택 상태·미리듣기는 게임 곡 선택·타이틀 플레이어가 함께 쓰는 것과 별개인 자기 `SongLibrary`입니다.
타이틀에서 듣던 곡을 이어받지 않고(타이틀은 이 화면으로 올 때 소리를 멈춤) 이 화면의 선택 곡을 새로 재생하며,
나갈 때는 타이틀로 돌아가더라도 소리를 멈춥니다([곡 선택](SongSelect.md#미리듣기와-타이틀-플레이어)).

## 에디터 전용 배치

[UI-02](DesignDecisions.md#ui-02-ui-리디자인) 설계대로 왼쪽 기록 열을 만들지 않고 그 자리는 BG가 보입니다.
곡 정보와 곡 목록은 게임 곡 선택과 같은 자리·같은 모양이고 같은 면(x 362부터 화면 오른쪽 끝) 위에 놓입니다. 하단 띠 가운데에는 옵션 톱니 하나만 있고
게임 모드 선택과 `Play options`는 없습니다. 곡 카드 오른쪽의 `+`가 유일한 에디터 전용 부품입니다.
패턴이 없는 곡의 `+`에 두는 금빛 밑줄은 아직 없습니다.

선택된 난이도 행을 누르거나 GO, Enter 또는 Space로 확정하면 선택 경로를 기록하고
`YOUMU.Editor` Scene으로 이동합니다. 악보·실시간 뷰와 타이밍·메타데이터·
이펙트 편집, YMP/YME 저장을 제공합니다. Escape는 미저장 변경의 폐기 여부를
확인한 뒤 에디터 곡 선택으로 돌아갑니다. 다시 진입한 곡 선택은 저장 결과를
카탈로그에서 다시 읽습니다. 자세한 조작은 [ChartEditor.md](ChartEditor.md)를 참고합니다.

## 새 난이도 생성

에디터 곡 카드의 맨 오른쪽 `+`는 `New Difficulty`와 `Convert .osu` 두 항목의 팝업을 엽니다. 팝업 밖 클릭이나 Escape는
팝업만 닫으며 뒤 화면을 실행하지 않습니다.
New Difficulty는 입력 창 없이 에디터를 그 곡의 [만들기 전 상태](ChartEditor.md#새-난이도)로 엽니다(Metadata 탭, `New` 표시,
저장 버튼은 Create, Compose·Audio 잠김). Taiko·120 BPM·offset 0ms·4/4이며 노트·타이밍 명령·YME 효과는 비어 있습니다.
Metadata에서 난이도명, 제작자, 태그, Accuracy Difficulty(JudgeLevel 1.00~100.00, 0.01 단위, 기본 50.00), HP, 기본 BPM과
offset, BG를 입력하고 Create를 누르면 파일을 만든 뒤 같은 에디터에서 이어서 편집합니다. 곡 자체의 YMM과 음원은
공유하고 수정하지 않습니다. Create 전에 나가면 파일을 만들지 않습니다.

파일은 고른 곡의 [곡 폴더](BuiltInAssets.md#곡-폴더-배치)에
`Chart/<첫 제작자> - <난이도명>/<YMM 이름> [<난이도명>].ymp/.yme`로 만듭니다. 제작자를 비워 두면
`Unknown`이고 제작자 이름의 파일명에 쓸 수 없는 문자는 `_`입니다. YMP에는 YMM 참조를 적지 않으며
YME 참조는 같은 폴더 기준입니다. YMM이 곡 폴더의 `Music`에 있지 않은 곡에는 만들 수 없습니다.
난이도명의 파일명 예약문자·경로·빈 이름은 거부하며 기존 차트 세트 폴더는 빈 폴더여도 덮어쓰지 않습니다.
이름·폴더 문제는 에디터 상태 문구에 이유를 두고, 그 밖의 쓰기 실패는 `Create failed` 경고 대화상자로 알립니다.
어느 쪽이든 입력한 값은 그대로입니다.

`MusicSelectView`는 생성 의도만 반환하고, Scene은 곡 항목을 `GameplayLaunchRequest::editorNewDifficulty`에 넣어
deferred Editor 전환을 수행합니다. 모드의 `NewDocument`·`CreateDocument`가 초안과 Create를,
`EditorScene/Submodules/EditorPatternCreation`이 형식 검증·경로·새 파일 쓰기/실패 정리를 소유합니다.
재진입한 곡 선택은 카탈로그에서 새 난이도를 다시 읽습니다.

## osu!taiko 가져오기

`Convert .osu`는 `.osu` 파일 선택 대화창을 엽니다. 선택 취소는 파일을 만들지 않습니다.
누른 카드와 다른 곡도 가져올 수 있으며 원본의 `Artist`와 `Title`(로마자), `Version`을
사용합니다. 현재 변환기는 native Taiko(`Mode: 1`)만 지원합니다.

새 파일의 배치는 아래와 같습니다. 음원은 재인코딩하지 않고 원본 확장자로 복사합니다.
파일명에 쓸 수 없는 문자는 `_`로 바꾸고 끝의 공백/마침표를 제거하며 예약 이름은 `_`를 붙입니다.

```text
Songs/
  <Artist - Title>/
    Music/
      <Artist - Title>.mp3
      <Artist - Title>.ymm
    Chart/<Creator - Version>/
      <Artist - Title> [<Version>].ymp
      <Artist - Title> [<Version>].yme
    BG/<원본 배경 파일명>
```

각 곡 폴더의 YMM이 참조하는 음원을 크기와 실제 바이트로 비교합니다. 내용이 같으면
기존 음원/YMM을 재사용하고 그 경로와 메타데이터는 변경하지 않으며, 새 난이도와 배경은 그 곡 폴더에
넣습니다. 파일명/태그가 같아도 내용이 다르면 재사용하지 않습니다. 같은 이름 곡 폴더의 YMM이 다른 음원을
참조하거나 차트 세트 폴더가 이미 있으면 오류를 표시하며 덮어쓰지 않습니다.
변환기는 임시 폴더에서 포맷·저장 검사를 수행한 뒤 음악과 완성된 YMP/YME 쌍을 공개합니다.
Client의 OsuImportValidation이 반환 JSON v1과 저장 파일의 곡 폴더 위치·참조·행 수를 확인하고 실제 모드 세션을
구성하여 원본 객체 수와 생성된 플레이 노트 수를 비교합니다. 실패는 공용 [경고 대화상자](TextManagement.md#경고-대화상자)
(`Conversion failed`, 진단 상자에 변환기 출력, OK)로 표시하며
기존/공유 Songs 파일은 삭제하지 않습니다. 성공한 결과만 목록을 갱신합니다.

변환이 끝나면 완료 안내 없이 곡 목록에 추가합니다. 변환하는 동안 곡 카드는 잠기며 따로 진행 표시는 없습니다.
성공했을 때의 변환기 출력은 표시하지 않습니다. 편집기는 목록에서 난이도를 선택하여 엽니다. 원본 `.osu`와 음원은 수정하지 않습니다.
`EditorOsuImport`가 파일 대화창과 백그라운드 자식 프로세스/결과 수명을 소유합니다.
Scene은 완료 결과에 따라 카탈로그만 갱신하며 프레임에서 변환/음원 비교를 기다리지 않습니다.
worker는 복사한 경로만 사용하고 프로그램 종료 시 진행 중인 작업이 끝날 때까지 기다릴 수 있습니다.
변환기 저장 규칙과 CLI는 [OsuTaikoConverter](OsuTaikoConverter.md)를 참고합니다.
