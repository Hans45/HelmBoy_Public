[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$Path = '..',
    [Parameter(Mandatory = $false)]
    [string[]]$excludedPaths = @(),
    [Parameter(Mandatory = $false)]
    [string[]]$searchpattern = @('*.cpp', '*.h'),
    [Parameter(Mandatory = $false)]
    [string]$searchedString = 'Colour\(0x(0-9a-fA-F){8}\)',
    [Parameter(Mandatory=$false)]
    [switch]$returnOnlyValues
)

function Get-StringInstances {
    param(
        [string]$Path,
        [string[]]$excludedPaths,
        [string[]]$searchpattern,
        [string]$searchedString
    )
    Clear-Host
    $searchResults = @()

    # Normalise les patterns (trim)
    $patterns = $searchpattern -is [array] ? $searchpattern : $searchpattern.Split(',')
    $patterns = $patterns | ForEach-Object { $_.Trim() }

    # Normalise les exclusions (trim)
    $exclusions = $excludedPaths | ForEach-Object { $_.Trim() }

    foreach ($sp in $patterns) {
        Write-Host "Searching pattern: $sp" -ForegroundColor Green

        Get-ChildItem -Path $Path -Recurse -Filter $sp -File -ErrorAction SilentlyContinue | ForEach-Object {
            $file      = $_
            $fullPath  = $file.FullName
            $parentDir = Split-Path $fullPath -Parent

            # Exclusion: soit par sous-chaîne de dossier, soit par segment exact
            $lookForIt = $true
            foreach ($e in $exclusions) {
                if ($parentDir -like "*\${e}") {
                    Write-Verbose "Excluding file: $fullPath : '$e' found in its path"
                    $lookForIt = $false
                    break
                }
            }

            if ($lookForIt) {
                Write-Verbose "Searching in file: $fullPath"
                $occurrences = Select-String -Path $fullPath -Pattern $searchedString -ErrorAction SilentlyContinue
                if ($occurrences) {
                    foreach ($o in $occurrences) {
                        $searchResults += [PSCustomObject]@{
                            File  = $o.Path
                            Lines = $o.LineNumber
                            Text  = $o.Line
                        }
                    }
                }
            }
        }
    }
    return $searchResults
}

# Appel et affichage final
$results = Get-StringInstances -Path $Path -excludedPaths $excludedPaths -searchpattern $searchpattern -searchedString $searchedString
if ($returnOnlyValues) {
    #$results | Select-Object -ExpandProperty Text
    $results | ForEach-Object { $_.Text -match $searchedString | Out-Null; $matches[0] }
} else {
    $results | Format-Table -AutoSize
}
