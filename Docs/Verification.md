# 빌드와 검증

이 문서는 재현 가능한 현재 절차입니다. 날짜별 완료 로그는 Git 이력에서 확인합니다.
문서만 바꾸면 로컬 링크·앵커와 `git diff --check`를 검사하며 C++ 빌드는 필요하지 않습니다.

검사 결과는 [관련 설계 결정](DesignDecisions.md)의 조건·예외·회귀 사례와 대조합니다.
통과한 코드/테스트가 확정 설계와 다르면 차이를 먼저 확인하며, 테스트 통과 자체를
설계 변경 근거로 사용하지 않습니다. 명시적으로 승인된 변경은 결정과 기능 문서도 함께 갱신합니다.

## 환경과 필수 검사

Visual Studio 2022/v143(C++ 데스크톱), Windows SDK, FMOD SDK, `../OsuTaikoConverter` checkout,
ripgrep(`rg`), Python 3이 필요합니다. `Scripts/CheckArchitecture.ps1`은 `rg`로 프로젝트·소스 목록을 만듭니다.
`FmodRoot`는 `FMOD_ROOT` 또는 `Directory.Build.props`의 기본 설치 경로를 사용합니다.
Presentation 실행기에는 `-FmodRoot`를 전달하거나 `FMOD_ROOT`를 설정합니다.

코드 변경은 두 구성의 솔루션 빌드, 로직·카탈로그, 네 smoke, 변경 기능의 회귀,
실제 프로젝트 설정의 정적 분석, 아키텍처 검사와 조건식 검사를 완료합니다.

### 검증 스크립트

