[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [int]$Iterations = 20,

    [Parameter(Mandatory = $false)]
    [switch]$Build,

    [Parameter(Mandatory = $false)]
    [string]$BuildDir = "build/ninja-msvc-x64",

    [Parameter(Mandatory = $false)]
    [string]$ExePath = "",

    [Parameter(Mandatory = $false)]
    [string]$PatchPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($Iterations -lt 1) {
    throw "Iterations must be >= 1"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $repoRoot

if ($Build) {
    Write-Host "Building HelmBoyStandalone (Release)..." -ForegroundColor Cyan
    cmake --build $BuildDir --config Release --target HelmBoyStandalone -j 8
}

if (-not ($ExePath -match "\\S")) {
    $candidate = Join-Path $repoRoot "$BuildDir/HelmBoyStandalone_artefacts/Release/helmBoy.exe"
    if (Test-Path $candidate) {
        $ExePath = (Resolve-Path $candidate).Path
    }
    else {
        throw "Could not find helmBoy.exe at '$candidate'. Use -ExePath explicitly or run with -Build."
    }
}
else {
    $ExePath = (Resolve-Path $ExePath).Path
}

$argList = @("--benchmark-headless")
if ($PatchPath -match "\\S") {
    $resolvedPatch = (Resolve-Path $PatchPath).Path
    $argList += $resolvedPatch
}

Write-Host "Using executable: $ExePath" -ForegroundColor Green
Write-Host "Iterations: $Iterations" -ForegroundColor Green
if ($argList.Count -gt 1) {
    Write-Host "Patch: $($argList[1])" -ForegroundColor Green
}

$results = @()

for ($i = 1; $i -le $Iterations; $i++) {
    $ms = (Measure-Command { & $ExePath @argList }).TotalMilliseconds
    $results += $ms
    Write-Host ("Run {0,2}: {1,7:N2} ms" -f $i, $ms)
}

$avg = ($results | Measure-Object -Average).Average
$min = ($results | Measure-Object -Minimum).Minimum
$max = ($results | Measure-Object -Maximum).Maximum

Write-Host ""
Write-Host ("AVG(ms): {0:N2} | MIN(ms): {1:N2} | MAX(ms): {2:N2}" -f $avg, $min, $max) -ForegroundColor Yellow
Write-Host "Sorted samples (ms):" -ForegroundColor DarkGray
($results | Sort-Object) -join ", " | Write-Host
