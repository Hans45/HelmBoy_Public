# Paramètres et état

@page state_and_parameters Paramètres et état

## Définition et exposition des paramètres

Les définitions centrales sont dans `mopo::Parameters`. Chaque `ValueDetails`
associe un nom de contrôle à sa plage et à sa valeur par défaut. Les modules
créent les valeurs DSP avec `HelmBoyModule`; `SynthBase::controls_` expose la
table commune aux widgets, à la sauvegarde et au pont de paramètres du plug-in.

`HelmPlugin` crée un `ValueBridge` pour chaque contrôle et l’enregistre auprès
de JUCE. Le pont convertit entre l’échelle normalisée de l’hôte et l’échelle
interne décrite par les métadonnées du paramètre. Il notifie l’hôte des gestes
et changements issus de l’interface. Un changement d’identifiant ou de
sémantique de paramètre affecte les projets hôtes et les presets; conserver les
identifiants publiés et vérifier les migrations avant de modifier ces contrats.

## Changements de valeur

- Un changement GUI utilise `SynthBase::valueChanged` et entre dans la file
  concurrente de contrôles; `processControlChanges` applique les valeurs au
  graphe pendant le traitement audio.
- Une modification de l’hôte passe par `HelmPlugin::parameterChanged`, puis
  `valueChangedExternal`; la valeur est mise en file pour le moteur et une
  notification GUI est préparée.
- Une valeur de modulation est un état de connexion distinct. Le changement
  est mis en file, puis `processModulationChanges` met à jour l’intensité et
  connecte/déconnecte la route dans `HelmBoyEngine`.
- Les gestes GUI appellent `beginChangeGesture`/`endChangeGesture`; ce sont des
  notifications d’automation, pas le calcul DSP lui-même.

Les files évitent de faire muter directement le graphe à partir de tous les
contextes appelants, mais cela ne signifie pas que toute l’interface de
`SynthBase` est lock-free. La sérialisation et les mutations d’état utilisent
également les sections critiques fournies par chaque hôte.

## Format des patches

Un patch est un objet JSON versionné. Les métadonnées de premier niveau
comprennent notamment `synth_version`, `patch_name`, `folder_name`, `author` et
`license`. L’objet `settings` contient les valeurs nommées des contrôles et la
liste `modulations`, chaque route décrivant sa source, sa destination et son
intensité. Les champs absents ne signifient pas qu’un contrôle est invalide :
le chargeur fournit sa valeur par défaut déclarée.

Les paramètres du limiteur sont `limiter_ceiling` (défaut `0`, plage `-12..0`
dB) et `limiter_release` (défaut `100`, plage `5..500` ms); `limiter_on` reste
son interrupteur indépendant. Les 1?220 patches suivis contiennent ces deux
clés à leurs valeurs par défaut, placées alphabétiquement autour de
`limiter_on`. Leur ajout ne modifie pas les autres valeurs ni les modulations.

Le paramètre `pan` va de `-1` (gauche) à `+1` (droite), avec `0` au centre.
Son défaut `0` figure dans les 1?220 patches, entre `osc_mix` et
`pitch_bend_range` dans `settings`.

Le chargement suit deux étapes :

1. `LoadSave::prepareState` valide et transforme une copie de l’objet avant la
section critique. Il traite les anciennes enveloppes de format, les aliases de
contrôles, les migrations de routes, les nombres non finis, les contrôles
inconnus et les valeurs hors plage.
2. `LoadSave::applyPreparedState` applique les valeurs reconnues, reconstruit
les connexions de modulation et restaure les métadonnées. Cette fonction ne
prend pas elle-même le verrou; son appelant protège les mutations. `SynthBase`
prépare d’abord l’état, puis applique le résultat dans sa section critique.

Des exemples de migrations versionnées dans l’implémentation convertissent les
anciens patches (par exemple le format antérieur à 0.4.1, le mélangeur ajouté
après 0.5.0 et les changements de niveau/unisson jusqu’à 0.8.6). Ajouter une
migration seulement pour une rupture réelle de schéma et conserver une valeur
par défaut sûre pour les nouveaux contrôles.

## Presets et configuration

`LoadSave` gère séparément l’état d’un patch et la configuration locale : racine
de patches personnalisée, mappings MIDI, périphériques, taille de fenêtre et
préférences. Le navigateur de patches est une vue de ces fichiers et banques;
les programmes hôte du plug-in ont leur propre liste chargée depuis les patches
disponibles. Ne pas confondre un numéro de programme d’hôte avec un index global
permanent du format JSON.

Les points d’entrée à consulter avant de modifier ce flux sont
`SynthBase::loadFromVar`, `HelmPlugin::getStateInformation`,
`HelmPlugin::setStateInformation`, `LoadSave::stateToVar`,
`LoadSave::prepareState` et `LoadSave::applyPreparedState`.