`Scripts/Verify`의 스크립트가 아래 절차를 그대로 실행합니다. 일반 PowerShell에서 실행해도 Visual Studio 개발자 환경을
스스로 열고, 모든 워크트리가 함께 쓰는 잠금을 잡아 다른 세션의 빌드·게임 실행과 겹치지 않게 합니다.
잠금과 세션 이름 설정은 [작업 방식](Contributing/Workflow.md#검증과-공유-잠금)에 있습니다. 로그는 `build/verify/`입니다.

```powershell
# 전체: Debug/Release 빌드·로직·smoke·러너 → 두 구성 Client 코드 분석 → 아키텍처 → 조건식
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Verify/VerifyAll.ps1
# 러너를 골라서(쉼표로 구분), 분석을 빼고
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Verify/VerifyAll.ps1 -Runners "GameplayPauseTests,ResultPresentationTests" -SkipAnalysis
# 부분 실행
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Verify/Build.ps1 -Configuration Release
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Verify/Test.ps1 -Configuration Debug -Core -Runners GameplayPauseTests
python Scripts/Verify/count_conditions.py --check
python Scripts/Verify/check_links.py
```

`-Runners`의 이름은 `Tests/Presentation/Run<이름>.ps1`의 `<이름>`입니다. 비우면 측정 하네스
`RunGameplayTimingProbe`를 뺀 전부를 돌립니다. `SyntaxCheck.ps1 -Files a.cpp,b.cpp`는 잠금 없이 `cl /Zs`로 문법만 봅니다.

### 직접 실행

스크립트 없이 x64 Developer PowerShell에서 같은 검사를 할 때의 명령입니다.

```powershell
git submodule update --init --recursive
msbuild YOUMU.sln /m /t:Rebuild /p:Configuration=Debug /p:Platform=x64
msbuild YOUMU.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

구성별 로직과 GUI smoke의 종료 코드를 확인합니다. GUI EXE의 직접 호출에서는
`$LASTEXITCODE`가 비어 있을 수 있으므로 `Start-Process -Wait -PassThru`를 사용합니다.

```powershell
foreach ($configuration in 'Debug', 'Release') {
    & "./bin/x64/$configuration/YOUMU.Rhythm.Tests.exe" --catalog-root Client/Assets/Songs
    if ($LASTEXITCODE -ne 0) { throw "$configuration logic tests failed" }
    $exe = (Resolve-Path "./bin/x64/$configuration/Y's Orchestral Unison - Melodic Ultimatum.exe").Path
    foreach ($route in '--smoke-test', '--smoke-lobby', '--smoke-gameplay', '--smoke-editor') {
        $process = Start-Process -FilePath $exe -ArgumentList $route -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "$configuration $route failed: $($process.ExitCode)" }
    }
}
powershell -NoProfile -File Scripts/CheckArchitecture.ps1
git diff --check
```

`--list`는 로직 테스트 이름, `--filter Chart`는 이름 부분 일치 선택입니다.
실패 또는 빈 선택은 종료 코드 1이며 나머지 검사는 계속 실행합니다.
전체 필수 검증에서는 filter를 사용하지 않습니다. 사용자 Songs·스킨·음원·Option.ini·
실제 기록은 보존하고 검증 데이터/로그는 ignored `build/`에 둡니다.

## 정적 분석과 기능별 회귀

MSVC Code Analysis 또는 clang-tidy를 실제 x64 구성의 include/define/C++20 설정으로
실행합니다. 파싱 오류를 성공으로 처리하지 않습니다. 종속 프로젝트를 빌드한 뒤
Client만 분석하는 예는 다음과 같습니다. 변경한 도메인 프로젝트도 같은 방식으로 분석합니다.

```powershell
foreach ($configuration in 'Debug', 'Release') {
    msbuild Client/YOUMU.Client.vcxproj /t:Rebuild /p:Configuration=$configuration /p:Platform=x64 /p:BuildProjectReferences=false /p:RunCodeAnalysis=true
    if ($LASTEXITCODE -ne 0) { throw "$configuration analysis failed" }
}
```

`.clang-format`/`.editorconfig`는 변경 영역의 서식 기준이며 `.clang-tidy`에는
clang-analyzer/bugprone/performance 규칙이 있습니다. 일괄 서식 변경·진단 억제는 하지 않습니다.
새 경고는 해결하고 범위 밖 기존 경고·도구 제한은 별도로 보고합니다.

[Tests/Presentation/README](../Tests/Presentation/README.md)에 실제 Client object를 연결하는
실행기가 있습니다. [변경 영향 지도](ChangeImpactMap.md)에서 관련 검사를 골라 두 구성으로
실행합니다. 에디터·옵션·키 설정·기록·변환 결과·Pause·결과·판정 표시·전체 노트 replay를
포함하며, `RunGameplayPauseTests.ps1`은 Debug 정지 중 입력과 Release 비활성화도 검사합니다.
실제 엔진을 실행하는 검사는 빌드나 다른 검사와 동시에 돌리지 않습니다. 재생 중 시간 검사는
측정한 실제 경과 시간에 배속을 곱한 범위와 비교하지만, 숨김 실행의 준비 대기 한도(cold start 15초·
Pause transport 10초)는 멈춤을 잡기 위한 고정 시간입니다.

화면 합성 비용은 Debug 빌드 게임 실행 중 F7 성능 띠의 `Draws` 값(1초 평균과 `peak` 최대 draw 수)으로 확인합니다.
Release 빌드의 띠에는 FPS와 UPS만 있습니다.
같은 파이프라인과 텍스처 페이지를 쓰는 연속 primitive는 draw 하나로 묶이므로, 사각형·이미지·
글자가 번갈아 나오는 화면일수록 값이 커집니다. 자세한 의미는 엔진
[PerformanceStatistics](../Dependencies/PhantomCore/Docs/PerformanceStatistics.md)를 따릅니다.

엔진 변경은 독립 저장소의 AGENTS/SDK 회귀를 따릅니다. 엔진 코드 분석은 SDK와 공개 SDK
테스트 프로젝트의 결과를 모두 확인합니다. 변환기 변경은 전용 저장소의
단위·CLI 검사를 수행하고 실제 출력의 카탈로그·모드 세션 검증은 Client에서 수행합니다.
재구축 가능한 목록 캐시와 영속 기록 DB를 같은 복구 정책으로 다루지 않습니다.

## 도메인 CI와 검증 한계

`.github/workflows/domain-tests.yml`은 Windows Debug/Release에서
`Tests/Rhythm/YOUMU.Rhythm.Tests.vcxproj`와 기본곡 카탈로그를 검사합니다.
도메인 빌드 작업에는 엔진 checkout/FMOD SDK가 필요하지 않으며 다음 명령으로 같은 경계를 빌드할 수 있습니다.

```powershell
msbuild Tests/Rhythm/YOUMU.Rhythm.Tests.vcxproj /m /t:Rebuild /p:Configuration=Debug /p:Platform=x64
```

별도 `architecture` 작업은 엔진 서브모듈을 checkout하고 `rg`를 확인한 뒤
`Scripts/CheckArchitecture.ps1 -AllowMissingConverter`를 실행합니다. 엔진을 빌드하거나
FMOD를 설치하지 않습니다. 변환기 형제 저장소가 없을 때만 변환기 프로젝트 세 개와
그 저장소 아래의 누락 참조를 경고로 낮춥니다. 다른 누락 참조·도메인 경계·순환 참조·
소스/filter 검사는 유지합니다. 변환기 저장소가 일부만 있거나 로컬에서 스위치를
생략하면 누락은 계속 오류입니다.

도메인 CI는 전체 Client·presentation·smoke·실제 장치 검증을 대체하지 않습니다.
smoke는 초기화/실행/정리 검사입니다. 시각 지정 replay·자동 UI 입력·PCM/mixer 검사는
물리 키보드·마우스 조작이나 수동 청음을 의미하지 않습니다. 완료 보고에서 구분합니다.

현재 알려진 빌드 관련 사항:

- Release Client는 엔진 SDK의 LIB/공개 헤더 또는 Client 프로젝트가 변경되면 자기 `IntDir`의
  증분 LTCG `.iobj`/`.ipdb`만 제거합니다. 성공한 Link 뒤에 입력 확인 stamp를 기록하고,
  입력이 같은 Client 빌드에서는 캐시를 계속 재사용합니다. `/GL`과 Release 최적화는 유지합니다.
  LNK1103 재발 시 OBJ/LIB/대상 PDB/링크 캐시와 실제 링크 명령을 먼저 보존한 뒤 비교합니다.

- Release 증분 LTCG의 LNK4020/LNK1318이 나면 같은 구성의 OBJ/LIB/compiler PDB를
  재생성하는 전체 Rebuild로 복구합니다. 필요하면 직렬 빌드와 전체 LTCG 링크를 사용합니다.
  최적화를 끄거나 경고를 억제하지 않으며 마지막 성공한 빌드·검사를 기준으로 합니다.

## 완료와 동기화

변경 코드와 직접 호출부의 정확성·수명·오류/경계·동시성·책임을 검토합니다.
실행한 구성·분석 도구·검사·미검증 범위와 사용자 데이터 보존 여부를 보고합니다.
기능 계약은 해당 문서에 갱신하고 문서에 완료 로그를 덧붙이지 않습니다.

작업은 최신 `origin/main`에서 만든 작업 브랜치에서 하고, PR을 연 뒤 지휘 세션에 병합 요청을 보냅니다.
지휘 세션이 병합 묶음을 `VerifyAll.ps1`로 검증한 뒤 main에 병합·push합니다. 엔진 변경은 엔진 PR 병합 →
YOUMU gitlink 갱신 순서를 지키고, 병합 뒤 `git ls-files -s Dependencies/PhantomCore`가 서브모듈 HEAD와 같은지
확인합니다. 자세한 절차는 [작업 방식](Contributing/Workflow.md)입니다.
