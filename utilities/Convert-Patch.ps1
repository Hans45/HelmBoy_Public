<#
.SYNOPSIS
Converts a Helm .helm patch to the helmBoy .helmboy format.

.DESCRIPTION
Adds missing parameter defaults from src/common/helmBoy_common.cpp, preserves
existing patch values and metadata, and writes the converted patch beside the
source file.

.PARAMETER InputPath
Path to the .helm patch to convert.

.EXAMPLE
.\Convert-Patch.ps1 -InputPath "C:\Patches\My Patch.helm"
#>
[CmdletBinding()]
param(
	[Parameter(Mandatory = $true, Position = 0)]
	[ValidateNotNullOrEmpty()]
	[string]$InputPath
)

$ErrorActionPreference = 'Stop'

function Get-HelmBoyParameterDefaults {
	param([string]$SourcePath)

	$source = Get-Content -LiteralPath $SourcePath -Raw
	if ($source -notmatch '(?sm)ValueDetailsLookup::parameter_list\[\]\s*=\s*\{(?<entries>.*?)^\s*\};') {
		throw "Could not find ValueDetailsLookup::parameter_list in '$SourcePath'."
	}
	$entries = $Matches['entries']

	$defaults = @{}
	foreach ($entry in ($entries -split '\{\s*"')) {
		if ($entry -notmatch '(?s)^\s*(?<name>[^"]+)"\s*,(?<fields>[^}]*)') {
			continue
		}

		$name = $Matches['name']
		$entryFields = $Matches['fields']
		if ($entryFields -match '//\s*DEPRECATED\b') {
			continue
		}

		$fields = @($entryFields -split ',')
		if ($fields.Count -lt 4) {
			throw "Could not read the default value for parameter '$name'."
		}

		$defaultText = $fields[3].Trim()
		if ($defaultText -notmatch '^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$') {
			throw "Default value '$defaultText' for parameter '$name' is not a numeric literal."
		}
		try {
			$defaultValue = ConvertFrom-Json -InputObject $defaultText
		}
		catch {
			throw "Default value '$defaultText' for parameter '$name' is not a JSON-compatible number."
		}
		$defaults[$name] = $defaultValue
	}

	if ($defaults.Count -eq 0) {
		throw "No active parameter defaults were read from '$SourcePath'."
	}

	return $defaults
}

$resolvedInput = (Resolve-Path -LiteralPath $InputPath).Path
$inputFile = Get-Item -LiteralPath $resolvedInput
if ($inputFile.Extension -ine '.helm') {
	throw "Input file must have the .helm extension: '$resolvedInput'."
}

$sourcePath = Join-Path $PSScriptRoot '..\src\common\helmBoy_common.cpp'
if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
	throw "Could not find the parameter source file at '$sourcePath'."
}

$patch = Get-Content -LiteralPath $resolvedInput -Raw | ConvertFrom-Json
$settingsProperty = $patch.PSObject.Properties['settings']
if ($null -eq $settingsProperty -or $null -eq $settingsProperty.Value -or
		$settingsProperty.Value -is [array] -or $settingsProperty.Value -is [string]) {
	throw "The input patch must contain a JSON 'settings' object."
}

$parameterDefaults = Get-HelmBoyParameterDefaults -SourcePath $sourcePath
$settingValues = @{}
foreach ($property in $settingsProperty.Value.PSObject.Properties) {
	$settingValues[$property.Name] = $property.Value
}

$addedCount = 0
foreach ($name in $parameterDefaults.Keys) {
	if ($settingValues.Keys -notcontains $name) {
		$settingValues[$name] = $parameterDefaults[$name]
		$addedCount++
	}
}

$sortRecords = @()
foreach ($name in $settingValues.Keys) {
	if ($name -eq 'modulations') {
		continue
	}
	$sortKey = ''
	for ($index = 0; $index -lt $name.Length; $index++) {
		$sortKey += '{0:D5}' -f [int][char]$name[$index]
	}
	$sortRecords += $sortKey + "`t" + $name
}
$settingNames = @($sortRecords | Sort-Object | ForEach-Object { $_ -replace '^\d+\t', '' })
$orderedSettings = [ordered]@{}
foreach ($name in $settingNames) {
	$orderedSettings[$name] = $settingValues[$name]
}
if ($settingValues.ContainsKey('modulations')) {
	$orderedSettings['modulations'] = $settingValues['modulations']
}

$orderedPatch = [ordered]@{}
foreach ($name in @('license', 'synth_version', 'patch_name', 'folder_name', 'author')) {
	$property = $patch.PSObject.Properties[$name]
	if ($null -ne $property) {
		$orderedPatch[$name] = $property.Value
	}
}
foreach ($property in $patch.PSObject.Properties) {
	if ($property.Name -ne 'settings' -and $orderedPatch.Keys -notcontains $property.Name) {
		$orderedPatch[$property.Name] = $property.Value
	}
}
$orderedPatch['settings'] = $orderedSettings

$outputPath = Join-Path (Split-Path -Parent $resolvedInput) ($inputFile.BaseName + '.helmboy')
$json = ConvertTo-Json -InputObject $orderedPatch -Depth 100
Set-Content -LiteralPath $outputPath -Value $json -Encoding utf8

Write-Host "Converted '$resolvedInput' to '$outputPath'. Added $addedCount defaults from $($parameterDefaults.Count) active parameters."
