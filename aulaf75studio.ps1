<#
.SYNOPSIS
    AULA F75 Studio unified Windows management script.

.DESCRIPTION
    Single entry point for all AULA F75 Studio Windows operations.
    Select an action by passing it as the first argument.

    Available commands:
      bootstrap   Check / install all prerequisites, then build
      build       CMake configure + compile (quick incremental build)
      test        Run the unit test suite via CTest / Google Test
      run         Launch AulaF75Studio from bin\ (or -AppDir install)
      format      Run clang-format over every C++ source
      clean       Remove the build and bin output
      help        Show this message

.PARAMETER Command
    Action to perform (see commands listed above).  Required.

.PARAMETER QtPath
    Qt6 MSVC 64-bit directory (e.g. C:\Qt\6.11.2\msvc2022_64).
    Auto-detected from environment variables and common paths if omitted.

.PARAMETER BuildDir
    CMake build directory.  Defaults to <repo>\build\msvc-<config>.

.PARAMETER Config
    CMake build configuration (debug or release).  Default: debug.

.PARAMETER Clean
    Wipe the build and bin directories before configuring (full rebuild).

.PARAMETER NoTests
    (build) Configure with tests disabled so only the app is built.

.PARAMETER RunTests
    (build) Run the test suite after a successful build.

.PARAMETER SkipDeploy
    (build) Skip the windeployqt step.

.PARAMETER NoLogging
    (build) Compile out spdlog file logging (AULAF75STUDIO_ENABLE_LOGGING=OFF).

.PARAMETER BuildNumber
    (build) Explicit build number appended as "-<number>" to the version.

.PARAMETER AppDir
    (run) Directory holding an installed AulaF75Studio.exe.

.PARAMETER AppArgs
    (run) Extra arguments for the app (e.g. "--probe").

.PARAMETER Quiet
    (test) Print only the per-target summary instead of detailed per-test output.

.PARAMETER FailFast
    (test) Stop after the first failing test.

.PARAMETER Filter
    (test) Google Test filter pattern (e.g. "ProtocolTest.*").

.PARAMETER Shuffle
    (test) Randomise test execution order.

.PARAMETER Repeat
    (test) Repeat the full test suite multiple times.

.PARAMETER RepeatCount
    (test) Number of repetitions when -Repeat is active.  Default: 10.

.PARAMETER Check
    (format) Report files that would change instead of rewriting them.

.PARAMETER Help
    Show this help message and exit.

.EXAMPLE
    .\aulaf75studio.ps1 help
    .\aulaf75studio.ps1 bootstrap
    .\aulaf75studio.ps1 build
    .\aulaf75studio.ps1 build -Config release -Clean
    .\aulaf75studio.ps1 build -RunTests
    .\aulaf75studio.ps1 test -Filter "ProtocolTest.*"
    .\aulaf75studio.ps1 run -AppArgs "--probe"
    .\aulaf75studio.ps1 format -Check
    .\aulaf75studio.ps1 clean
#>

