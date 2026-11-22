param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug', [string]$Target = 'Build', [switch]$Analyze)
# 솔루션을 빌드하거나(-Analyze) Client 프로젝트를 MSVC Code Analysis로 분석합니다. 로그: build/verify/
# - 솔루션 빌드는 Engine 잠금을 씁니다. 형제 폴더의 OsuTaikoConverter 프로젝트도 이 워크트리의 YOUMU 소스(YOUMURoot)로 다시 빌드합니다.
#   테스트 프로젝트(이름이 .Tests로 끝나는 프로젝트)의 컴파일·링크 경고는 오류입니다(TestWarnings.targets). 경고가 난 파일은
#   오브젝트가 생기지 않아 고칠 때까지 빌드할 때마다 다시 컴파일되어 실패합니다.
# - 코드 분석은 이 워크트리 안에만 쓰므로 공유 Build 잠금을 씁니다. 분석 전용 오브젝트 폴더(build/obj/YOUMU.Client.Analysis)에서
#   컴파일과 분석 결과 보고(RunNativeCodeAnalysis)만 하고 링크하지 않습니다. 일반 빌드의 오브젝트와 exe를 건드리지 않고,
#   지난 분석 뒤에 바뀐 파일과 그 헤더를 쓰는 파일만 다시 분석합니다.
#   경고는 오류입니다. 분석 경고는 파일별 결과 파일에 남아 고칠 때까지 매번 실패하고(CodeAnalysisTreatWarningsAsErrors),
#   컴파일 경고가 난 파일은 오브젝트가 생기지 않아 매번 다시 컴파일됩니다(Analysis.targets의 /WX).
#   `-Analyze -Target Rebuild`는 분석 폴더를 지우고 전부 다시 분석합니다.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Enter-YoumuDevShell
$root = $script:RepoRoot
$lock = Enter-VerifyLock -Kind $(if ($Analyze) { 'Build' } else { 'Engine' })
try {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    if ($Analyze) {
        $log = Join-Path $script:VerifyLogDir "analysis-$Configuration.log"
        $analysisDir = Join-Path $root "build\obj\YOUMU.Client.Analysis\x64\$Configuration"
        if ($Target -eq 'Rebuild') { Remove-Item -LiteralPath $analysisDir -Recurse -Force -ErrorAction SilentlyContinue }
        $analysisTargets = Join-Path $PSScriptRoot 'Analysis.targets'
        # IgnoreWarnIntDirSharingDetected: 참조 프로젝트도 이 IntDir를 넘겨받아 상태 폴더를 만들 뿐 컴파일하지 않으므로 MSB8028을 끕니다.
        msbuild Client/YOUMU.Client.vcxproj /m /nologo /v:m /t:RunNativeCodeAnalysis /p:Configuration=$Configuration /p:Platform=x64 /p:BuildProjectReferences=false /p:RunCodeAnalysis=true /p:CodeAnalysisTreatWarningsAsErrors=true /p:IgnoreWarnIntDirSharingDetected=true "/p:IntDir=$analysisDir\" "/p:ForceImportAfterCppTargets=$analysisTargets" *> $log
    } else {
        $log = Join-Path $script:VerifyLogDir "build-$Configuration.log"
        $testWarnings = Join-Path $PSScriptRoot 'TestWarnings.targets'
        msbuild YOUMU.sln /m /nologo /v:m /t:$Target /p:Configuration=$Configuration /p:Platform=x64 "/p:YOUMURoot=$root\" "/p:ForceImportAfterCppTargets=$testWarnings" *> $log
    }
    $code = $LASTEXITCODE
}
finally { Exit-VerifyLock $lock }
"exit $code in $([int]$watch.Elapsed.TotalSeconds)s -> $log"
Get-Content $log | Select-String -Pattern ' error |warning ' | Select-Object -First 40
exit $code
