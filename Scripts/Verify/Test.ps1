param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug', [string[]]$Runners = @(), [switch]$AllRunners, [switch]$Core,
      [switch]$NoBuildCache)
# 로직·카탈로그 테스트, smoke 4종(-Core), 이름을 준 Presentation 러너(-Runners A,B 또는 -AllRunners)를 차례로 돌립니다.
# smoke는 Client 프로젝트가 만든 게임 exe(msbuild의 TargetPath)만 실행하고, 실행한 경로를 출력합니다. 없으면 실패합니다.
# 실행하는 동안 Engine 잠금을 쥐므로 다른 세션의 빌드·엔진 검사와 겹치지 않습니다. 로그: build/verify/
# -AllRunners는 Tests/Presentation/Run*.ps1 전부입니다(측정 하네스 RunGameplayTimingProbe 제외).
# 러너의 테스트 exe는 입력이 지난번과 같으면 다시 컴파일하지 않고 지난번 것을 실행합니다(TestBuildCache.ps1).
# 테스트는 언제나 실행합니다. -NoBuildCache는 테스트 exe도 언제나 다시 빌드합니다.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Enter-YoumuDevShell
if (-not $env:FMOD_ROOT) { throw 'FMOD_ROOT를 설정하세요(Presentation 러너가 FMOD import library를 링크합니다).' }
if ($NoBuildCache) { $env:YOUMU_TEST_BUILD_CACHE = '0' }
. (Join-Path $PSScriptRoot 'TestBuildCache.ps1')
$Runners = @($Runners | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
if ($AllRunners) {
    $Runners = @(Get-ChildItem -LiteralPath (Join-Path $script:RepoRoot 'Tests\Presentation') -Filter 'Run*.ps1' |
        ForEach-Object { $_.BaseName.Substring(3) } | Where-Object { $_ -ne 'GameplayTimingProbe' } | Sort-Object)
}
$lock = Enter-VerifyLock -Kind Engine
try {
    $failed = @()
    if ($Core) {
        $log = Join-Path $script:VerifyLogDir "logic-$Configuration.log"
        & "./bin/x64/$Configuration/YOUMU.Rhythm.Tests.exe" --catalog-root Client/Assets/Songs *> $log
        if ($LASTEXITCODE -ne 0) { $failed += 'logic'; Get-Content $log -Tail 15 } else { "PASS logic: $(Get-Content $log -Tail 1)" }
        # 게임 exe는 Client 프로젝트의 출력 경로로 찾습니다. 폴더의 다른 exe(측정·진단용)를 게임으로 잘못 고르지 않습니다.
        $exe = "$(& msbuild Client/YOUMU.Client.vcxproj -nologo -getProperty:TargetPath "-p:Configuration=$Configuration" -p:Platform=x64)".Trim()
        if ($LASTEXITCODE -ne 0 -or -not $exe) { throw 'Client 프로젝트의 출력 경로(TargetPath)를 읽지 못했습니다.' }
        if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "게임 실행 파일이 없습니다: $exe. 먼저 Build.ps1을 실행하세요." }
        "smoke exe: $exe"
        $options = Join-Path (Split-Path $exe) 'Option.ini'
        $before = if (Test-Path -LiteralPath $options) { (Get-FileHash -LiteralPath $options).Hash } else { '' }
        foreach ($route in '--smoke-test', '--smoke-lobby', '--smoke-gameplay', '--smoke-editor') {
            $process = Start-Process -FilePath $exe -ArgumentList $route -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden -Wait -PassThru
            if ($process.ExitCode -ne 0) { $failed += "smoke $route"; "FAIL $route exit $($process.ExitCode)" } else { "PASS $route" }
        }
        $after = if (Test-Path -LiteralPath $options) { (Get-FileHash -LiteralPath $options).Hash } else { '' }
        # 새 워크트리에는 Option.ini가 없으므로 첫 smoke가 기본 파일을 만드는 것은 변경이 아닙니다.
        if ($before -and $before -ne $after) { $failed += 'Option.ini changed' }
    }
    foreach ($runner in $Runners) {
        $log = Join-Path $script:VerifyLogDir "$runner-$Configuration.log"
        $watch = [Diagnostics.Stopwatch]::StartNew()
        $ok = $true
        try { & "./Tests/Presentation/Run$runner.ps1" -Configuration $Configuration *> $log; if ($LASTEXITCODE -ne 0) { $ok = $false } }
        catch { $ok = $false; $_ | Out-File $log -Append }
        if ($ok) { "PASS $runner ($([int]$watch.Elapsed.TotalSeconds)s)" }
        else { $failed += $runner; "FAIL $runner ($([int]$watch.Elapsed.TotalSeconds)s)"; Get-Content $log -Tail 12 }
    }
}
finally { Exit-VerifyLock $lock }
if ($failed.Count) { "FAILED: $($failed -join ', ')"; exit 1 }
'ALL PASSED'
