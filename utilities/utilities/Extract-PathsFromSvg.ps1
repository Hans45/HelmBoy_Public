[cmdletbinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SvgFilePath
)

# 1. On s'assure que la liste est initialisée proprement
$result = New-Object System.Collections.Generic.List[string]

# 2. Chargement du XML
[xml]$svg = Get-Content $SvgFilePath -Raw

# 3. Sécurité : On force $paths à être traité comme un tableau (même s'il n'y a qu'un seul chemin)
$paths = @($svg.svg.path)

foreach ($path in $paths) {
    # Si la balise <path> ou l'attribut d n'existent pas, on passe au suivant
    if ($null -eq $path -or $null -eq $path.d) { continue }

    # Extraction propre en texte
    $currentPathData = $path.d.ToString()
    Write-Host "Path trouvé : $currentPathData"

    try {
        # Sécurité ultime : si par un mystère $result est devenu null, on le recrée à la volée
        if ($null -eq $result) {
            $result = New-Object System.Collections.Generic.List[string]
        }

        $result.Add("""$currentPathData""")
    }
    catch {
        Write-Host "Failed to add path: $($_.Exception.Message)" -ForegroundColor Red
    }
}

# Retourne le résultat final
set-clipboard -Value ($result -join "`n")
