# YOUMU 작업 규칙

이 저장소에 적용합니다. `Dependencies/PhantomCore`는 엔진 저장소의 규칙을 따릅니다.

## 시작과 범위

- 지금 상태는 `Docs/Status.md`, 일하는 절차(역할·브랜치·PR·병합 요청·검증 잠금)는
  `Docs/Contributing/Workflow.md`에서 먼저 확인합니다.
- 기능을 바꾸기 전에 `Docs/Design/Principles.md`와 `Docs/Design/Decisions.md`의 관련 ID를 읽고,
  이어서 `Docs/README.md`에서 해당 기능 문서를 찾습니다. 소스·테스트 소유는 `Docs/Contributing/ChangeImpactMap.md`입니다.
  선언과 직접 호출부부터 읽고 검색을 넓힙니다. 모든 문서나 SDK를 읽지 않습니다.
- 시작·화면 전환·수명을 바꿀 때는 `Docs/Architecture/ExecutionFlow.md`, Canvas·입력·렌더링 작업은 Visual2D 문서,
  엔진 API·소유권 작업일 때만 엔진의 `Docs/EngineOverview.md`를 읽습니다.
- 요청한 동작만 구현합니다. 기존 소유와 인터페이스를 재사용하고 리팩터링은 국소적으로 합니다.
  기능·데이터·화면·추상화를 짐작해 만들지 않습니다.
- 버그 수정·최적화·리팩터링·문서 정리는 다른 확정 설계를 바꿀 근거가 아닙니다. 먼저 그 결정 ID의 조건·예외·
  회귀 사례를 확인합니다. 코드와 테스트는 구현의 증거일 뿐 확정 요구 사항을 뒤집는 근거가 아닙니다.
  사용자가 명시적으로 바꾸라고 한 것은 이미 승인된 것이므로 같은 결정을 다시 묻지 않습니다.
- 합리적인 해석에 따라 동작·소유·API·파일 형식·설계·외부 상태가 크게 달라지면 그 부분은 구현 전에 묻습니다.
  명확한 다른 작업은 계속할 수 있습니다. 작은 선택은 되돌리기 쉬운 가장 작은 변경으로 하고 가정을 보고에 적습니다.
  제안은 요구 사항이 아닙니다.
- 실제 실행 데이터를 쓰고, 데이터가 없을 때는 빈 상태를 명시적으로 보여 줍니다.
- 내장 파일·명령 도구를 씁니다. Chat On Steroids는 쓰지 않습니다.

## 소유와 구현

- 이 저장소는 게임 코드와 자산을 소유합니다. 엔진 변경은 분리된 서브모듈 체크아웃이 아니라 독립 PhantomCore
  저장소에서 합니다. Client의 엔진 include·참조는 `PhantomCore.h`와 `PhantomCore.vcxproj`뿐입니다.
- 도메인 라이브러리는 Client·엔진 구현에 의존하지 않습니다. YMP는 플레이할 채보 데이터, YME는 시각·소리 자동화를
  소유합니다. 게임 모드는 `IPlayGameMode`를 구현하고 채보에서 노트를 만드는 일을 소유합니다.
- 제품·도메인 공개 타입은 전역 범위에 둡니다. 익명·내부 범위, 충돌을 피하는 UI 상수, 서드파티 namespace는 필요한 곳에
  유지합니다. 엔진 이름은 PhantomCore 소유입니다. 헤더에서는 한정해 쓰고, 구현 파일은 include 뒤에서만
  `using namespace PhantomCore`를 쓸 수 있습니다.
- 변환기 구현은 독립 OsuTaikoConverter 저장소 소유이며, 직접 고칠 수 있는 외부 라이브러리로 다룹니다. 변환기는 파일 형식과
  Taiko 노트 계약을 쓰고 `.osu` 파일 하나를 YMM/YMP/YME로 바꾸는 일만 합니다. 가져오기의 나머지는 Client 소유입니다:
  `OsuImportValidation`이 실제 플레이 세션으로 결과를 검사하고 `OsuSongPlacement`가 이름을 정해 Songs에 복사·잠금·게시합니다.
- Scene ID는 `Client/GameFlow/YOUMUSceneIds.h`에 둡니다. 팩토리는 유지 방식을 명시해 등록하고 지연된 `ChangeScene`을 씁니다.
  플레이 Scene은 `DestroyOnExit`이며 플레이 세션 밖으로는 결과 데이터만 나갑니다.
- Win32 Virtual-Key 값과 순서가 보장된 Raw Input QPC 시각을 씁니다. 세션마다 `RhythmTimer` 하나가 입력과 DSP 시간을
  변환하고, 일시정지 뒤에는 DSP 기준을 다시 잡습니다. 렌더링 시각은 판정 근거가 아닙니다. 템포 위치는 프레임마다 적분하지 않고
  로드할 때 정수 마이크로초로 컴파일합니다.
- 노트는 백엔드와 무관한 `NoteEvent`/`AudioCueRequest`를 내고, Client 오디오 라우팅이 클립·버스·효과로 연결합니다.
  게임 전용 UI는 Client에 둡니다.
- 곡 선택은 `SongCatalog`를 쓰고, 곡 폴더 배치(`SongLayout`: YMP는 그것이 저장된 곡 폴더의 YMM에 속함)와 제목·아티스트
  목록 행을 유지하며, 들어갈 때 복사한 `GameplayLaunchRequest`를 넘깁니다.
