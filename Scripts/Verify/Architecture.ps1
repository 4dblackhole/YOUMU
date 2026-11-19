# 아키텍처 검사(Scripts/CheckArchitecture.ps1)를 ripgrep이 PATH에 있는 환경에서 실행합니다.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
if (-not (Get-Command rg -ErrorAction SilentlyContinue)) { throw 'ripgrep(rg)이 필요합니다: winget install BurntSushi.ripgrep.MSVC' }
& (Join-Path $script:RepoRoot 'Scripts\CheckArchitecture.ps1') @args
exit $LASTEXITCODE
