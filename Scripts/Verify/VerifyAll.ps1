param([string]$Runners = '', [switch]$SkipAnalysis)
# 코드 변경의 전체 검증을 차례로 실행합니다. 첫 실패 단계에서 멈춥니다(Docs/Verification.md).
#   Debug 빌드 → Debug 테스트 → Release 빌드 → Release 테스트 → Client 코드 분석 Debug/Release → 아키텍처 검사
#   → 조건식 검사(count_conditions/count_expressions)
# -Runners를 비우면 Presentation 러너 전부, 'A,B'를 주면 그 러너만 돌립니다.
$ErrorActionPreference = 'Continue'
$here = $PSScriptRoot
Set-Location (Split-Path -Parent (Split-Path -Parent $here))
function Stage([string]$name, [scriptblock]$body) {
    "== $name"
    & $body
    if ($LASTEXITCODE -ne 0) { "STAGE FAILED: $name"; exit 1 }
}
# if/else 식으로 고르면 원소 하나짜리 배열이 문자열로 풀려 스플래팅이 깨지므로 배열을 직접 대입합니다.
$runnerArgs = @('-AllRunners')
if ($Runners) { $runnerArgs = @('-Runners', $Runners) }
Stage 'Debug build' { powershell -NoProfile -ExecutionPolicy Bypass -File "$here/Build.ps1" -Configuration Debug }
Stage 'Debug tests' { powershell -NoProfile -ExecutionPolicy Bypass -File "$here/Test.ps1" -Configuration Debug -Core @runnerArgs }
Stage 'Release build' { powershell -NoProfile -ExecutionPolicy Bypass -File "$here/Build.ps1" -Configuration Release }
Stage 'Release tests' { powershell -NoProfile -ExecutionPolicy Bypass -File "$here/Test.ps1" -Configuration Release -Core @runnerArgs }
if (-not $SkipAnalysis) {
    Stage 'Debug analysis' { powershell -NoProfile -ExecutionPolicy Bypass -File "$here/Build.ps1" -Configuration Debug -Analyze }
    Stage 'Release analysis' { powershell -NoProfile -ExecutionPolicy Bypass -File "$here/Build.ps1" -Configuration Release -Analyze }
}
Stage 'Architecture' { powershell -NoProfile -ExecutionPolicy Bypass -File "$here/Architecture.ps1" }
Stage 'Conditions' { python "$here/count_conditions.py" --check }
Stage 'Expressions' { python "$here/count_expressions.py" --check }
'VERIFY ALL PASSED'
