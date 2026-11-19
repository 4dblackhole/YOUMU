param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug', [string[]]$Runners = @(), [switch]$AllRunners, [switch]$Core)
# 로직·카탈로그 테스트, smoke 4종(-Core), 이름을 준 Presentation 러너(-Runners A,B 또는 -AllRunners)를 차례로 돌립니다.
# 실행하는 동안 Engine 잠금을 쥐므로 다른 세션의 빌드·엔진 검사와 겹치지 않습니다. 로그: build/verify/
# -AllRunners는 Tests/Presentation/Run*.ps1 전부입니다(측정 하네스 RunGameplayTimingProbe 제외).
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Enter-YoumuDevShell
if (-not $env:FMOD_ROOT) { throw 'FMOD_ROOT를 설정하세요(Presentation 러너가 FMOD import library를 링크합니다).' }
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
        $exe = (Get-ChildItem -LiteralPath "./bin/x64/$Configuration" -Filter '*.exe' |
            Where-Object { $_.Name -notmatch 'Tests|Probe|Capture|OsuTaikoConverter' } | Select-Object -First 1).FullName
        if (-not $exe) { throw "게임 실행 파일이 bin/x64/$Configuration 에 없습니다. 먼저 Build.ps1을 실행하세요." }
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