param(
    [Parameter(Position = 0)]
    [string]$Command      = "",

    # -- Shared ---------------------------------------------------------------
    [string]$QtPath       = "",
    [switch]$Help,

    # -- build ----------------------------------------------------------------
    [string]$BuildDir     = "",
    [ValidateSet('debug', 'release')]
    [string]$Config       = "debug",
    [switch]$Clean,
    [switch]$NoTests,
    [switch]$RunTests,
    [switch]$SkipDeploy,
    [switch]$NoLogging,
    [string]$BuildNumber  = "",

    # -- run ------------------------------------------------------------------
    [string]$AppDir       = "",
    [string]$AppArgs      = "",

    # -- test -----------------------------------------------------------------
    [switch]$Quiet,
    [switch]$FailFast,
    [string]$Filter       = "",
    [switch]$Shuffle,
    [switch]$Repeat,
    [int]   $RepeatCount  = 10,

    # -- format ---------------------------------------------------------------
    [switch]$Check
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = $PSScriptRoot
$AppName  = "AulaF75Studio"

# ==============================================================================
# Shared helpers
# ==============================================================================
function Write-Ok   ([string]$m) { Write-Host "  [OK]    $m" -ForegroundColor Green  }
function Write-Warn ([string]$m) { Write-Host "  [!!]    $m" -ForegroundColor Yellow }
function Write-Err  ([string]$m) { Write-Host "  [ERROR] $m" -ForegroundColor Red    }
function Write-Info ([string]$m) { Write-Host "          $m" -ForegroundColor Gray   }
function Write-Step ([string]$m) {
    Write-Host ""
    Write-Host ("=" * 56) -ForegroundColor Cyan
    Write-Host "  $m"     -ForegroundColor Cyan
    Write-Host ("=" * 56) -ForegroundColor Cyan
}

# -- Shared: paths from selected configuration ---------------------------------
function Get-BuildDir {
    if (-not [string]::IsNullOrEmpty($BuildDir)) { return $BuildDir }
    return Join-Path $RepoRoot "build\msvc-$Config"
}
function Get-BinDir { return Join-Path $RepoRoot "bin" }
function Get-AppExe { return Join-Path (Get-BinDir) "$AppName.exe" }

# -- Shared: run native tool and check exit code -------------------------------
# Run with Continue. Tool stderr stays non-terminating during 2>&1 redirects.
function Invoke-Native {
    param(
        [Parameter(Mandatory)][scriptblock]$Command,
        [Parameter(Mandatory)][string]$FailureMessage
    )
    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try { & $Command } finally { $ErrorActionPreference = $previous }
    if ($LASTEXITCODE -ne 0) { throw $FailureMessage }
}

# -- Shared: Qt6 auto-detection ------------------------------------------------
function Get-QtPath ([string]$supplied) {
    if (-not [string]::IsNullOrEmpty($supplied) -and (Test-Path "$supplied\bin\windeployqt.exe")) {
        return $supplied
    }
    $candidates = @(
        $env:QT_DIR, $env:QT_PATH, $env:Qt6_DIR, $env:QTDIR,
        "C:\Qt\6.11.2\msvc2022_64", "C:\Qt\6.11.0\msvc2022_64",
        "C:\Qt\6.9.1\msvc2022_64",  "C:\Qt\6.8.2\msvc2022_64",
        "C:\Qt\6.8.0\msvc2022_64"
    )
    foreach ($c in $candidates) {
        if (-not [string]::IsNullOrEmpty($c) -and (Test-Path "$c\bin\windeployqt.exe")) { return $c }
    }
    return ""
}

# -- Shared: put MSVC toolchain on PATH for Ninja ------------------------------
function Enter-MsvcEnv {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        Write-Info "MSVC compiler environment already available."
        return
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found. Install Visual Studio or run from a Developer PowerShell."
    }

    $vsPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vsPath) { throw "No Visual Studio installation with the C++ toolset was found." }

    Write-Info "Using Visual Studio at: $vsPath"
    # Reset PATH first. Repeated DevShell imports can overflow environment.
    $env:PATH = [Environment]::GetEnvironmentVariable("PATH", "Machine") + ";" +
                [Environment]::GetEnvironmentVariable("PATH", "User")
    Import-Module (Join-Path $vsPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
    Enter-VsDevShell -VsInstallPath $vsPath -DevCmdArguments "-arch=x64 -host_arch=x64" -SkipAutomaticLocation | Out-Null

    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw "cl.exe still not on PATH after initializing the Visual Studio environment."
    }
}

# -- Shared: add Qt to PATH ----------------------------------------------------
# CTest handles this. Needed only for direct binary launch.
function Add-QtToPath ([string]$qtPath) {
    if ([string]::IsNullOrEmpty($qtPath)) { return }
    $env:PATH = "$qtPath\bin;$env:PATH"
}

# -- Shared: stop running instance before bin replacement ----------------------
function Stop-RunningApp {
    Get-Process -Name $AppName -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}

# -- Shared: locate the Google Test executable --------------------------------
function Get-GTestExe {
    $candidates = @(
        (Join-Path (Get-BuildDir) "tests\aulaf75_gtest.exe"),
        (Join-Path (Get-BuildDir) "aulaf75_gtest.exe")
    )
    foreach ($c in $candidates) { if (Test-Path $c) { return $c } }
    return ""
}

# -- Shared: locate clang-format (PATH, Visual Studio, Qt Creator) -------------
function Get-ClangFormat {
    $onPath = Get-Command clang-format -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -property installationPath
        if ($vsPath) {
            $candidate = Join-Path $vsPath "VC\Tools\Llvm\x64\bin\clang-format.exe"
            if (Test-Path $candidate) { return $candidate }
        }
    }
    $qtCreator = "C:\Qt\Tools\QtCreator\bin\clang\bin\clang-format.exe"
    if (Test-Path $qtCreator) { return $qtCreator }
    return ""
}

