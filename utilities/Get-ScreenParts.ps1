param (
    [Parameter(Mandatory = $true, HelpMessage = "Le chemin vers l'image source à découper.")]
    [string]$FichierSource,

    [Parameter(Mandatory = $true, HelpMessage = 'Le dossier de destination où placer les morceaux découpés.')]
    [string]$Destination
)

# 1. Vérification de l'existence du fichier source
if (-not (Test-Path $FichierSource)) {
    Write-Error "Le fichier source '$FichierSource' est introuvable."
    exit
}

# 2. Création du dossier de destination s'il n'existe pas
if (-not (Test-Path $Destination)) {
    Write-Host "Création du dossier de destination : $Destination" -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
}

# 3. Définition de votre liste de coordonnées
# Format de la chaîne : "NomFichierSortie, Largeur, Hauteur, CoordonnéeX, CoordonnéeY"
$Zones = @(
    'logo,1,1,1,1',
    'Browser, 128, 128, 50, 40',
    'volume, 128, 128, 50, 40',
    'oscilloscope, 128, 128, 50, 40',
    'limiter, 128, 128, 50, 40',
    'BPM, 128, 128, 50, 40',
    'Arp, 128, 128, 50, 40',
    'Oscillators, 128, 128, 50, 40',
    'Ocillator, 128, 128, 50, 40',
    'Sub, 128, 128, 50, 40',
    'Mixer, 128, 128, 50, 40',
    'Amplitude Envelope, 128, 128, 50, 40',
    'Filter Envelope, 128, 128, 50, 40',
    'Mod Envelope, 128, 128, 50, 40',
    'Step Sequencer, 128, 128, 50, 40',
    'Keyboard, 128, 128, 50, 40',
    'Feedback, 128, 128, 50, 40',
    'Filter, 128, 128, 50, 40',
    'Stutter, 128, 128, 50, 40',
    'Chorus, 128, 128, 50, 40',
    'Formant, 128, 128, 50, 40',
    'Distortion, 128, 128, 50, 40',
    'Delay, 128, 128, 50, 40',
    'Reverb, 128, 128, 50, 40',
    'LFOs, 128, 128, 50, 40',
    'Mono LFO, 128, 128, 50, 40',
    'Poly LFO, 128, 128, 50, 40',
    'Keyboard Mod, 128, 128, 50, 40',
    'Pitch and Voices, 128, 128, 50, 40',
    'Portamento, 128, 128, 50, 40'
)

Write-Host "Début du découpage de l'image..." -ForegroundColor Cyan

foreach ($zone in $Zones) {
    # Découpage de la ligne par les virgules et nettoyage des espaces vides
    $data = $zone -split ','
    $nom = $data[0].Trim()
    $w = $data[1].Trim()
    $h = $data[2].Trim()
    $x = $data[3].Trim()
    $y = $data[4].Trim()

    # Construction du chemin de sortie final (ex: .\images\NewPictures\tete_decoupe.png)
    $fichierSortie = Join-Path $Destination "${nom}_decoupe.png"

    Write-Host "Extraction de [$nom] (${w}x${h} aux coordonnées X:$x, Y:$y)..." -ForegroundColor Gray

    # Exécution de la commande ImageMagick
    magick $FichierSource -crop "${w}x${h}+${x}+${y}" $fichierSortie
}

Write-Host "`nDécoupage terminé avec succès ! Les fichiers sont dans : $Destination" -ForegroundColor Green
