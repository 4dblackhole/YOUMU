param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
# 크래시 진단 기록: 단위 검사(실행 인자, 최근 통계, 로그 파일 보관·폴더 이름 가림, 장치 끊김·치명적 오류·멈춤 줄)를 돌린 뒤,
# 게임 exe를 숨김 창으로 세 번 실행해(정상, 그래픽 장치 일부러 제거, Update 일부러 멈춤) 로그 파일의 내용을 확인합니다.
# 로그는 --log-dir로 build/ 아래 임시 폴더에 쓰므로 게임 폴더의 Data/Logs를 건드리지 않습니다.
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$testRoot = Join-Path $repoRoot ('build/crash-diagnostics-tests/' + $Configuration + '-' + [guid]::NewGuid().ToString('N'))
$objects = Join-Path $testRoot 'obj'
New-Item -ItemType Directory -Path $objects -Force | Out-Null
$runtime = if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MT', '/DNDEBUG') }
$units = @('Pch', 'GameLog', 'RecentStatistics', 'GameDiagnostics', 'LaunchDiagnostics')
$clientObjects = $units | ForEach-Object { "build/obj/YOUMU.Client/x64/$Configuration/$_.obj" }
$testExe = Join-Path $testRoot 'CrashDiagnosticsTests.exe'

# 로그 한 개를 읽어, 있어야 할 줄과 없어야 할 글자를 확인합니다.
function Test-GameLog([string]$Name, [string]$Folder, [string[]]$Expected) {
    $logs = @(Get-ChildItem -LiteralPath $Folder -Filter 'youmu-*.log' -ErrorAction SilentlyContinue)
    if ($logs.Count -ne 1) { throw "${Name}: expected one log file in $Folder, found $($logs.Count)." }
    $text = [IO.File]::ReadAllText($logs[0].FullName)
    foreach ($part in $Expected) {
        if (-not $text.Contains($part)) { throw "${Name}: the log has no '$part'.`n$text" }
    }
    # 사용자 이름이 든 프로필 폴더가 로그에 그대로 남으면 안 됩니다(게임 폴더는 <game>으로 바뀝니다).
    if ($env:USERPROFILE -and $text.Contains($env:USERPROFILE)) { throw "${Name}: the log shows the profile folder.`n$text" }
    $text
}

Push-Location $repoRoot
try {
    & cl /nologo /W4 /analyze /utf-8 /std:c++20 /EHsc @runtime /DNOMINMAX /DWIN32_LEAN_AND_MEAN /DUNICODE /D_UNICODE `
        /IClient /ITests/Presentation /IYOUMU.Chart /IYOUMU.Assets/Public /IDependencies/PhantomCore/Engine/SDK `
        Tests/Presentation/CrashDiagnosticsTests.cpp @clientObjects "bin/x64/$Configuration/YOUMU.Assets.lib" `
        "Dependencies/PhantomCore/bin/x64/$Configuration/PhantomCore.lib" `
        "/Fo$objects/" "/Fe$testExe" /link /OPT:NOICF /OPT:NOREF `
        "/LIBPATH:$env:FMOD_ROOT/api/core/lib/x64" user32.lib gdi32.lib ole32.lib shell32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Crash diagnostics test compilation failed.' }

    & $testExe (Join-Path $testRoot 'unit')
    if ($LASTEXITCODE -ne 0) { throw 'Crash diagnostics unit checks failed.' }

    # 게임 exe는 Client 프로젝트의 출력 경로로 찾습니다(Test.ps1의 smoke와 같은 방법).
    $exe = "$(& msbuild Client/YOUMU.Client.vcxproj -nologo -getProperty:TargetPath "-p:Configuration=$Configuration" -p:Platform=x64)".Trim()
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "The game executable was not found: $exe" }
    $runs = @(
        @{ Name = 'normal'; Arguments = @('--smoke-lobby'); ExitCode = 0
           Expected = @('YOUMU, built', 'windows ', 'start: scene ', 'engine: graphics adapter: ', 'engine: window: ', 'options: audio ',
               'scene: ', 'exit code 0') },
        @{ Name = 'device'; Arguments = @('--smoke-lobby', '--crash-test=device'); ExitCode = 1
           Expected = @('crash test device', 'graphics device lost: ', 'reason DXGI_ERROR_DEVICE_REMOVED', 'recent statistics, ',
               'fatal error: ', 'exit code 1') },
        @{ Name = 'stall'; Arguments = @('--smoke-lobby', '--crash-test=stall'); ExitCode = 0
           Expected = @('crash test stall', 'crash test: holding one Update', 'main loop stalled: no pass for ', 'recent statistics, ',
               'main loop resumed after ', 'exit code 0') })
    foreach ($run in $runs) {
        $logFolder = Join-Path $testRoot ('logs-' + $run.Name)
        $arguments = $run.Arguments + ('--log-dir="' + $logFolder + '"')
        $process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne $run.ExitCode) { throw "$($run.Name): exit code $($process.ExitCode), expected $($run.ExitCode)." }
        $text = Test-GameLog $run.Name $logFolder $run.Expected
        Write-Output "PASS $($run.Name): exit $($process.ExitCode), $(@($text -split "`r`n").Count - 1) log lines"
    }
    # A passing run leaves nothing behind; a failing run keeps its logs (the throw above skips this) for diagnosis.
    Remove-Item -LiteralPath $testRoot -Recurse -Force
    Write-Output 'Crash diagnostics tests passed; fixtures removed.'
} finally { Pop-Location }
