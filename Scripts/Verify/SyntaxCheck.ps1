param([string[]]$Files, [switch]$Tests)
# Client 소스의 문법만 검사합니다(cl /Zs). 오브젝트·라이브러리·실행 파일을 만들지 않으므로 공유 잠금이 필요 없고,
# 잠금을 기다리는 동안 빠르게 확인하는 데 씁니다. -Tests는 Pch.h를 강제로 넣지 않습니다(테스트 소스용).
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Enter-YoumuDevShell
$generated = Get-ChildItem -Recurse -Directory -Filter Generated build/obj/YOUMU.Client -ErrorAction SilentlyContinue | Select-Object -First 1
$includes = @('/IClient', '/IDependencies/PhantomCore/Engine/SDK', '/IYOUMU.Rhythm', '/IYOUMU.Chart', '/IYOUMU.Modes', '/IYOUMU.Assets/Public', '/IYOUMU.Editor')
if ($generated) { $includes += "/I$($generated.FullName)" }
$failed = 0
foreach ($file in @($Files | ForEach-Object { $_ -split ',' } | Where-Object { $_ })) {
    & cl /nologo /Zs /std:c++20 /EHsc /utf-8 /W4 /permissive- /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_DEBUG /MDd @(if (-not $Tests) { '/FIPch.h' }) @includes $file 2>&1 |
        Where-Object { $_ -match 'error|warning' } | Select-Object -First 25
    if ($LASTEXITCODE -ne 0) { $failed++ }
}
"syntax check failures: $failed"
exit $failed
