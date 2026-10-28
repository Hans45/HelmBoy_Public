# helmBoy

helmBoy est un synthétiseur polyphonique logiciel dérivé de Helm. Le moteur DSP
modulaire `mopo` est partagé entre l’application autonome JUCE et le plug-in
audio. L’interface réunit les oscillateurs, les filtres, les enveloppes, les
sources de modulation, l’arpégiateur, le séquenceur et les effets.

## Fonctionnement

Les événements MIDI, les automatismes de l’hôte et les contrôles GUI alimentent
`SynthBase` et `mopo::HelmBoyEngine`. Le moteur construit un graphe de processeurs
et un gestionnaire de voix polyphoniques. La sortie passe par la chaîne globale :

`voix -> distorsion -> chorus stéréo -> délai stéréo -> filtres DC gauche/droite -> réverbération stéréo -> volume -> balance Pan -> mesure de crête -> limiteur lié -> sorties`

Le plug-in utilise les informations de tempo/playhead fournies par l’hôte quand
elles sont disponibles. L’application autonome reçoit les événements de ses
périphériques audio et MIDI ainsi que du clavier intégré. Le format de patch
JSON conserve contrôles, modulations et métadonnées; le chargement prépare et
migre l’état avant de l’appliquer au moteur.

Le chemin MIDI normal de l’application est MIDI 1.0 via `MidiManager`. Un
décodeur explicite de paquets UMP MIDI 2.0 est présent pour Note On/Off, CC et
pitch bend, mais l’entrée UMP native des hôtes et périphériques n’est pas
raccordée. Voir la [documentation MIDI développeur](docs/Source%20Documentation/developer/midi.md).

## Documentation

- [Manuel utilisateur](docs/helmBoy%20online%20manual/index.md)
- [Architecture du logiciel](docs/Source%20Documentation/developer/architecture.md)
- [Paramètres et état](docs/Source%20Documentation/developer/state-and-parameters.md)
- [MIDI et UMP](docs/Source%20Documentation/developer/midi.md)
- [Interface et threads](docs/Source%20Documentation/developer/gui-and-threads.md)
- [Développement et tests ciblés](docs/Source%20Documentation/developer/development-and-tests.md)
- [Référence API générée par Doxygen](docs/Source%20Documentation/README.md)

## Effets stéréo

Le chorus est désactivé par défaut, y compris lorsqu’un ancien patch ne possède
pas ses paramètres. Les valeurs par défaut et plages sont définies dans
`src/common/helmBoy_common.cpp`. Il utilise des lignes de délai interpolées JUCE et
des oscillateurs en quadrature; les transitions de mix, feedback, largeur et
bypass sont lissées. Le stockage du délai est préparé hors du traitement audio.

## Limiteur

Le panneau **Limiter** se trouve à droite de l’oscilloscope, avec la même
hauteur que celui-ci et la largeur du panneau Distortion. Son bouton marche/
arrêt est dans la barre de titre; **Ceiling** règle le plafond de ?12 à 0 dBFS
(défaut 0 dBFS) et **Release** le retour du gain de 5 à 500 ms (défaut 100 ms).
La détection et la réduction de gain sont couplées entre les canaux gauche et
droit. Le traitement n’ajoute pas de lookahead ni de latence.

Le contrôle rotatif **Pan** du bloc Volume équilibre les canaux après le volume.
Au centre (`0`), les deux canaux restent inchangés; vers un côté, le canal
opposé est atténué sans downmix mono.

## Tests ciblés

Les tests sont facultatifs et désactivés par défaut. Depuis PowerShell, à la
racine du dépôt, l’utilisateur peut activer les deux cibles ainsi :

```powershell
cmake -S . -B build -DHELMBOY_BUILD_MIDI_TESTS=ON -DHELMBOY_BUILD_EFFECT_TESTS=ON
cmake --build build --config Debug --target HelmBoyMidiTests HelmBoyEffectTests
ctest --test-dir build -C Debug --output-on-failure
```

`Midi2ManagerTests` couvre le décodage et les callbacks du gestionnaire; il ne
teste pas l’entrée UMP d’un hôte. `StereoEffectsTests` couvre le chorus, le
bypass stéréo, le délai ping-pong, la réverbération, le plafond et le relâchement du
limiteur, le balance Pan et les valeurs non finies. Les tests
automatisés ne remplacent pas l’écoute ni la validation dans un hôte.
