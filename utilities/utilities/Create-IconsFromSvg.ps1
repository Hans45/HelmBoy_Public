param (
    [Parameter(Mandatory = $true, HelpMessage = 'Le chemin vers le fichier SVG source.')]
    [string]$FichierSource,

    [Parameter(Mandatory = $true, HelpMessage = 'Le dossier de destination où placer les fichiers.')]
    [string]$Destination,

    [Parameter(Mandatory = $false, HelpMessage = "La ou les couleurs à générer (ex: black, white, '#FF0000').")]
    [string[]]$Couleurs = @('black', 'white'),

    [Parameter(Mandatory = $false, HelpMessage = 'Le préfixe de nom utilisé pour les fichiers de sortie.')]
    [string]$NamePrefix = 'helmboy_icon',

    [Parameter(Mandatory = $true, HelpMessage = 'Le chemin vers le fichier PNG source pour Inno Setup.')]
    [string]$FichierSourceInnoSetup
)

if (-not (Test-Path $FichierSource)) {
    Write-Error "Le fichier source '$FichierSource' est introuvable."
    exit
}

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

if (-not (Test-Path $Destination)) {
    Write-Host "Création du dossier de destination : $Destination" -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
}

$taillesPng = 16, 22, 24, 32, 48, 64, 128, 256, 512, 1024
$taillesIco = 16, 24, 32, 48, 64, 128, 256

foreach ($couleur in $couleurs) {
    $nomSousDossier = $couleur -replace '#', 'hex_'
    $cheminDossierCouleur = Join-Path $Destination "Icones_$nomSousDossier"

    if (-not (Test-Path $cheminDossierCouleur)) {
        New-Item -ItemType Directory -Path $cheminDossierCouleur | Out-Null
    }

    Write-Host "Génération du jeu [$couleur] dans le dossier : $cheminDossierCouleur" -ForegroundColor Cyan

    # 1. Génération des fichiers PNG (Proportions gardées et transparence préservée)
    foreach ($t in $taillesPng) {
        # Sécurisation du nom de fichier avec ${NamePrefix}
        $ciblePng = Join-Path $cheminDossierCouleur "${NamePrefix}_${t}_1x.png"

        magick -background transparent $FichierSource -channel RGB -fill $couleur -colorize 100% +channel -resize "${t}x${t}" -gravity center -background transparent -extent "${t}x${t}" $ciblePng
    }

    # 2. Génération du fichier ICO multi-résolutions
    $fichiersTemporairesIco = @()
    foreach ($t in $taillesIco) {
        $tempPng = [System.IO.Path]::GetTempFileName() + '.png'

        magick -background transparent $FichierSource -channel RGB -fill $couleur -colorize 100% +channel -resize "${t}x${t}" -gravity center -background transparent -extent "${t}x${t}" $tempPng
        $fichiersTemporairesIco += $tempPng
    }

    $cibleIco = Join-Path $cheminDossierCouleur "$NamePrefix.ico"
    magick $fichiersTemporairesIco $cibleIco

    foreach ($f in $fichiersTemporairesIco) {
        if (Test-Path $f) { Remove-Item $f -Force }
    }

    # 3. Génération du fichier XPM
    $cibleXpm = Join-Path $cheminDossierCouleur "$NamePrefix.xpm"
    magick -background transparent $FichierSource -channel RGB -fill $couleur -colorize 100% +channel $cibleXpm

    # 4. Copie des fichiers vers des noms différents pour les besoins de HelmBoy (Corrigé avec ${NamePrefix} et -Force)
    Copy-Item $(Join-Path $cheminDossierCouleur "${NamePrefix}_32_1x.png") -Destination $(Join-Path $cheminDossierCouleur "${NamePrefix}_16_2x.png") -Force
    Copy-Item $(Join-Path $cheminDossierCouleur "${NamePrefix}_64_1x.png") -Destination $(Join-Path $cheminDossierCouleur "${NamePrefix}_32_2x.png") -Force
    Copy-Item $(Join-Path $cheminDossierCouleur "${NamePrefix}_256_1x.png") -Destination $(Join-Path $cheminDossierCouleur "${NamePrefix}_128_2x.png") -Force
    Copy-Item $(Join-Path $cheminDossierCouleur "${NamePrefix}_512_1x.png") -Destination $(Join-Path $cheminDossierCouleur "${NamePrefix}_256_2x.png") -Force
    Copy-Item $(Join-Path $cheminDossierCouleur "${NamePrefix}_1024_1x.png") -Destination $(Join-Path $cheminDossierCouleur "${NamePrefix}_512_2x.png") -Force
}

$InnoSetupGraphicsFolder = Join-Path $Destination "Inno Setup Graphics"
if (-not (Test-Path $InnoSetupGraphicsFolder)) {
    New-Item -ItemType Directory -Path $InnoSetupGraphicsFolder | Out-Null
}

Write-Host "Génération du fichier helmBoy_wizard.png dans le dossier $InnoSetupGraphicsFolder" -ForegroundColor Cyan
magick -background transparent $FichierSourceInnoSetup -resize '534x534' -gravity center -extent '534x1022' -background transparent "$(Join-Path $InnoSetupGraphicsFolder 'helmBoy_wizard.png')"
Write-Host "Génération de l'icône helmBoy.ico dans le dossier $InnoSetupGraphicsFolder" -ForegroundColor Cyan
$fichiersTemporairesIco = @()
foreach ($t in $taillesIco) {
    $tempPng = [System.IO.Path]::GetTempFileName() + '.png'

    magick -background transparent $FichierSourceInnoSetup -resize "${t}x${t}" -gravity center -background transparent -extent "${t}x${t}" $tempPng
    $fichiersTemporairesIco += $tempPng
}

$cibleIco = Join-Path $InnoSetupGraphicsFolder "helmBoy.ico"
magick $fichiersTemporairesIco "$cibleIco"

foreach ($f in $fichiersTemporairesIco) {
    if (Test-Path $f) { Remove-Item $f -Force }
}

Write-Host "`nTerminé ! Le contenu et la transparence sont désormais parfaits." -ForegroundColor Green
