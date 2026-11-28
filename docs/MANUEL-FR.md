# OMNISCIENCE — Manuel utilisateur
**Liberty Music Software · v0.7.9 · DARK COSMOS**

## Installation
macOS Apple Silicon : décompressez l'archive, ouvrez le paquet PKG, suivez l'assistant puis relancez votre DAW. Les plug-ins AU et VST3 sont installés dans les dossiers système Audio/Plug-Ins. Windows x64 : ouvrez l'EXE, suivez l'assistant puis relancez votre DAW et rescanez les VST3 si nécessaire. L'EXE peut afficher un avertissement SmartScreen s'il n'est pas signé Authenticode. Les formats distribués sont AU/VST3 ARM64 et VST3 x64 ; aucun format Intel macOS ou AAX n'est annoncé. L'installation ne requiert pas de modification manuelle des protections système.

## Première utilisation
Créez une piste instrument, chargez Omniscience et choisissez un preset. Connectez un clavier MIDI ou utilisez le piano roll du DAW. Commencez à niveau d'écoute modéré. Le navigateur propose 370 presets d'usine. Sauvegardez votre projet avant les changements importants.

## Architecture et synthèse
Quatre couches, cinq oscillateurs par couche, jusqu'à 32 voix. Sélectionnez une couche pour régler ses oscillateurs, enveloppes, filtres et niveaux. Le moteur combine notamment synthèse par formes d'onde, wavetable et granulaire. Les pages de modulation et la matrice permettent d'affecter des sources aux destinations, en dosant leur profondeur. Les macros regroupent les commandes de performance. Vérifiez le niveau du master quand plusieurs couches jouent ensemble.

## NEXUS
La zone XY et le morphing permettent de faire évoluer les timbres. Les trajectoires, snapshots, mouvements et mutations fournissent des variations et des rappels. Enregistrez un snapshot avant de modifier une configuration que vous souhaitez retrouver.

## Rack FX
Ouvrez NEXUS puis FX. Choisissez l'un des quatre racks de couche ou le master. Chaque rack comprend huit emplacements, soit 40 au total. Sélectionnez un effet, activez-le, réglez le mélange Dry/Wet, l'entrée, la sortie et les paramètres A/B adaptés à son moteur. Les racks peuvent fonctionner en série ou en parallèle. Les commandes Monter/Descendre modifient l'ordre des effets. Les favoris et la recherche facilitent l'accès au catalogue.
Les 100 effets couvrent utilitaires, dynamique, analogique, modulation, délais, réverbérations et traitements spectraux/créatifs. Les trois profils Doux, Équilibré et Intense donnent 300 réglages individuels. Les gains de 47 profils ont été réduits sur un signal de référence ; la crête et la sonie dépendent toujours du signal réellement traité.

## Qualité, latence et performances
Les effets analogiques 19–28 proposent les facteurs 1x, 2x, 4x, 8x et 16x. Augmenter le facteur réduit généralement l'aliasing au prix de calculs supplémentaires. Le FIR ajoute 24 échantillons pour les facteurs supérieurs à 1x. Les effets spectraux 79–89 proposent des FFT de 512, 1024, 2048 et 4096 points, soit environ 10,7, 21,3, 42,7 et 85,3 ms à 48 kHz par emplacement. Les latences s'additionnent en série ; les chemins parallèles et les couches sont alignés. Le plugin informe l'hôte de la latence calculée. Évitez de changer de résolution pendant une prise critique : une adaptation en cours de lecture dépend du DAW.
Choisissez des réglages modestes pendant l'enregistrement et une qualité supérieure au mixage si le budget CPU le permet. Les délais créatifs et granulaires ne constituent pas tous une latence de compensation. Le Time Stretch est un traitement granulaire temps réel borné, pas un outil d'édition hors ligne. La correction d'enveloppe des formants est approximative.

## MIDI Learn, automation et sauvegarde
Choisissez le paramètre d'effet, activez MIDI Learn puis déplacez le contrôleur MIDI. Effacer CC supprime l'affectation. Les paramètres du plug-in sont accessibles à l'automation du DAW selon ses capacités. Enregistrez le projet et rechargez-le pour vérifier l'état. La version préserve les identifiants d'effets et les gains enregistrés ; la calibration s'applique à la sélection explicite d'un profil.

## Mesures et dépannage
Le spectre 24 bandes des effets spectraux, la réduction de gain, les crêtes d'échantillons et la corrélation stéréo aident au contrôle. Les tracés de réglage ne sont pas tous des mesures de réponse acoustique. En cas de surcharge, augmentez le buffer du DAW, réduisez la polyphonie, la résolution FFT ou le suréchantillonnage ; désactivez les effets inutiles. En cas de silence, vérifiez piste MIDI, canal, couches actives et master.
Pour signaler un problème : version, système, DAW, fréquence, taille de buffer et étapes reproductibles. https://libertymusicsoftware.com/

## Statut
Ce document accompagne la qualification précommerciale. Les builds, les mesures de référence et les installateurs ne remplacent pas l'écoute et les essais dans les DAW réels. Voir QC-v0.7.9.md.