# ==============================================================================
# Command: help
# ==============================================================================
function Invoke-ShowHelp {
    Write-Host ""
    Write-Host "  AULA F75 Studio - unified Windows management script" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Usage:  .\aulaf75studio.ps1 <command> [options]"
    Write-Host ""
    Write-Host "  Commands:" -ForegroundColor Cyan
    Write-Host "    bootstrap   Check prerequisites (MSVC, CMake, Ninja, Qt), then build"
    Write-Host "    build       CMake configure + compile, then deploy the Qt runtime"
    Write-Host "    test        Run the unit test suite via CTest / Google Test"
    Write-Host "    run         Launch AulaF75Studio from bin\ (or -AppDir)"
    Write-Host "    format      Run clang-format over every C++ source"
    Write-Host "    clean       Remove the build and bin output"
    Write-Host "    help        Show this message"
    Write-Host ""
    Write-Host "  Common options:" -ForegroundColor Cyan
    Write-Host "    -Config <debug|release>   Build configuration (default: debug)"
    Write-Host "    -Clean                    Wipe build\ and bin\ first"
    Write-Host "    -QtPath <dir>             Qt6 MSVC directory (auto-detected)"
    Write-Host "    -BuildDir <dir>           Override the CMake build directory"
    Write-Host ""
    Write-Host "  build options:" -ForegroundColor Cyan
    Write-Host "    -NoTests                  Don't compile the unit tests"
    Write-Host "    -RunTests                 Run the tests after building"
    Write-Host "    -SkipDeploy               Skip windeployqt"
    Write-Host "    -NoLogging                Compile out spdlog file logging"
    Write-Host ""
    Write-Host "  test options:" -ForegroundColor Cyan
    Write-Host "    -Filter <pattern>         Google Test filter (e.g. ProtocolTest.*)"
    Write-Host "    -Quiet                    Print the summary only, not every test"
    Write-Host "    -FailFast                 Stop at the first failure"
    Write-Host "    -Shuffle                  Randomise execution order"
    Write-Host "    -Repeat [-RepeatCount N]  Repeat the suite (default 10)"
    Write-Host ""
    Write-Host "  run options:" -ForegroundColor Cyan
    Write-Host "    -AppDir <dir>             Launch an installed copy instead of bin\"
    Write-Host "    -AppArgs <args>           Pass CLI switches (--probe, --dump, --hid-list, ...)"
    Write-Host ""
    Write-Host "  format options:" -ForegroundColor Cyan
    Write-Host "    -Check                    List files that need formatting; exit 1 if any"
    Write-Host ""
    Write-Host "  Examples:" -ForegroundColor Cyan
    Write-Host "    .\aulaf75studio.ps1 build"
    Write-Host "    .\aulaf75studio.ps1 build -Config release -Clean"
    Write-Host "    .\aulaf75studio.ps1 build -RunTests"
    Write-Host "    .\aulaf75studio.ps1 test -Filter `"ProfileStoreTest.*`""
    Write-Host "    .\aulaf75studio.ps1 run -AppArgs `"--probe`""
    Write-Host ""
}

