<#
.SYNOPSIS
    Prépare les paquets à copier sur les PC de l'événement : client, serveur et éditeur de cartes.

.DESCRIPTION
    Compile la solution (Release x64), puis crée dans dist\ trois dossiers et leurs archives zip.
    Le zip du client est aussi déposé dans la page web du serveur (assets\web\telecharger\) : les
    joueurs le téléchargent depuis http://<serveur>:8080/telecharger.html.
      TacticalWar-client   jeu (joueurs, spectateur, administration)
      TacticalWar-serveur  serveur de jeu + vue projetée web + bot de test
      TacticalWar-editeur  éditeur de cartes
    Le runtime Visual C++ est copié à côté des programmes (déploiement local) : rien à installer
    sur les PC. Les données du serveur (équipes, tournois, mots de passe) ne sont jamais incluses.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\package.ps1 -ServerHost 192.168.1.10
#>
param(
    # Adresse du serveur écrite dans le client.json du paquet client (modifiable ensuite).
    [string]$ServerHost = "127.0.0.1",
    [switch]$NoBuild,
    [string]$OutDir = "dist"
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$release = Join-Path $root "x64\Release"
$dist = if ([System.IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $root $OutDir }

function Find-VisualStudio {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "Visual Studio introuvable (vswhere.exe absent)." }
    $path = & $vswhere -latest -requires Microsoft.Component.MSBuild -property installationPath
    if (-not $path) { throw "Aucune installation de Visual Studio avec MSBuild." }
    return $path
}

function Copy-Files([string]$from, [string[]]$names, [string]$to) {
    New-Item -ItemType Directory -Force $to | Out-Null
    foreach ($name in $names) {
        $source = Join-Path $from $name
        if (-not (Test-Path $source)) { throw "Fichier manquant : $source" }
        Copy-Item $source $to -Force
    }
}

function Copy-Tree([string]$from, [string]$to, [string[]]$exclude = @()) {
    New-Item -ItemType Directory -Force $to | Out-Null
    # robocopy : code de retour < 8 = succès.
    $arguments = @($from, $to, "/E", "/NFL", "/NDL", "/NJH", "/NJS", "/NP")
    if ($exclude.Count -gt 0) { $arguments += "/XF"; $arguments += $exclude }
    & robocopy @arguments | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "Copie impossible : $from" }
}

function Write-Utf8([string]$path, [string]$text) {
    [System.IO.File]::WriteAllText($path, $text.Replace("`r`n", "`n").Replace("`n", "`r`n"), (New-Object System.Text.UTF8Encoding($false)))
}

$vs = Find-VisualStudio

if (-not $NoBuild) {
    $msbuild = Join-Path $vs "MSBuild\Current\Bin\MSBuild.exe"
    Write-Host "Compilation (Release x64)..."
    & $msbuild (Join-Path $root "TacticalWar.sln") -p:Configuration=Release -p:Platform=x64 -m -v:minimal -nologo
    if ($LASTEXITCODE -ne 0) { throw "La compilation a échoué." }

    Write-Host "Tests..."
    Push-Location $release
    & .\TacticalWarTests.exe
    $testsFailed = $LASTEXITCODE -ne 0
    Pop-Location
    if ($testsFailed) { throw "Des tests unitaires échouent." }
}

# Runtime Visual C++ (version la plus récente disponible).
$redist = Get-ChildItem (Join-Path $vs "VC\Redist\MSVC") -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName "x64\Microsoft.VC143.CRT\msvcp140.dll") } |
    Sort-Object { [version]($_.Name -replace '[^0-9.]', '') } | Select-Object -Last 1
if (-not $redist) { throw "Runtime Visual C++ (Microsoft.VC143.CRT) introuvable." }
$crtDir = Join-Path $redist.FullName "x64\Microsoft.VC143.CRT"
$crt = @("msvcp140.dll", "msvcp140_1.dll", "vcruntime140.dll", "vcruntime140_1.dll")

$sfml = @("sfml-graphics-2.dll", "sfml-window-2.dll", "sfml-system-2.dll")
$assets = Join-Path $root "assets"

if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
New-Item -ItemType Directory -Force $dist | Out-Null

# --- Client -------------------------------------------------------------------
$client = Join-Path $dist "TacticalWar-client"
Copy-Files $release (@("TacticalWar.exe", "tgui.dll", "openal32.dll", "sfml-audio-2.dll", "sfml-network-2.dll") + $sfml) $client
Copy-Files $crtDir $crt $client
Copy-Tree $assets (Join-Path $client "assets") @("equipe.txt", "scoreEnd.txt")
Remove-Item -Recurse -Force (Join-Path $client "assets\web")
# Les peintures d'origine (plusieurs Mo) ne servent qu'au générateur de tuiles.
Get-ChildItem (Join-Path $client "assets\tiles") -File -Filter *.png | Remove-Item -Force
Write-Utf8 (Join-Path $client "client.json") (@{ serverHost = $ServerHost; serverPort = 12345; sound = $true } | ConvertTo-Json)
Write-Utf8 (Join-Path $client "Spectateur-realisateur.bat") "@echo off`ncd /d %~dp0`nstart """" TacticalWar.exe --director`n"
Write-Utf8 (Join-Path $client "Demonstration.bat") "@echo off`ncd /d %~dp0`nstart """" TacticalWar.exe --training-autoplay`n"
Write-Utf8 (Join-Path $client "LISEZMOI.txt") @"
Tactical War - client