- 레인 요소는 한 부모 아래에 기본 로컬 방향으로 둡니다. 모드에 따라 자식을 화면 좌표로 하나씩 옮기지 않고 부모를 회전합니다.
- Scene은 책임을 소유한 클래스를 작은 호출로 조합합니다. 기능의 헤더·소스를 함께 두고, Scene의 `Submodules/`와 실제 폴더·
  필터 트리를 맞춥니다. 구체적인 계약을 include하고 구현 `.cpp`나 호환용 파사드는 include하지 않습니다.
  파일 나누기로 소유 분리를 대신하지 않습니다.
- 객체 사이의 참조는 트리 모양으로 두고 순환시키지 않습니다. 필요 없는 동적 할당을 하지 않습니다. enum 값에 따라 동작이
  갈리면 `switch`를 늘리지 않고 인터페이스·상속으로 구현을 나눕니다(`Docs/Design/Principles.md`의 코드 구조 원칙).
- 구별되는 동작은 이름 있는 도우미로 뽑습니다. 길지만 응집된 함수는 코드를 되풀이하지 말고 의미 있는 단계와 필요한 순서를 설명합니다.
- `if`/`while` 조건 하나에 `&&`/`||`는 두 개까지입니다. 더 길면 조기 반환, 이름 붙인 `bool`, 술어로 나누고
  평가 순서와 부작용을 유지합니다(`Scripts/Verify/count_conditions.py`·`count_expressions.py`가 0이어야 함).
- FMOD SDK·import library·런타임 DLL은 커밋하지 않습니다. 재배포하는 글꼴 옆에 라이선스를 두고 서드파티 고지를 보존합니다.

## 검증과 문서

- 바꾼 코드와 직접 호출부의 정확성·수명·오류 처리·경계·변환·동시성·중복·읽기 쉬운 책임을 검토합니다.
- `Docs/Contributing/Verification.md`를 따릅니다. 코드 변경은 Debug/Release x64 솔루션 빌드, 로직·카탈로그 테스트, 구성마다 smoke 4종,
  관련 회귀, 영향 받는 Client 소유 C++ 프로젝트의 MSVC Code Analysis 또는 clang-tidy(실제 include·define·C++ 설정)가
  필요합니다. 파싱 오류는 실패입니다. `Scripts/Verify`의 스크립트가 이 절차와 공유 잠금을 처리합니다.
- 코드·프로젝트 변경은 `Scripts/CheckArchitecture.ps1`, 모든 변경은 `git diff --check`를 실행합니다.
  문서만 바꾸면 C++ 빌드 대신 diff·링크 검사(`Scripts/Verify/check_links.py`)를 합니다.
- 새 진단은 고치고 기존 지적은 따로 보고합니다. 검사를 통과하려고 경고를 일괄 수정하거나 억제하지 않습니다.
  엔진·벤더·생성 코드는 Client 수정 범위 밖입니다. 쓸 수 없던 도구와 실제로 한 검사를 보고합니다.
- 바뀐 책임·영향·검증·한계를 보고합니다. 자동 검사와 사람이 한 화면·입력·청음 확인을 구분합니다.
- 영구 계약은 해당 기능 문서에 두고, 흐름이나 조작이 바뀌면 README·색인 링크를 고칩니다. 날짜별 완료 기록, 세션의
  미커밋 파일 목록, 대체된 조사 결과를 입문·검증 문서에 덧붙이지 않습니다. 지금 상태는 `Docs/Status.md`만 고칩니다.
- 문서를 합칠 때 결정 ID, 근거, 구현 상태, 조건, 예외, 값·단위, 이유, 회귀 링크를 보존합니다. 승인된 설계 변경은 결정과
  기능 계약을 함께 고칩니다. 제안이나 보류된 HP 작업을 채택·구현된 기능으로 올리지 않습니다.

## Git

- 작업은 최신 `origin/main`에서 만든 작업 브랜치(`<세션>/<주제>`)와 자기 워크트리에서 합니다. 주 체크아웃은 읽기만 하고,
  다른 세션의 폴더·브랜치·잠금 파일은 건드리지 않습니다. 관련 없는 변경은 보존합니다.
- 검증한 뒤 브랜치를 push하고 PR을 열고, 지휘 세션에 병합 요청을 보냅니다(형식은 `Docs/Contributing/Workflow.md`).
  main에 직접 커밋·push하지 않고 자기 PR을 병합하지 않습니다.
- 지휘 세션은 병합 묶음을 `Scripts/Verify/VerifyAll.ps1`로 검증한 뒤 main에 병합·push하고, PR이 MERGED가 된 뒤에
  원격 작업 브랜치를 지우며, `Docs/Status.md`를 갱신합니다.
- 엔진 변경은 엔진 저장소 PR을 먼저 병합하고, YOUMU PR에서 Client gitlink를 그 커밋으로 올립니다.
  `git ls-files -s Dependencies/PhantomCore`가 서브모듈 HEAD와 같은지 확인합니다. 병합 뒤에는 main을 fast-forward하고
  서브모듈을 재귀적으로 sync/update합니다.
- 검사·보호 규칙·충돌을 우회하지 않고, 필요한 PR이 열려 있는 동안 동기화를 마쳤다고 하지 않습니다.
