# AudioTone-Studio
Générateur de tonalités audio multicanal pour Windows.

## Fonctionnalités prévues

- Fréquences réglables de 1 Hz à 20 000 Hz.
- Réglage indépendant de chaque canal.
- Contrôle individuel du volume.
- Activation et désactivation des canaux.
- Lecture simultanée et arrêt.
- Synchronisation des fréquences.
- Sélection des périphériques audio disponibles.
- Compilation Windows automatique avec GitHub Actions.

## Compilation

Le projet utilise Qt 6, CMake et GitHub Actions.

Ouvrez l'onglet Actions du dépôt GitHub pour lancer la compilation et récupérer l'artefact Windows.

## Audio multicanal

La sortie 5.1 dépend de la prise en charge du format par le périphérique audio et son pilote. Une sortie virtuelle peut nécessiter un pilote audio virtuel compatible.
