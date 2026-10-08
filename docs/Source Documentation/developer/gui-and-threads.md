# Interface et frontières de threads

@page gui_and_threads Interface, visualisations et threads

## Composition de l’interface

`HelmEditor` (plug-in) et `HelmBoyEditor` (standalone) fournissent le contexte
JUCE et implémentent `SynthGuiInterface`. `FullInterface` assemble les sections
de synthèse, le navigateur de patches, les contrôles globaux, les visualisations
et le gestionnaire de modulation. `SynthesisInterface` compose les sections
fonctionnelles; les composants réutilisables (slider, clavier, enveloppe,
oscilloscope, compteurs) vivent dans `src/editor_components`.

Les contrôles d’interface correspondent aux noms du registre moteur. Le
gestionnaire de modulation crée les commandes et compteurs de modulation à
partir des sources et destinations exposées par `HelmBoyModule`. Le rendu
OpenGL est optionnel au niveau du cycle de vie graphique : l’interface peut
dessiner son fond logiciel avant l’attachement du contexte, et chaque renderer
doit vérifier que ses ressources de shader sont valides.

## Transport des changements vers l’interface

`SynthBase` reçoit plusieurs catégories de données avec des durées de vie et
des rythmes différents :

- Les changements de paramètres issus du traitement audio sont publiés dans
  une table de contrôles préallouée contenant valeur atomique et indicateur
  `dirty`. Un timer JUCE sur le thread message délivre les notifications GUI
  toutes les 30 ms environ; les composants ne sont pas mutés depuis la boucle
  audio.
- `captureGuiStateSnapshot` copie valeurs et routes de modulation sous la
  section critique. Le code GUI applique ensuite cette copie aux widgets et au
  gestionnaire de modulation.
- Les tables publiées restent vivantes tant que le synthétiseur existe; lors
  d’un rebuild, le pointeur atomique bascule vers une nouvelle table au lieu de
  libérer une table qu’un lecteur audio pourrait encore observer.

Ces mécanismes ne rendent pas chaque méthode de l’interface thread-safe. Les
fonctions d’état, le graphe de modulation et les callbacks d’hôte ont leurs
propres règles de verrouillage; consulter les implémentations avant de déplacer
un appel entre threads.

## Télémétrie et visualisations

Après le calcul audio, `SynthBase` publie une télémétrie sélectionnée et des
niveaux de crête atomiques. Les tables de télémétrie utilisent un indicateur
`atomic_flag` non bloquant; si un lecteur occupe la table, la publication du
bloc courant est sautée. Les lecteurs doivent garder leur dernier snapshot
complet plutôt que d’attendre dans un chemin temps réel.

L’oscilloscope utilise `copyOutputMemory` pour copier une fenêtre cohérente du
signal. La méthode échoue sans modifier la destination quand le snapshot est
occupé ou trop petit. Les visualiseurs d’enveloppe, d’onde et les compteurs de
modulation s’appuient sur les sorties sélectionnées et leur télémétrie; ils ne
doivent pas lire directement un buffer DSP mutable depuis le thread de rendu.

Les géométries, images de fond et ressources OpenGL ont également leur propre
publication/cycle de vie. Les créations et destructions GL restent attachées
aux callbacks du contexte OpenGL; toute nouvelle visualisation doit traiter
l’absence de peer, de layout prêt ou de shader valide sans déréférencer les
ressources.

## Saisie des quantites de modulation

`SynthSlider::showModulationPopup` ajoute aux commandes existantes une entree
`Enter Value for ...` par liaison active, depuis la source, la destination ou
son controle de quantite. Le callback conserve les noms source/destination,
pas des pointeurs de connexions ni des indices a recalculer pour ces entrees.

`FullInterface` delegue la saisie a `OpenGLModulationManager`, qui utilise la
plage, les unites et la conversion du controle de modulation concerne.
La boite reutilise la validation de `Enter Value...` avec un controle temporaire
dont la duree de vie couvre le dialogue asynchrone. Des `SafePointer` protegent
les composants GUI; la liaison est reverifiee avant validation pour ne pas
recreer une route supprimee pendant la saisie.

La confirmation appelle `setModulationAmount` sur le thread message, conserve
la source selectionnee, rafraichit les indicateurs et marque le patch modifie.
Le moteur recoit la quantite par la file de changements de modulation existante.
Une quantite nulle supprime la liaison et ses entrees dans les menus suivants.

## Verrous propres aux hôtes

Le plug-in retourne le verrou de callback JUCE à `SynthBase`; l’application
autonome utilise le `CriticalSection` de `HelmBoyEditor`. Le traitement audio
standalone tient ce verrou pendant la préparation des changements, la lecture
MIDI et le rendu du bloc. Une modification d’état ne doit donc pas supposer que
les règles de verrouillage sont identiques entre ces deux adaptateurs.
