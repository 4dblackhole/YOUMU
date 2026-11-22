# Presentation 러너의 테스트 exe 빌드: 경고를 오류로 올리고, 바뀌지 않은 exe는 다시 만들지 않습니다. Test.ps1이 dot-source 합니다.
#
# 러너(Tests/Presentation/Run*.ps1)는 실행할 때마다 `cl`로 테스트 소스를 컴파일하고 Client 오브젝트와 링크합니다.
# 이 파일을 dot-source 하면 `cl` 함수가 cl.exe를 가려서, 입력이 지난번과 같을 때는 지난번에 만든 exe를 그대로 가져다 놓습니다.
# 테스트 실행 여부는 여기서 정하지 않습니다. 컴파일·링크 시간만 줄입니다. 지난 컴파일이 낸 출력도 그대로 다시 적습니다.
#
# 경고는 오류입니다. 컴파일러(분석 경고 포함)에는 /WX를, 링커에는 /link 뒤에 /WX를 붙입니다. 캐시를 꺼도 붙습니다.
# 경고가 난 빌드는 exe가 생기지 않으므로 보관되지 않고, 고칠 때까지 실행할 때마다 다시 빌드되어 실패합니다.
#
# 같다고 보는 조건(하나라도 다르면 다시 빌드):
#   - cl 명령줄(/Fe·/Fo 출력 경로 제외), cl.exe 자체, INCLUDE·LIB 환경
#   - 명령줄에 적힌 파일(테스트 소스, Client .obj, .lib, .res)과 /LIBPATH 폴더의 .lib의 크기와 수정 시각
#   - 지난 컴파일이 실제로 읽은 헤더(cl /sourceDependencies 기록)의 크기와 수정 시각. 컴파일러·Windows SDK 헤더는
#     cl.exe와 INCLUDE로 갈음합니다.
# 끄려면 YOUMU_TEST_BUILD_CACHE=0 (Test.ps1 -NoBuildCache). 캐시 폴더: build/verify/test-builds
#
# 상태는 전역 변수 하나에 둡니다. `cl`은 러너 스크립트 안에서 불리고, 그때 $script: 범위는 러너의 것이기 때문입니다.
$global:YoumuTestBuild = @{
    Compiler   = (Get-Command cl -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
    CacheDir   = Join-Path $script:VerifyLogDir 'test-builds'
    # INCLUDE에 든 폴더(컴파일러·Windows SDK). 이 아래 헤더는 하나씩 보지 않습니다.
    SystemDirs = @("$env:INCLUDE" -split ';' | Where-Object { $_ } | ForEach-Object { $_.TrimEnd('\').ToLowerInvariant() + '\' })
}

function Get-TestBuildHash([string]$Text) {
    $sha = [Security.Cryptography.SHA1]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text))) -replace '-', '').Substring(0, 16).ToLowerInvariant() }
    finally { $sha.Dispose() }
}

# 파일 하나의 상태 줄. 없는 파일도 줄을 남겨, 헤더가 지워지면 다시 빌드하게 합니다.
function Get-TestBuildFileState([string]$Path) {
    $info = [IO.FileInfo]::new($Path)
    if (-not $info.Exists) { return "$Path|missing" }
    return "$Path|$($info.Length)|$($info.LastWriteTimeUtc.Ticks)"
}

function Get-TestBuildToolset {
    return "$(Get-TestBuildFileState $global:YoumuTestBuild.Compiler)|$env:INCLUDE|$env:LIB"
}

function Get-TestBuildState([string[]]$Inputs, [string]$HeaderList) {
    $files = @($Inputs)
    if (Test-Path -LiteralPath $HeaderList) { $files += @(Get-Content -LiteralPath $HeaderList) }
    return Get-TestBuildHash (($files | ForEach-Object { Get-TestBuildFileState $_ }) -join "`n")
}

# 방금 끝난 컴파일의 /sourceDependencies JSON에서 읽은 헤더를 모읍니다(컴파일러·SDK 헤더 제외).
function Save-TestBuildHeaders([string]$DependencyDir, [string]$HeaderList) {
    $systemDirs = $global:YoumuTestBuild.SystemDirs
    $headers = foreach ($json in Get-ChildItem -LiteralPath $DependencyDir -Filter '*.json' -ErrorAction SilentlyContinue) {
        foreach ($header in (Get-Content -LiteralPath $json.FullName -Raw | ConvertFrom-Json).Data.Includes) {
            if (-not $header) { continue }
            $lower = $header.ToLowerInvariant()
            if (-not ($systemDirs | Where-Object { $lower.StartsWith($_) })) { $header }
        }
    }
    @($headers | Sort-Object -Unique) | Set-Content -LiteralPath $HeaderList -Encoding utf8
}

