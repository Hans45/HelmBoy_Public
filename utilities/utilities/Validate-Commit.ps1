[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [switch]$RunBuild,

    [Parameter(Mandatory = $false)]
    [string]$BuildDir = "build",

    [Parameter(Mandatory = $false)]
    [string]$Generator = "Visual Studio 18 2026",

    [Parameter(Mandatory = $false)]
    [string]$Platform = "x64",

    [Parameter(Mandatory = $false)]
    [string]$Configuration = "Release"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Section([string]$Title) {
    Write-Host "`n=== $Title ===" -ForegroundColor Cyan
}

function Invoke-Checked([string]$Command) {
    Write-Host "> $Command" -ForegroundColor DarkGray
    Invoke-Expression $Command
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed: $Command"
    }
}

Write-Section "Commit Context"
Invoke-Checked "git rev-parse --short HEAD"
Invoke-Checked "git log -1 --pretty=`"%H %s`""
Invoke-Checked "git status --short"

Write-Section "Validation Checklist"
Write-Host "[ ] Configure CMake (Admin/Dev PowerShell)"
Write-Host "[ ] Build mopo"
Write-Host "[ ] Build HelmBoyStandalone"
Write-Host "[ ] Build HelmBoyPlugin_VST3"
Write-Host "[ ] Launch standalone and confirm audible default preset"
Write-Host "[ ] Verify oscilloscope is non-flat"
Write-Host "[ ] Quick filter modulation sweep (no clicks/silence)"

Write-Section "Commands"
$configureCmd = "cmake -S . -B $BuildDir -G `"$Generator`" -A $Platform"
$buildMopoCmd = "cmake --build $BuildDir --config $Configuration --target mopo --parallel"
$buildStandaloneCmd = "cmake --build $BuildDir --config $Configuration --target HelmBoyStandalone --parallel"
$buildVst3Cmd = "cmake --build $BuildDir --config $Configuration --target HelmBoyPlugin_VST3 --parallel"

Write-Host $configureCmd
Write-Host $buildMopoCmd
Write-Host $buildStandaloneCmd
Write-Host $buildVst3Cmd
Write-Host ".\$BuildDir\HelmBoyStandalone_artefacts\$Configuration\helmBoy.exe"

if (-not $RunBuild) {
    Write-Section "Mode"
    Write-Host "Preview mode only. Re-run with -RunBuild to execute commands in this session." -ForegroundColor Yellow
    exit 0
}

Write-Section "Build Execution"
Invoke-Checked $configureCmd
Invoke-Checked $buildMopoCmd
Invoke-Checked $buildStandaloneCmd
Invoke-Checked $buildVst3Cmd

Write-Section "Done"
Write-Host "Build completed. Run manual audio checks now." -ForegroundColor Green