# ==============================================================================
# Command: bootstrap
# ==============================================================================
function Invoke-Bootstrap {
    Write-Step "Bootstrap: checking prerequisites"

    $missing = @()

    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        Write-Ok "MSVC compiler (cl.exe) on PATH"
    } else {
        $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vswhere) {
            $vsPath = & $vswhere -latest -products * `
                -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            if ($vsPath) { Write-Ok "Visual Studio C++ toolset: $vsPath" }
            else { Write-Err "Visual Studio found, but without the C++ toolset."; $missing += "MSVC C++ build tools" }
        } else {
            Write-Err "Visual Studio / Build Tools not found."
            $missing += "Visual Studio 2022 with 'Desktop development with C++'"
        }
    }

    foreach ($tool in @("cmake", "ninja")) {
        $found = Get-Command $tool -ErrorAction SilentlyContinue
        if ($found) { Write-Ok "$tool ($($found.Source))" }
        else { Write-Err "$tool not found on PATH."; $missing += $tool }
    }

    $qt = Get-QtPath $QtPath
    if ($qt) { Write-Ok "Qt6: $qt" }
    else { Write-Warn "Qt6 not found; set -QtPath or QT_DIR (windeployqt will be skipped)." }

    if ($missing.Count -gt 0) {
        Write-Host ""
        Write-Err "Missing prerequisites:"
        $missing | ForEach-Object { Write-Info "- $_" }
        Write-Host ""
        Write-Info "Install them, then re-run:  .\aulaf75studio.ps1 bootstrap"
        exit 1
    }

    Write-Host ""
    Write-Ok "All prerequisites satisfied."
    Invoke-Build
}

# ==============================================================================
# Command: clean
# ==============================================================================
function Invoke-Clean {
    Write-Step "Clean"
    Stop-RunningApp
    foreach ($dir in @((Get-BuildDir), (Get-BinDir))) {
        if (Test-Path $dir) {
            Write-Info "Removing $dir"
            Remove-Item -Recurse -Force $dir
        }
    }
    Write-Ok "Build output removed."
}

# ==============================================================================
# Command: build
# ==============================================================================
function Invoke-Build {
    Write-Step "Build $AppName ($Config)"

    if ($Clean) { Invoke-Clean }

    Enter-MsvcEnv

    $buildPath = Get-BuildDir
    $testsFlag = if ($NoTests) { "OFF" } else { "ON" }
    $loggingFlag = if ($NoLogging) { "OFF" } else { "ON" }

    # Explicit build number adds "-<number>" suffix.
    # Clear stale cached value for normal build.
    $cmakeArgs = @("--preset", "msvc-$Config", "-DAULAF75STUDIO_BUILD_TESTS=$testsFlag",
                   "-DAULAF75STUDIO_ENABLE_LOGGING=$loggingFlag")
    $qt = Get-QtPath $QtPath
    if ($qt) { $cmakeArgs += "-DCMAKE_PREFIX_PATH=$($qt -replace '\\', '/')" }
    if ($BuildNumber) { $cmakeArgs += "-DAULAF75STUDIO_BUILD_NUMBER=$BuildNumber" }
    else              { $cmakeArgs += "-UAULAF75STUDIO_BUILD_NUMBER" }

    Write-Info ("cmake " + ($cmakeArgs -join ' '))
    Invoke-Native { cmake @cmakeArgs } "CMake configuration failed."

    Write-Info "cmake --build $buildPath"
    Stop-RunningApp
    Invoke-Native { cmake --build $buildPath } "Build failed."

    Write-Ok "Build succeeded: $(Get-AppExe)"

    if (-not $SkipDeploy) {
        # Deploy Qt runtime into bin. App runs standalone.
        if ($qt) {
            Write-Info "windeployqt -> $(Get-BinDir)"
            $deployMode = if ($Config -eq "debug") { "--debug" } else { "--release" }
            Invoke-Native {
                & (Join-Path $qt "bin\windeployqt.exe") $deployMode `
                    --qmldir (Join-Path $RepoRoot "Views") (Get-AppExe)
            } "windeployqt failed."
            Write-Ok "Qt runtime deployed."
        } else {
            Write-Warn "windeployqt not found; bin\ may be missing the Qt runtime."
        }
    }

    if ($RunTests) { Invoke-Test }
}

# ==============================================================================
# Command: test
# ==============================================================================
function Invoke-Test {
    Write-Step "Test suite ($Config)"

    $buildPath = Get-BuildDir
    if (-not (Test-Path $buildPath)) {
        Write-Err "Build directory '$buildPath' not found. Build first: .\aulaf75studio.ps1 build"
        exit 1
    }

    # Google Test options apply only to gtest binary.
    # Otherwise run whole CTest suite.
    $usesGTestOptions = $Filter -or $FailFast -or $Shuffle -or $Repeat

    if ($usesGTestOptions) {
        $exe = Get-GTestExe
        if (-not $exe) {
            Write-Err "aulaf75_gtest.exe not found. Build with tests enabled first."
            exit 1
        }

        $gtestArgs = @()
        if ($Filter)   { $gtestArgs += "--gtest_filter=$Filter" }
        if ($FailFast) { $gtestArgs += "--gtest_break_on_failure" }
        if ($Shuffle)  { $gtestArgs += "--gtest_shuffle" }
        if ($Repeat)   { $gtestArgs += "--gtest_repeat=$RepeatCount" }

        Write-Info "$([System.IO.Path]::GetFileName($exe)) $($gtestArgs -join ' ')"
        Add-QtToPath (Get-QtPath $QtPath)
        Push-Location (Split-Path $exe -Parent)
        try { Invoke-Native { & $exe @gtestArgs } "Tests failed." } finally { Pop-Location }
    } else {
        $ctestArgs = @("--test-dir", $buildPath, "--output-on-failure")
        if (-not $Quiet) { $ctestArgs += "--verbose" }
        if ($FailFast)   { $ctestArgs += "--stop-on-failure" }

        Write-Info "ctest $($ctestArgs -join ' ')"
        Invoke-Native { ctest @ctestArgs } "Tests failed."
    }

    Write-Ok "All tests passed."
}

