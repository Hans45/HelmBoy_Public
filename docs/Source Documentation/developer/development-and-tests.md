# Développement et tests ciblés

@page development_and_tests Développement et tests ciblés

## Cibles principales

Le projet CMake configure les cibles JUCE `HelmBoyPlugin` et
`HelmBoyStandalone`. Les formats du plug-in sont contrôlés par `BUILD_VST3`,
`BUILD_AU`, `BUILD_LV2` et `BUILD_AAX`; VST3, AU et LV2 sont activés par défaut,
AAX ne l’est pas. Les formats effectivement produits dépendent des SDK et de la
plateforme. Le standalone dépend des pilotes activés et de la disponibilité
optionnelle du SDK ASIO sur Windows.

## Tests optionnels

Les deux options de test sont désactivées par défaut :

- `HELMBOY_BUILD_MIDI_TESTS` ajoute `HelmBoyMidiTests` et le test CTest
  `Midi2ManagerTests`.
- `HELMBOY_BUILD_EFFECT_TESTS` ajoute `HelmBoyEffectTests` et le test CTest
  `StereoEffectsTests`.

Exemple PowerShell à lancer localement depuis la racine, si un build Debug
existe ou doit être configuré :

```powershell
cmake -S . -B build -DHELMBOY_BUILD_MIDI_TESTS=ON -DHELMBOY_BUILD_EFFECT_TESTS=ON
cmake --build build --config Debug --target HelmBoyMidiTests HelmBoyEffectTests
ctest --test-dir build -C Debug --output-on-failure
```

Les tests MIDI vérifient le décodeur UMP et certains callbacks isolés. Les tests
d’effets couvrent aussi le ping-pong du délai stéréo, le plafond du limiteur,
son relâchement, son bypass, le lien stéréo et les positions centre/gauche/droite
du Pan. Ils ne remplacent pas un essai audio, une validation
de preset, ni un test dans chaque hôte/formats.

Les options `HELMBOY_ENABLE_BIQUAD_JUCE_PATH`,
`HELMBOY_ENABLE_SVF_JUCE_PATH` et `HELMBOY_ENABLE_REVERB_JUCE_PATH` sont
activées par défaut dans `CMakeLists.txt`. Les options `HELMBOY_DEBUG_*` et
les cibles de tests ont des valeurs distinctes; vérifier le cache CMake utilisé
avant de conclure quel chemin a été compilé.

## Documentation API

La référence Doxygen est configurée dans `Doxyfile`; les pages Markdown du
répertoire sont aussi ses entrées. La procédure et le chemin de sortie sont
décrits dans [le README Doxygen](../README.md). La sortie HTML n’est pas une
source : ne pas modifier ni régénérer ses fichiers dans une tâche documentaire
sans besoin explicite.

## Discipline de validation

Pour un changement documentaire, vérifier les liens relatifs, les chemins
mentionnés, les identifiants de paramètres et les déclarations d’options contre
le source. Pour un changement DSP ou d’hôte, les vérifications locales
pertinentes sont compilation des cibles touchées, tests CTest ciblés, écoute et
validation dans l’hôte visé. Les commandes de ce guide sont destinées à
l’utilisateur; aucun résultat de compilation ou d’écoute n’est implicite.
