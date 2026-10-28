# Changelog

## English

Notable changes to HelmBoy are listed here, based on the Git history. Entries
are grouped by version and feature.

## [1.1.1] - 2026-10-28

### Added

- Load a preset by double-clicking it in the patch browser; the selection
  window closes after loading.
- New presets: Large Pad and Agressive Organ.

### Changed

- Corrected patch names.
- Optimized linking and code generation on MSVC with `/LTCG`.

## [1.1.0] - 2026-10-20

### Added

- Modulateable hard sync between oscillators and independent feedback paths
  for each oscillator.
- AHDSR and DAHDSR envelope stages.
- Notch and Comb filters, separate aftertouch controls, and LFOs and a
  sequencer extended to 128 steps.
- Stereo chorus, stereo effect parameters, stereo width and Freeze mode for
  reverb, and a stereo limiter.
- Direct entry of modulation values and improved modulation displays,
  including oscillator and LFO phase stretch.
- Audio device persistence and MIDI profile save/load in the standalone
  application.
- Many factory presets, as well as imported and converted presets.
- Linux DEB and RPM packages, with presets distributed separately. Linux and
  macOS build workflows were also added; the macOS workflow had not yet been
  validated according to the history.

### Changed

- Biquad and SVF filters migrated to JUCE DSP paths; JUCE reverb integrated
  with support for stereo parameters.
- Optimized standalone startup and headless mode.
- Modernized JUCE APIs and adapted build configurations for Linux, LV2, AU,
  and AUv3.

### Fixed

- Freezes when switching presets and issues loading legacy presets.
- Standalone control synchronization and some modulation targets.
- Linux/LV2 builds with GCC and LTO, along with several compilation and
  integration issues.

## Français

Les changements notables de HelmBoy sont répertoriés ici, d’après l’historique
Git. Les entrées regroupent les commits par version et par fonctionnalité.

## [1.1.1] - 2026-10-28

### Ajouté

- Chargement d’un preset par double-clic dans le navigateur de patches, avec
  fermeture de la fenêtre de sélection.
- Nouveaux presets : Large Pad et Agressive Organ.

### Modifié

- Correction de noms de patches.
- Optimisation de l’édition de liens et de la génération de code sous MSVC avec
  `/LTCG`.

## [1.1.0] - 2026-10-20

### Ajouté

- Synchronisation dure modulable entre oscillateurs et chemins de feedback
  indépendants pour chaque oscillateur.
- Phases AHDSR et DAHDSR pour les enveloppes.
- Filtres Notch et Comb, aftertouch différenciés, et LFO et séquenceur étendus
  jusqu’à 128 pas.
- Chorus stéréo, paramètres d’effets stéréo, largeur stéréo et mode Freeze pour
  la réverbération, ainsi qu’un limiteur stéréo.
- Saisie directe des valeurs de modulation et améliorations de leur affichage,
  notamment pour le phase stretch des oscillateurs et des LFO.
- Mémorisation des périphériques audio et sauvegarde/chargement des profils
  MIDI dans l’application autonome.
- Nombreux presets d’usine et presets importés ou convertis.
- Paquets Linux DEB et RPM, avec les presets distribués séparément. Des workflows
  de génération pour Linux et macOS ont également été ajoutés ; le workflow
  macOS n’était pas encore validé dans l’historique.

### Modifié

- Filtres biquad et SVF migrés vers les chemins DSP JUCE ; réverbération JUCE
  intégrée avec prise en charge des paramètres stéréo.
- Démarrage autonome et mode sans interface optimisés.
- Modernisation des API JUCE et adaptation des configurations de build pour
  Linux, LV2, AU et AUv3.

### Corrigé

- Blocages lors du changement de preset et problèmes de chargement des anciens
  presets.
- Synchronisation des contrôles autonomes et de certaines cibles de modulation.
- Compatibilité des builds Linux/LV2 avec GCC et LTO, ainsi que plusieurs
  problèmes de compilation et d’intégration.