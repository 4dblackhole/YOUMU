# 빌드와 검증

이 문서는 재현 가능한 현재 절차입니다. 날짜별 완료 로그는 Git 이력에서 확인합니다.
문서만 바꾸면 로컬 링크·앵커와 `git diff --check`를 검사하며 C++ 빌드는 필요하지 않습니다.

## 환경과 필수 검사

저장소 루트의 x64 Developer PowerShell에서 실행합니다. Visual Studio 2022/v143,
Windows SDK, FMOD SDK와 `../OsuTaikoConverter` checkout이 필요합니다.
`FmodRoot`는 `FMOD_ROOT` 또는 `Directory.Build.props`의 기본 설치 경로를 사용합니다.
Presentation 실행기에는 `-FmodRoot`를 전달하거나 `FMOD_ROOT`를 설정합니다.

코드 변경은 두 구성의 솔루션 빌드, 로직·카탈로그, 네 smoke, 변경 기능의 회귀,
실제 프로젝트 설정의 정적 분석과 아키텍처 검사를 완료합니다.

```powershell
git submodule update --init --recursive
msbuild YOUMU.sln /m /t:Rebuild /p:Configuration=Debug /p:Platform=x64
msbuild YOUMU.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

구성별 로직과 GUI smoke의 종료 코드를 확인합니다. GUI EXE의 직접 호출에서는
`$LASTEXITCODE`가 비어 있을 수 있으므로 `Start-Process -Wait -PassThru`를 사용합니다.

```powershell
foreach ($configuration in 'Debug', 'Release') {
    & "./bin/x64/$configuration/YOUMU.Rhythm.Tests.exe" --catalog-root YOUMU.Assets/Assets/Songs
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

엔진 변경은 독립 저장소의 AGENTS/SDK 회귀를 따릅니다. 변환기 변경은 전용 저장소의
단위·CLI 검사를 수행하고 실제 출력의 카탈로그·모드 세션 검증은 Client에서 수행합니다.
재구축 가능한 목록 캐시와 영속 기록 DB를 같은 복구 정책으로 다루지 않습니다.

## 도메인 CI와 검증 한계

`.github/workflows/domain-tests.yml`은 Windows Debug/Release에서
`Tests/Rhythm/YOUMU.Rhythm.Tests.vcxproj`와 기본곡 카탈로그를 검사합니다.
엔진 checkout/FMOD SDK는 필요하지 않으며 다음 명령으로 같은 경계를 빌드할 수 있습니다.

```powershell
msbuild Tests/Rhythm/YOUMU.Rhythm.Tests.vcxproj /m /t:Rebuild /p:Configuration=Debug /p:Platform=x64
```

도메인 CI는 전체 Client·presentation·smoke·실제 장치 검증을 대체하지 않습니다.
smoke는 초기화/실행/정리 검사입니다. 시각 지정 replay·자동 UI 입력·PCM/mixer 검사는
물리 키보드·마우스 조작이나 수동 청음을 의미하지 않습니다. 완료 보고에서 구분합니다.

현재 알려진 빌드 관련 사항:

- 전체 엔진 분석에는 범위 밖 기존 C26478(`ScreenVisual2DManager`) 및
  C26498/C6011(`Visual2DSurface`)이 남아 있습니다. 실제 분석 결과를 다시 확인하고 보고합니다.
- Release 증분 LTCG의 LNK4020/LNK1318이 나면 같은 구성의 OBJ/LIB/compiler PDB를
  재생성하는 전체 Rebuild로 복구합니다. 필요하면 직렬 빌드와 전체 LTCG 링크를 사용합니다.
  최적화를 끄거나 경고를 억제하지 않으며 마지막 성공한 빌드·검사를 기준으로 합니다.

## 완료와 동기화

변경 코드와 직접 호출부의 정확성·수명·오류/경계·동시성·책임을 검토합니다.
실행한 구성·분석 도구·검사·미검증 범위와 사용자 데이터 보존 여부를 보고합니다.
기능 계약은 해당 문서에 갱신하고 인수인계에 완료 로그를 덧붙이지 않습니다.

main 직접 작업은 검증 후 commit/push합니다. Client/엔진을 fetch하여 upstream과 비교하고,
엔진 변경은 원격 게시 → Client gitlink 갱신 순서를 지킵니다. gitlink와 submodule HEAD,
최종 status를 확인하고 정확한 커밋을 보고합니다. 태스크 브랜치는 사용자 요청 때만 사용하며
PR/병합 절차는 [AGENTS.md](../AGENTS.md)를 따릅니다.