Lancer TacticalWar.exe, puis se connecter avec les identifiants de la fiche de l'équipe.
L'adresse du serveur est demandée sur l'écran de connexion (ou dans client.json).

Entraînement sans serveur : bouton "Entraînement" de l'écran de connexion (combat contre
l'ordinateur). Demonstration.bat enchaîne des combats entre ordinateurs (écran d'accueil).

Écran projeté : Spectateur-realisateur.bat suit automatiquement le combat le plus serré
(caméra sur le personnage actif). Pour choisir soi-même le combat à regarder : laisser
l'identifiant et le mot de passe vides sur l'écran de connexion.

Pas de son sur ce PC : ajouter --no-sound à la ligne de commande.
"@

function Compress-Package([string]$package) {
    $zip = "$package.zip"
    Compress-Archive -Path (Join-Path $package "*") -DestinationPath $zip -Force
    $size = [math]::Round((Get-Item $zip).Length / 1MB, 1)
    Write-Host ("{0} ({1} Mo)" -f (Split-Path $zip -Leaf), $size)
    return $zip
}

# Zip du client d'abord : le serveur le propose au téléchargement (clients d'une autre version).
$clientZip = Compress-Package $client

# --- Serveur ------------------------------------------------------------------
$server = Join-Path $dist "TacticalWar-serveur"
Copy-Files $release @("TacticalWarServer.exe", "TacticalWarBot.exe") $server
Copy-Files $crtDir $crt $server
foreach ($folder in @("data", "map", "web")) {
    Copy-Tree (Join-Path $assets $folder) (Join-Path $server "assets\$folder")
}
New-Item -ItemType Directory -Force (Join-Path $server "assets\web\telecharger") | Out-Null
Copy-Item $clientZip (Join-Path $server "assets\web\telecharger\TacticalWar-client.zip") -Force
New-Item -ItemType Directory -Force (Join-Path $server "assets\tiles") | Out-Null
Copy-Item (Join-Path $assets "tiles\tileset.json") (Join-Path $server "assets\tiles") -Force
Write-Utf8 (Join-Path $server "Ouvrir-pare-feu.bat") @"
@echo off
rem À lancer une fois en tant qu'administrateur : autorise le jeu (12345) et la vue projetée (8080).
netsh advfirewall firewall add rule name="Tactical War - jeu" dir=in action=allow protocol=TCP localport=12345
netsh advfirewall firewall add rule name="Tactical War - vue projetee" dir=in action=allow protocol=TCP localport=8080
pause
"@
Write-Utf8 (Join-Path $server "LISEZMOI.txt") @"
Tactical War - serveur

1. Lancer TacticalWarServer.exe. Au premier lancement, le mot de passe administrateur est généré
   et affiché : le noter (TacticalWarServer.exe --set-admin-password pour le changer).
2. Le serveur affiche ses adresses sur le réseau local : les donner aux joueurs.
3. Vue projetée : ouvrir http://<adresse>:8080/ dans un navigateur (?rotate=20 pour faire
   défiler les vues). Le jeu se télécharge sur http://<adresse>:8080/telecharger.html (un client
   d'une autre version y est renvoyé à la connexion).
4. Pare-feu : lancer Ouvrir-pare-feu.bat en tant qu'administrateur.

Les équipes, tournois et résultats sont enregistrés dans le dossier data\ (à sauvegarder).
Arrêt : Ctrl+C (tout est enregistré). Après un arrêt brutal, le tournoi reprend au relancement.

Test sans joueurs : TacticalWarBot.exe --login <login> --password <mot de passe> joue des actions
au hasard (un bot par joueur).

Voir aussi docs\checklist-evenement.md dans le dépôt.
"@

# --- Éditeur de cartes ----------------------------------------------------------
$editor = Join-Path $dist "TacticalWar-editeur"
Copy-Files $release (@("EnvironmentEditor.exe") + $sfml) $editor
Copy-Files $crtDir $crt $editor
foreach ($folder in @("map", "tiles", "shaders", "font")) {
    Copy-Tree (Join-Path $assets $folder) (Join-Path $editor "assets\$folder")
}
# Les peintures d'origine (plusieurs Mo) ne servent qu'au générateur de tuiles.
Get-ChildItem (Join-Path $editor "assets\tiles") -File -Filter *.png | Remove-Item -Force
Write-Utf8 (Join-Path $editor "LISEZMOI.txt") @"
Tactical War - éditeur de cartes

Les cartes sont enregistrées dans assets\map\<numéro>.json : les copier dans le dossier
assets\map du serveur (il les envoie aux joueurs au début de chaque combat).
Une carte doit avoir au moins 2 cases de départ praticables par équipe, reliées entre elles :
le bouton « Valider la carte » le vérifie.
"@

# --- Archives -------------------------------------------------------------------
foreach ($package in @($server, $editor)) {
    Compress-Package $package | Out-Null
}
Write-Host "Paquets prêts dans $dist"
