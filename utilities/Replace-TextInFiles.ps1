[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$Path = '..',
    [Parameter(Mandatory = $false)]
    [string[]]$excludedPaths = @('build','externals','look_and_feel'),
    [Parameter(Mandatory = $false)]
    [string[]]$searchpattern = @('*.cpp', '*.h'),
    [Parameter(Mandatory = $false)]
    [string]$refFile = './ListeCouleursEtRemplacements.csv'
)

function Set-TxtInFiles {
    param(
        [string]$Path,
        [string[]]$excludedPaths,
        [string[]]$searchpattern,
        [string]$refFile
    )
    Clear-Host
    #Extraction des paires valeur d'origine / Texte de remplacement
    $ListeRemplacements = Get-Content $refFile | convertfrom-csv -delimiter ';' | select-object ValeurDOrigine, Remplacement
    # Normalise les patterns (trim)
    $patterns = $searchpattern -is [array] ? $searchpattern : $searchpattern.Split(',')
    $patterns = $patterns | ForEach-Object { $_.Trim() }

    # Normalise les exclusions (trim)
    $exclusions = $excludedPaths | ForEach-Object { $_.Trim() }

    #Constitution de la liste des fichiers à examiner pour tenter les modifications
    $filesToModify = @()
    foreach ($sp in $patterns) {
        Write-Host "Searching pattern: $sp" -ForegroundColor Green
        Start-Sleep -Seconds 2
        Get-ChildItem -Path $Path -Recurse -Filter $sp -File -ErrorAction SilentlyContinue | ForEach-Object {
            $file      = $_
            $fullPath  = $file.FullName
            $parentDir = Split-Path $fullPath -Parent

            # Exclusion robuste : vérifie chaque segment du chemin
            $lookForIt = $true
            $segments = $parentDir -split '[\\/]'  # Découpe le chemin en segments
            foreach ($e in $exclusions) {
                write-verbose "Checking the presence of $e in $fullPath..."
                if ($segments -contains $e) {
                    Write-Verbose "Excluding file: $fullPath : '$e' found in its path"
                    $lookForIt = $false
                    break
                }
            }

            if ($lookForIt) {
                Write-Verbose "Adding file: $fullPath to list to check for modifications"
                $filesToModify += $file
            }
        }
    }
    foreach($rp in $ListeRemplacements){
        $origine = $rp | Select-Object -ExpandProperty ValeurDOrigine
        $remplacement = $rp | Select-Object -ExpandProperty Remplacement
        Write-Host "Looking for $origine to replace it with $remplacement" -ForegroundColor Yellow
        Start-Sleep -Seconds 2
            foreach ($fileToCheck in $filesToModify) {
                Write-Verbose "Checking $fileToCheck..."
                try {
                    $content = Get-Content $fileToCheck.FullName -Raw
                    $newContent = $content -replace "$origine", "$remplacement"
                    if ($newContent -ne $content) {Write-Host "$origine found in $fileToCheck!" -ForegroundColor Cyan}
                    Set-Content $fileToCheck.FullName $newContent
                } catch {
                    Write-Warning "Impossible de modifier $($fileToCheck.FullName) : $($_.Exception.Message)"
                }
            }
    }
}

# Appel et affichage final
Set-TxtInFiles -Path $Path -excludedPaths $excludedPaths -searchpattern $searchpattern -refFile $refFile