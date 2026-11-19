# 검증 공유 잠금 (공정한 대기열). 이 PC의 모든 워크트리·세션이 하나의 잠금 폴더를 함께 씁니다.
# Docs/Verification.md: 실제 엔진을 띄우는 검사는 빌드나 다른 검사와 동시에 돌리지 않습니다.
#   Engine = 배타 잠금(솔루션 빌드, 테스트·smoke, 측정)
#   Build  = 공유 잠금(Client만 다시 빌드하는 코드 분석). 여러 워크트리가 동시에 쥘 수 있음
# 기다리는 쪽은 번호표(.ticket)를 받고 가장 오래된 번호표만 잠금을 가져갑니다. 프로세스가 끝난 잠금·번호표는 자동으로 지웁니다.
#
# 잠금 폴더 결정 순서:
#   1. 환경 변수 YOUMU_VERIFY_LOCKS
#   2. 주 워크트리의 build/ui02/locks가 있으면 그것(이전 도구 사본과 같은 잠금을 쓰는 이전 기간용)
#   3. 주 워크트리의 build/verify-locks
# 사용: $lock = Enter-VerifyLock -Kind Engine   ...   Exit-VerifyLock $lock
function Get-VerifyLockRoot {
    if ($env:YOUMU_VERIFY_LOCKS) { return $env:YOUMU_VERIFY_LOCKS }
    $main = (& git -C $script:RepoRoot worktree list --porcelain 2>$null | Select-Object -First 1) -replace '^worktree\s+', ''
    if (-not $main) { $main = $script:RepoRoot }
    $legacy = Join-Path $main 'build\ui02\locks'
    if (Test-Path -LiteralPath $legacy) { return $legacy }
    return (Join-Path $main 'build\verify-locks')
}
$script:VerifyLockRoot = Get-VerifyLockRoot

function Get-VerifyOwner {
    # 잠금 파일에 남는 이름: YOUMU_SESSION_NAME(있으면) + 워크트리 폴더 이름
    $leaf = (Split-Path -Leaf $script:RepoRoot) -replace '[^A-Za-z0-9_.]', '_'
    if ($env:YOUMU_SESSION_NAME) { return ($env:YOUMU_SESSION_NAME -replace '[^A-Za-z0-9_.]', '_') + '-' + $leaf }
    return $leaf
}

function Remove-StaleVerifyLocks {
    foreach ($file in Get-ChildItem -LiteralPath $script:VerifyLockRoot -File -ErrorAction SilentlyContinue) {
        if ($file.Extension -ne '.lock' -and $file.Extension -ne '.ticket') { continue }
        $text = Get-Content -LiteralPath $file.FullName -Raw -ErrorAction SilentlyContinue
        if ($text -match 'pid=(\d+)') {
            if (-not (Get-Process -Id ([int]$Matches[1]) -ErrorAction SilentlyContinue)) {
                Remove-Item -LiteralPath $file.FullName -ErrorAction SilentlyContinue
            }
        }
    }
}

function Write-VerifyLockFile([string]$Path, [string]$Owner, [string]$Kind) {
    # CreateNew는 파일이 있으면 실패하므로 Engine 잠금은 하나만 생깁니다.
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
    $writer = New-Object IO.StreamWriter($stream)
    $writer.Write("owner=$Owner kind=$Kind pid=$PID since=$(Get-Date -Format o)")
    $writer.Dispose()
}

function Test-VerifyHead([string]$Ticket) {
    # 번호표 이름은 0을 채운 UTC tick으로 시작하므로 이름 순서가 도착 순서입니다.
    $first = Get-ChildItem -LiteralPath $script:VerifyLockRoot -Filter '*.ticket' -ErrorAction SilentlyContinue |
        Sort-Object Name | Select-Object -First 1
    return ($null -ne $first) -and ($first.FullName -eq $Ticket)
}

function Enter-VerifyLock {
    param([ValidateSet('Build', 'Engine')][string]$Kind, [string]$Owner = (Get-VerifyOwner), [int]$TimeoutMinutes = 480)
    New-Item -ItemType Directory -Force -Path $script:VerifyLockRoot | Out-Null
    $engine = Join-Path $script:VerifyLockRoot 'engine.lock'
    $ticket = Join-Path $script:VerifyLockRoot ('{0:D20}-{1}-{2}-{3}.ticket' -f [DateTime]::UtcNow.Ticks, $Kind, $Owner, $PID)
    Write-VerifyLockFile $ticket $Owner $Kind
    $deadline = (Get-Date).AddMinutes($TimeoutMinutes)
    $lastNotice = [datetime]::MinValue
    try {
        while ($true) {
            Remove-StaleVerifyLocks
            if (-not (Test-Path -LiteralPath $ticket)) { Write-VerifyLockFile $ticket $Owner $Kind }
            if (Test-VerifyHead $ticket) {
                if ($Kind -eq 'Engine') {
                    $taken = $false
                    try { Write-VerifyLockFile $engine $Owner $Kind; $taken = $true } catch [IO.IOException] { }
                    if ($taken) {
                        Remove-Item -LiteralPath $ticket -ErrorAction SilentlyContinue
                        # 먼저 시작한 공유 빌드가 끝날 때까지 기다리고, 새 공유 빌드는 engine.lock 뒤에서 기다립니다.
                        while (Get-ChildItem -LiteralPath $script:VerifyLockRoot -Filter 'build-*.lock' -ErrorAction SilentlyContinue) {
                            Remove-StaleVerifyLocks
                            Start-Sleep -Seconds 5
                        }
                        return $engine
                    }
                }
                elseif (-not (Test-Path -LiteralPath $engine)) {
                    $mine = Join-Path $script:VerifyLockRoot "build-$Owner-$PID.lock"
                    Write-VerifyLockFile $mine $Owner $Kind
                    if (-not (Test-Path -LiteralPath $engine)) {
                        Remove-Item -LiteralPath $ticket -ErrorAction SilentlyContinue
                        return $mine
                    }
                    Remove-Item -LiteralPath $mine -ErrorAction SilentlyContinue
                }
            }
            if ((Get-Date) -gt $deadline) { throw "공유 $Kind 잠금을 기다리다 시간이 지났습니다." }
            if (((Get-Date) - $lastNotice).TotalMinutes -ge 5) {
                $holders = (Get-ChildItem -LiteralPath $script:VerifyLockRoot -Filter '*.lock' -ErrorAction SilentlyContinue |
                    ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join '; '
                $ahead = @(Get-ChildItem -LiteralPath $script:VerifyLockRoot -Filter '*.ticket' -ErrorAction SilentlyContinue |
                    Sort-Object Name | Where-Object { $_.FullName -lt $ticket }).Count
                # Write-Host: 파이프라인에 쓰면 반환값에 섞입니다.
                Write-Host "공유 $Kind 잠금 대기 중 (앞에 $ahead; 보유: $holders)"
                $lastNotice = Get-Date
            }
            Start-Sleep -Seconds 5
        }
    }
    finally {
        # 성공(번호표는 이미 지움), 시간 초과, 스크립트 중단 모두 여기를 지납니다.
        Remove-Item -LiteralPath $ticket -ErrorAction SilentlyContinue
    }
}

function Exit-VerifyLock([string]$Path) {
    if ($Path) { Remove-Item -LiteralPath $Path -ErrorAction SilentlyContinue }
}
