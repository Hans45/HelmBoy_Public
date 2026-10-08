# Architecture du logiciel

@page architecture Architecture du logiciel

HelmBoy partage son moteur sonore et ses contrôles entre deux intégrations JUCE : le plug-in audio et l’application autonome. Le code spécifique à l’hôte fournit les événements, prépare les blocs audio et expose les paramètres; le moteur `mopo::HelmBoyEngine` calcule les voix et la chaîne d’effets.

## Couches du dépôt

| Répertoire | Responsabilité |
| --- | --- |
| `src/plugin` | Adaptateur `AudioProcessor`, paramètres d’automation, programmes, état d’hôte et création de l’éditeur. |
| `src/standalone` | Fenêtre, gestionnaire de périphériques audio/MIDI et adaptation `AudioSource`. |
| `src/common` | `SynthBase`, façade commune, contrôles, MIDI, sauvegarde, configuration et transport des mises à jour GUI. |
| `src/synthesis` | `HelmBoyEngine`, routage des paramètres, modulation, oscillateurs et construction des voix. |
| `src/editor_sections` | Sections fonctionnelles de l’interface, navigateur de patches et orchestration de l’éditeur. |
| `src/editor_components` | Contrôles, claviers, visualisations et composants OpenGL. |
| `src/look_and_feel` | Palette, typographie, shaders et rendu JUCE/OpenGL. |
| `mopo/src` | Primitives DSP, graphe de processeurs, routeurs, paramètres et filtres/effets. |

`HelmPlugin` et `HelmBoyEditor` dérivent tous deux de `SynthBase`. Cela partage l’API du synthétiseur, pas leurs règles de synchronisation : le plug-in utilise le verrou de callback JUCE, tandis que le standalone possède son propre `CriticalSection` et son propre cycle de périphériques.

## Cycle audio

### Plug-in

`HelmPlugin::prepareToPlay` transmet la fréquence d’échantillonnage et la taille de bloc au moteur et au gestionnaire MIDI. `processBlock` lit les informations de transport disponibles, applique les changements de contrôles et de modulations en attente, puis traite les événements MIDI à leurs offsets. Les blocs dépassant `MAX_BUFFER_PROCESS` sont découpés en segments d’au plus 256 échantillons. Chaque segment appelle `SynthBase::processAudioSafe`.

### Application autonome

`HelmBoyEditor::prepareToPlay` prépare le moteur après l’ouverture du périphérique. `getNextAudioBlock` protège la section de traitement par le verrou de l’éditeur, vérifie l’état de démarrage et la taille du buffer, récupère les événements du périphérique et du clavier, puis traite le buffer par segments d’au plus 256 échantillons. Si l’audio n’est pas prêt, la région active est effacée.

### Moteur et signal

`HelmBoyEngine` dérive de `HelmBoyModule`, lui-même fondé sur `mopo::ProcessorRouter`. Le routeur ordonne les processeurs d’après leurs dépendances; les processeurs de feedback représentent les cycles explicites. Les modules enregistrent leurs contrôles et sources/destinations de modulation. Le `HelmBoyVoiceHandler` contient les éléments polyphoniques et duplique les processeurs nécessaires selon le nombre de voix; les sources monophoniques (LFO, séquenceur, arpégiateur) sont construites au niveau du moteur.

La chaîne de sortie raccordée dans `HelmBoyEngine::init` est :

1. sommation des voix;
2. distorsion;
3. chorus stéréo;
4. délai stéréo et bypass;
5. un filtre DC par canal;
6. réverbération stéréo et bypass;
7. volume lissé;
8. balance stéréo Pan;
9. mesure de crête;
10. limiteur stéréo lié;
11. clamp final et sorties gauche/droite.

Le limiteur est après le mesureur de crête : l’indication de crête exposée par
ce point du graphe n’est donc pas une mesure post-limiteur. Il calcule un pic
commun gauche/droite sur le bloc, réduit immédiatement le gain si ce pic dépasse
le plafond demandé, puis lisse le retour vers le gain unité selon le temps de
relâchement. Le limiteur n’ajoute pas de buffer de lookahead.

Les chemins Reverb JUCE et legacy reçoivent tous deux les entrées gauche et
droite séparément. Le chemin legacy alimente ses réseaux de combs/all-pass
gauche et droite avec leur canal respectif; l’entrée droite non connectée
retombe sur la gauche pour conserver la compatibilité mono.

Dans l’interface, le panneau Limiter est placé à droite de l’oscilloscope, à sa
même hauteur et avec la largeur du panneau Distortion. Son activator est dans
la barre de titre; Ceiling et Release sont les deux contrôles du panneau.

`SynthBase::processAudioSafe` appelle le graphe, copie les sorties du moteur dans les canaux audio en alternant gauche/droite, vérifie les échantillons finis avec les assertions actives, puis publie les données destinées aux visualisations. Ce chemin produit un signal de synthèse; il ne mélange pas un flux audio d’entrée de l’hôte.

Le contrôle `pan` agit après le volume et avant la mesure de crête. Au centre,
les deux canaux conservent leur gain; vers la gauche ou la droite, seul le canal
opposé est atténué. Le traitement ne somme pas les canaux et préserve leur
contenu stéréo.

## Paramètres et modulation

Les contrôles sont créés par les modules à partir des définitions de `mopo::Parameters`. Les tables des modules servent à exposer les contrôles à `SynthBase` et à l’interface. Une connexion de modulation associe une source, une destination et une valeur d’intensité; `HelmBoyEngine` raccorde la source à l’entrée du processeur de destination et active les sélecteurs mono/poly appropriés.

Dans le plug-in, `ValueBridge` expose les contrôles JUCE à l’hôte et convertit les valeurs entre l’échelle du paramètre interne et celle du paramètre plugin. Les paramètres existants doivent conserver leurs identifiants et leur ordre d’enregistrement; l’ajout de paramètres est effectué en fin de liste afin de préserver les indices d’automation déjà publiés.

Le traitement exact des changements, de l’état sérialisé et des migrations est détaillé dans @ref state_and_parameters. Les chemins MIDI sont décrits dans @ref midi_input.

## Initialisation et portée des garanties

Le graphe global d’effets est construit par `HelmBoyEngine::init`. Le plug-in initialise aussi les modules pendant la construction de son moteur; le standalone peut différer l’initialisation de certains modules jusqu’au préflight de démarrage. Cette différence concerne la séquence de lancement, pas l’ordre de la chaîne sonore.

Cette page décrit l’architecture lue dans le code. Elle ne certifie ni la compatibilité de chaque hôte/formats, ni la stabilité en temps réel, ni le comportement audible pour toutes les configurations. Les tests disponibles et leurs options sont recensés dans @ref development_and_tests.