# ==============================================================================
# Command: format
# ==============================================================================
function Invoke-Format {
    Write-Step "clang-format"

    $clangFormat = Get-ClangFormat
    if (-not $clangFormat) {
        Write-Err "clang-format not found (PATH, Visual Studio LLVM, or Qt Creator)."
        exit 1
    }
    Write-Info "Using $clangFormat"

    $files = Get-ChildItem -Path (Join-Path $RepoRoot "Controllers"), (Join-Path $RepoRoot "Model"),
                                 (Join-Path $RepoRoot "tests") -Recurse -Include *.cpp, *.h
    # Root sources only. -Depth recurses, so glob the directory itself.
    $files += Get-ChildItem -Path (Join-Path $RepoRoot "*") -Include *.cpp, *.h -File
    $paths = $files | Select-Object -ExpandProperty FullName | Sort-Object -Unique

    if ($Check) {
        $dirty = @()
        foreach ($p in $paths) {
            & $clangFormat --dry-run --Werror $p 2>$null
            if ($LASTEXITCODE -ne 0) { $dirty += $p }
        }
        if ($dirty.Count -gt 0) {
            Write-Err "Files need formatting:"
            $dirty | ForEach-Object { Write-Info "- $_" }
            exit 1
        }
        Write-Ok "All $($paths.Count) files are formatted."
    } else {
        Invoke-Native { & $clangFormat -i @paths } "clang-format failed."
        Write-Ok "Formatted $($paths.Count) files."
    }
}

# ==============================================================================
# Command: run
# ==============================================================================
function Invoke-Run {
    # Prefer explicit -AppDir install. Else use dev bin\ folder.
    if (-not [string]::IsNullOrEmpty($AppDir)) {
        $exePath = Join-Path $AppDir "$AppName.exe"
        if (-not (Test-Path $exePath)) {
            Write-Err "$AppName.exe not found in '$AppDir'."
            exit 1
        }
    } else {
        $exePath = Get-AppExe
        if (-not (Test-Path $exePath)) {
            Write-Err "bin\$AppName.exe not found. Build first: .\aulaf75studio.ps1 build"
            exit 1
        }
    }

    Add-QtToPath (Get-QtPath $QtPath)
    if ($AppArgs) {
        # CLI switches print JSON to stdout. Run inline and forward the exit code.
        Write-Info "Running: $exePath $AppArgs"
        & $exePath ($AppArgs -split ' ')
        exit $LASTEXITCODE
    }

    Stop-RunningApp
    Write-Info "Launching: $exePath"
    Start-Process -FilePath $exePath -WorkingDirectory (Split-Path $exePath -Parent)
    Write-Ok "$AppName started."
}

# ==============================================================================
# Main dispatch
# ==============================================================================
if ($Help -or [string]::IsNullOrEmpty($Command) -or $Command -eq "help") {
    Invoke-ShowHelp
    exit 0
}

switch ($Command.ToLower()) {
    "bootstrap" { Invoke-Bootstrap }
    "build"     { Invoke-Build     }
    "test"      { Invoke-Test      }
    "run"       { Invoke-Run       }
    "format"    { Invoke-Format    }
    "clean"     { Invoke-Clean     }
    default {
        Write-Host ""
        Write-Host "  [ERROR] Unknown command: '$Command'" -ForegroundColor Red
        Write-Host "  Run  .\aulaf75studio.ps1 help  for usage." -ForegroundColor Yellow
        Write-Host ""
        exit 1
    }
}
