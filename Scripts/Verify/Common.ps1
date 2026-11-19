# 검증 스크립트 공용 설정. 다른 Scripts/Verify 스크립트가 dot-source 합니다.
# - 저장소 루트로 이동하고, 로그 폴더(build/verify)를 만듭니다.
# - cl/msbuild가 PATH에 없으면 vswhere로 찾은 Visual Studio 개발자 환경(x64)을 엽니다.
# - FMOD_ROOT가 비어 있으면 Directory.Build.props와 같은 기본 설치 경로를 씁니다.
# - WinGet으로 설치한 ripgrep(rg)을 PATH에 더합니다(CheckArchitecture.ps1이 사용).
$script:RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $script:RepoRoot
$script:VerifyLogDir = Join-Path $script:RepoRoot 'build\verify'
New-Item -ItemType Directory -Force -Path $script:VerifyLogDir | Out-Null

function Enter-YoumuDevShell {
    if ((Get-Command cl -ErrorAction SilentlyContinue) -and (Get-Command msbuild -ErrorAction SilentlyContinue)) { return }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe가 없습니다. Visual Studio 2022(C++ 데스크톱 워크로드)를 설치하세요.' }
    $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $install) { throw 'C++ 도구가 있는 Visual Studio를 찾지 못했습니다.' }
    # VsDevCmd.bat 안의 확장 스크립트가 vswhere.exe를 PATH에서 다시 찾으므로 설치 폴더를 PATH에 넣습니다.
    $env:Path = "$(Split-Path -Parent $vswhere);$env:Path"
    Import-Module (Join-Path $install 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
    Enter-VsDevShell -VsInstallPath $install -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
    Set-Location $script:RepoRoot
}

if (-not $env:FMOD_ROOT) {
    $defaultFmod = 'C:\Program Files (x86)\FMOD SoundSystem\FMOD Studio API Windows'
    if (Test-Path -LiteralPath (Join-Path $defaultFmod 'api\core\inc\fmod.hpp')) { $env:FMOD_ROOT = $defaultFmod }
}

$wingetLinks = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Links'
if ((Test-Path -LiteralPath $wingetLinks) -and ($env:Path -notlike "*$wingetLinks*")) { $env:Path = "$wingetLinks;$env:Path" }

. (Join-Path $PSScriptRoot 'VerifyLock.ps1')
