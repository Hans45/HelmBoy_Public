[CmdletBinding()]
param(
    [string]$BuildDirectory = "build/svf-tests",
    [string]$Generator = "",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [ValidateRange(1, 256)]
    [int]$Parallelism = [Math]::Max(1, [Environment]::ProcessorCount),
    [switch]$SkipConfigure,
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $buildPath = [System.IO.Path]::GetFullPath($BuildDirectory)
}
else {
    $buildPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDirectory))
}

$cmakePath = (Get-Command cmake -CommandType Application -ErrorAction Stop).Source
$ctestPath = (Get-Command ctest -CommandType Application -ErrorAction Stop).Source
$reportDirectory = Join-Path $buildPath "test-reports"
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null

$runId = Get-Date -Format "yyyyMMdd-HHmmss"
$logPath = Join-Path $reportDirectory "svf-tests-$runId.log"
$junitPath = Join-Path $reportDirectory "svf-tests-$runId.junit.xml"
$summaryPath = Join-Path $reportDirectory "svf-tests-$runId.summary.txt"

function Invoke-NativeCommand {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$Description
    )

    Write-Host "`n=== $Description ===" -ForegroundColor Cyan
    $displayArguments = $Arguments -join " "
    $commandLine = "> $Executable $displayArguments"
    Write-Host $commandLine -ForegroundColor DarkGray
    Add-Content -LiteralPath $logPath -Value "`n$Description`n$commandLine"

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = @(& $Executable @Arguments 2>&1)
        $exitCode = $LASTEXITCODE
    }
    catch {
        $output = @($_.Exception.Message)
        $exitCode = -1
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    foreach ($line in $output) {
        $text = [string]$line
        Write-Host $text
        Add-Content -LiteralPath $logPath -Value $text
    }

    return [PSCustomObject]@{
        Description = $Description
        ExitCode = $exitCode
    }
}

function Assert-NativeSuccess {
    param([PSCustomObject]$Result)

    if ($Result.ExitCode -ne 0) {
        throw "$($Result.Description) failed with exit code $($Result.ExitCode). See $logPath"
    }
}

Write-Host "HelmBoy SVF tests" -ForegroundColor Green
Write-Host "Build directory: $buildPath"
Write-Host "Configuration:   $Configuration"
Write-Host "Run log:         $logPath"

if (-not $SkipConfigure) {
    $configureArguments = @(
        "-S", $repoRoot,
        "-B", $buildPath,
        "-DHELMBOY_RUN_TESTS=ON",
        "-DHELMBOY_BUILD_SVF_TESTS=ON",
        "-DCMAKE_BUILD_TYPE=$Configuration"
    )
    if (-not [string]::IsNullOrWhiteSpace($Generator)) {
        $configureArguments += @("-G", $Generator)
    }

    $configureResult = Invoke-NativeCommand -Executable $cmakePath `
        -Arguments $configureArguments -Description "Configure SVF test target"
    Assert-NativeSuccess $configureResult
}

if (-not $SkipBuild) {
    $buildArguments = @(
        "--build", $buildPath,
        "--config", $Configuration,
        "--target", "HelmBoySVFTests",
        "--parallel", "$Parallelism"
    )
    $buildResult = Invoke-NativeCommand -Executable $cmakePath `
        -Arguments $buildArguments -Description "Build SVF test target"
    Assert-NativeSuccess $buildResult
}

$testArguments = @(
    "--test-dir", $buildPath,
    "-C", $Configuration,
    "-R", "^StateVariableFilterTests$",
    "--output-on-failure",
    "--output-junit", $junitPath,
    "--no-tests=error"
)
$testResult = Invoke-NativeCommand -Executable $ctestPath `
    -Arguments $testArguments -Description "Run StateVariableFilterTests"

if (-not (Test-Path -LiteralPath $junitPath)) {
    throw "CTest did not produce the expected JUnit report. See $logPath"
}

try {
    [xml]$junit = Get-Content -LiteralPath $junitPath -Raw
}
catch {
    throw "Could not parse CTest JUnit report '$junitPath': $($_.Exception.Message)"
}

$testCases = @($junit.SelectNodes("//testcase"))
$failureCases = @($junit.SelectNodes("//testcase/failure | //testcase/error"))
$skippedCases = @($junit.SelectNodes("//testcase/skipped"))
$passedCount = $testCases.Count - $failureCases.Count - $skippedCases.Count
$runStatus = if ($testResult.ExitCode -eq 0 -and $failureCases.Count -eq 0) { "PASS" } else { "FAIL" }

$summaryLines = @(
    "HelmBoy SVF test summary",
    "Result: $runStatus",
    "Configuration: $Configuration",
    "Build directory: $buildPath",
    "Tests: $($testCases.Count); passed: $passedCount; failed: $($failureCases.Count); skipped: $($skippedCases.Count)",
    "CTest exit code: $($testResult.ExitCode)",
    "JUnit report: $junitPath",
    "Run log: $logPath"
)

if ($failureCases.Count -gt 0) {
    $summaryLines += ""
    $summaryLines += "Failures:"
    foreach ($failure in $failureCases) {
        $case = $failure.ParentNode
        $summaryLines += "- $($case.GetAttribute('name')): $($failure.InnerText.Trim())"
    }
}

Set-Content -LiteralPath $summaryPath -Value $summaryLines -Encoding UTF8
Write-Host "`n=== Analyse ===" -ForegroundColor Cyan
Write-Host "Result: $runStatus"
Write-Host "Tests: $($testCases.Count); passed: $passedCount; failed: $($failureCases.Count); skipped: $($skippedCases.Count)"
foreach ($failure in $failureCases) {
    $case = $failure.ParentNode
    Write-Host "FAIL: $($case.GetAttribute('name'))" -ForegroundColor Red
    Write-Host $failure.InnerText.Trim()
}
Write-Host "Summary: $summaryPath"

if ($runStatus -ne "PASS") {
    exit 1
}