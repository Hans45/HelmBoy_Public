<#
ATTENTION : ce script ne fonctionne pas pour helmBoy, actuellement : la configuration cMake n'est pas correcte.
Avant d'utiliser ce script, créer le dossier .cmake\api\v1\query sous build,
Et y créer un fichier vide : codemodel-v2
New-Item -itemtype Directory -Path ./build/.cmake/api/v1/query
New-Item -ItemType File -Path ./build/.cmake/api/v1/query/codemodel-v2

Lancer ensuite un cmake -S . -B build
#>
#region variables
$buildDir   = "$PSScriptRoot\build"
$replyDir   = Join-Path $buildDir ".cmake\api\v1\reply"
$repoRoot   = Split-Path $buildDir -Parent  # g:\git\helmBoy
#endregion variables
#region functions
# Helpers
function ConvertTo-AbsPath {
    param(
        [string]$path,
        [string]$root
    )
    if ([string]::IsNullOrWhiteSpace($path)) { return $null }

    # Remplacer les / par \ pour Windows
    $p = $path -replace '/', '\'

    # Si déjà absolu, on le garde; sinon on le rend absolu par rapport à $repoRoot
    if ([System.IO.Path]::IsPathRooted($p)) {
        try { return ([System.IO.Path]::GetFullPath($p)).ToLower() } catch { return $p.ToLower() }
    } else {
        $combined = Join-Path $root $p
        try { return ([System.IO.Path]::GetFullPath($combined)).ToLower() } catch { return $combined.ToLower() }
    }
}
#endregion functions
# Collect used files from target-*.json
$usedFilesAbs = New-Object System.Collections.Generic.List[string]

Get-ChildItem $replyDir -Filter "target-*.json" | ForEach-Object {
    $t = Get-Content $_.FullName | ConvertFrom-Json -Depth 6

    # Filtrer seulement les vraies cibles (code compilé)
    if ($t.type -in @("EXECUTABLE","STATIC_LIBRARY","SHARED_LIBRARY","MODULE_LIBRARY")) {
        # 1) CMake "sources" (souvent liste de .cpp)
        if ($t.sources) {
            foreach ($s in $t.sources) {
                $abs = ConvertTo-AbsPath -path $s.path -root $repoRoot
                if ($abs) { $usedFilesAbs.Add($abs) }
            }
        }
        # 2) compileGroups.sources (selon version/générateur)
        if ($t.compileGroups) {
            foreach ($cg in $t.compileGroups) {
                if ($cg.sources) {
                    foreach ($s in $cg.sources) {
                        $abs = ConvertTo-AbsPath -path $s.path -root $repoRoot
                        if ($abs) { $usedFilesAbs.Add($abs) }
                    }
                }
            }
        }
        # 3) headerGroups.sources (headers déclarés)
        if ($t.headerGroups) {
            foreach ($hg in $t.headerGroups) {
                if ($hg.sources) {
                    foreach ($s in $hg.sources) {
                        $abs = ConvertTo-AbsPath -path $s.path -root $repoRoot
                        if ($abs) { $usedFilesAbs.Add($abs) }
                    }
                }
            }
        }
    }
}

# Build a case-insensitive HashSet for comparison
$usedSet = New-Object System.Collections.Generic.HashSet[string]([StringComparer]::OrdinalIgnoreCase)
foreach ($p in $usedFilesAbs | Sort-Object -Unique) { [void]$usedSet.Add($p) }

# Collect all repo files (.cpp, .c, .h, .hpp)
$allFilesAbs = Get-ChildItem $repoRoot -Recurse -File -Include *.cpp, *.c, *.h, *.hpp |
    ForEach-Object { ([System.IO.Path]::GetFullPath($_.FullName)).ToLower() }

# Optional: ignore folders (examples/tests/externals, ajuster à ton projet)
$ignorePatterns = @(
    "\externals\",
    "\build\",
    "\.git\",
    "\.cmake\",
    "\out\",
    "\dist\",
    "\tests\",
    "\examples\"
    "\savedFiles\"
)
$allFilesAbs = $allFilesAbs | Where-Object { $p = $_; -not ($ignorePatterns | Where-Object { $p.Contains($_) }) }

# Compute unused
$unused = @()
foreach ($p in $allFilesAbs) {
    if (-not $usedSet.Contains($p)) { $unused += $p }
}

Write-Host "Fichiers non utilisés :" -ForegroundColor Yellow
$unused | Sort-Object
Write-Host "$($unused.Count) fichiers" -ForegroundColor Yellow

# Créer le dossier savedFiles à la racine du projet
$savedRoot = Join-Path $repoRoot "savedFiles"
if (-not (Test-Path $savedRoot)) {
    New-Item -ItemType Directory -Path $savedRoot | Out-Null
}

foreach ($file in $unused) {
    # Calculer le chemin relatif depuis la racine du repo
    $rel = Resolve-Path $file | ForEach-Object { $_.Path.Substring($repoRoot.Length).TrimStart('\') }
    $destDir = Split-Path (Join-Path $savedRoot $rel) -Parent

    # Créer le dossier cible si besoin
    if (-not (Test-Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    }

    # Déplacer le fichier
    Write-Host "moving $file to $(Join-Path $savedRoot $rel)..."
    Move-Item -Path $file -Destination (Join-Path $savedRoot $rel)
}