function cl {
    $cache = $global:YoumuTestBuild
    $arguments = @($args | ForEach-Object { "$_" })
    # 경고는 오류: 컴파일러 옵션은 맨 앞에, 링커 옵션은 /link 뒤(맨 끝)에 둡니다. 캐시 키에도 들어갑니다.
    $linkerWarnings = @('/WX')
    if ([Array]::IndexOf($arguments, '/link') -lt 0) { $linkerWarnings = @('/link', '/WX') }
    $arguments = @('/WX') + $arguments + $linkerWarnings
    $outputArgument = $arguments | Where-Object { $_ -like '/Fe*' } | Select-Object -First 1
    if ($env:YOUMU_TEST_BUILD_CACHE -eq '0' -or -not $outputArgument) {
        & $cache.Compiler @arguments
        return
    }
    $target = [IO.Path]::GetFullPath([IO.Path]::Combine((Get-Location).Path, $outputArgument.Substring(3)))
    $name = [IO.Path]::GetFileNameWithoutExtension($target)
    # 출력 경로는 러너마다 매번 달라질 수 있으므로(임시 폴더) 같은 빌드인지 판단할 때는 뺍니다.
    $command = @($arguments | Where-Object { $_ -notlike '/Fe*' -and $_ -notlike '/Fo*' })
    $key = Get-TestBuildHash (($command -join "`n") + "`n" + (Get-TestBuildToolset))
    $inputs = @($command | Where-Object { -not $_.StartsWith('/') } |
        ForEach-Object { [IO.Path]::GetFullPath([IO.Path]::Combine((Get-Location).Path, $_)) } |
        Where-Object { [IO.File]::Exists($_) })
    foreach ($argument in $command) {
        if ($argument -notlike '/LIBPATH:*') { continue }
        $libraryDir = [IO.Path]::Combine((Get-Location).Path, $argument.Substring(9))
        if ([IO.Directory]::Exists($libraryDir)) { $inputs += @([IO.Directory]::GetFiles($libraryDir, '*.lib') | Sort-Object) }
    }
    New-Item -ItemType Directory -Force -Path $cache.CacheDir | Out-Null
    $headerList = Join-Path $cache.CacheDir "$name-$key.headers"
    $state = Get-TestBuildState $inputs $headerList
    $cached = Join-Path $cache.CacheDir "$name-$key-$state.exe"
    $outputLog = Join-Path $cache.CacheDir "$name-$key.out"
    if ([IO.File]::Exists($cached)) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
        Copy-Item -LiteralPath $cached -Destination $target -Force
        if (Test-Path -LiteralPath $outputLog) { Get-Content -LiteralPath $outputLog }
        "test build up to date: $name"
        $global:LASTEXITCODE = 0
        return
    }

    # 다시 빌드합니다. 읽은 헤더 목록을 받아 두려고 /link 앞에 /sourceDependencies를 끼웁니다.
    $dependencyDir = Join-Path $cache.CacheDir "$name-$key.deps"
    Remove-Item -LiteralPath $dependencyDir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $dependencyDir | Out-Null
    $linkAt = [Array]::IndexOf($arguments, '/link')
    if ($linkAt -lt 0) { $linkAt = $arguments.Count }
    $build = @($arguments | Select-Object -First $linkAt) + @('/sourceDependencies', "$dependencyDir\") +
        @($arguments | Select-Object -Skip $linkAt)
    & $cache.Compiler @build | Tee-Object -FilePath "$outputLog.new"
    $exitCode = $LASTEXITCODE
    if ($exitCode -eq 0 -and [IO.File]::Exists($target)) {
        Save-TestBuildHeaders $dependencyDir $headerList
        Move-Item -LiteralPath "$outputLog.new" -Destination $outputLog -Force
        Get-ChildItem -LiteralPath $cache.CacheDir -Filter "$name-$key-*.exe" | Remove-Item -Force
        $state = Get-TestBuildState $inputs $headerList
        Copy-Item -LiteralPath $target -Destination (Join-Path $cache.CacheDir "$name-$key-$state.exe") -Force
    }
    Remove-Item -LiteralPath "$outputLog.new" -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $dependencyDir -Recurse -Force -ErrorAction SilentlyContinue
    $global:LASTEXITCODE = $exitCode
}

# 30일 넘게 다시 만들어지지 않은 항목(없어진 러너, 바뀐 명령줄)은 지웁니다.
if (Test-Path -LiteralPath $global:YoumuTestBuild.CacheDir) {
    Get-ChildItem -LiteralPath $global:YoumuTestBuild.CacheDir -File |
        Where-Object { $_.LastWriteTime -lt (Get-Date).AddDays(-30) } | Remove-Item -Force -ErrorAction SilentlyContinue
}
