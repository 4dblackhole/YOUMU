param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug', [string]$Target = 'Build', [switch]$Analyze)
# 솔루션을 빌드하거나(-Analyze) Client 프로젝트만 MSVC Code Analysis로 다시 빌드합니다. 로그: build/verify/
# - 솔루션 빌드는 Engine 잠금을 씁니다. 형제 폴더의 OsuTaikoConverter 프로젝트도 이 워크트리의 YOUMU 소스(YOUMURoot)로 다시 빌드합니다.
# - 코드 분석은 이 워크트리 안에만 쓰므로 공유 Build 잠금을 씁니다.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Enter-YoumuDevShell
$root = $script:RepoRoot
$lock = Enter-VerifyLock -Kind $(if ($Analyze) { 'Build' } else { 'Engine' })
try {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    if ($Analyze) {
        $log = Join-Path $script:VerifyLogDir "analysis-$Configuration.log"
        msbuild Client/YOUMU.Client.vcxproj /nologo /v:m /t:Rebuild /p:Configuration=$Configuration /p:Platform=x64 /p:BuildProjectReferences=false /p:RunCodeAnalysis=true *> $log
    } else {
        $log = Join-Path $script:VerifyLogDir "build-$Configuration.log"
        msbuild YOUMU.sln /m /nologo /v:m /t:$Target /p:Configuration=$Configuration /p:Platform=x64 "/p:YOUMURoot=$root\" *> $log
    }
    $code = $LASTEXITCODE
}
finally { Exit-VerifyLock $lock }
"exit $code in $([int]$watch.Elapsed.TotalSeconds)s -> $log"
Get-Content $log | Select-String -Pattern ' error |warning ' | Select-Object -First 40
exit $code
