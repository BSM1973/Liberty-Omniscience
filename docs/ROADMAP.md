# Omniscience — Dark Cosmos

Reference fonctionnelle : Omnisphere 3. Développement indépendant, sans ressources ni banques propriétaires de Spectrasonics.

## v0.1 — COSMOS FOUNDATION
- 4 couches par patch, 32 voix partagées, 4 formes d'onde, filtre passe-bas, ADSR par couche.
- Volume, panoramique, accordage, activation, automation et état du projet.
- 100 patches de développement générés par paramètres (écoute et différenciation à valider).
- Navigateur français, recherche, interface Dark Cosmos redimensionnable.
- MIDI notes, vélocité, sustain, pitch bend ±2 demi-tons.
- Builds de développement AU/VST3/Standalone macOS ARM64 et VST3/Standalone Windows x64.
- Aucun installateur commercial ni notarisation à ce stade.

## Étapes suivantes
v0.2 Hybrid Synthesis (wavetable, sample, FM/AM, oscillateurs multiples);
v0.3 Deep Granular; v0.4 Filter Perfection; v0.5 Modulation Universe;
v0.6 Nexus & Morph Zone; v0.7 FX Universe; v0.8 Performance Studio (8 parties);
v0.9 Sound Library Pro (objectif 2 000 presets originaux); v0.10 Visual Perfection;
v0.11 Professional Quality Control; v1.0 Commercial Release via Liberty Release Center.

Contraintes : aucune commande fictive, aucun chevauchement, accents français, aucune régression audio.
Les objectifs de 20 oscillateurs, 70 filtres, 100 effets, 64 slots de modulation et 8 parties ne sont pas encore réalisés.

## v0.2 — HYBRID SYNTHESIS (code candidat)
- 5 oscillateurs par couche, 4 couches, 20 oscillateurs par patch. 16 voix d'unisson par oscillateur (mono additionnée), dispersion et dérive.
- Modes VA, Wavetable HD 16 x 2048 avec 8 bandes harmoniques, FM par modulation de phase, AM et sample mono. Hard Sync sur la fondamentale de la couche, modulation en anneau, saturation.
- Import WAV/AIFF, source partagée par couche, mono ou downmix stéréo, 30 secondes maximum et plafond de 64 Mio de sources conservées en session. Note de référence C4; pas de boucles ni de multisamples. Chemins externes sauvegardés, non embarqués; absence de source = silence.
- Page oscillateurs et aperçu de forme d'onde (pas un oscilloscope de sortie). Presets d'usine v0.1 conservés, oscillateurs additionnels désactivés par défaut.
- Tests DSP complémentaires; validation CPU, anti-aliasing des modulations non linéaires, import dans les DAW, migration des sessions et contrôle des interfaces à effectuer avant validation.

## v0.3 — DEEP GRANULAR (candidat, non validé)
- Mode granulaire sur chacun des 20 oscillateurs; 16 grains simultanés par oscillateur et voix, unisson classique désactivé en mode grain.
- Taille, densité, position, dispersion, stretch par vitesse de balayage, freeze, blur (jitter de position et intervalle), reverse, jitter de hauteur, Hann/triangle/trapèze, densité relative au BPM, boucles normalisées et télémétrie de la première voix active.
- 4 sources d'usine synthétiques originales; 100 patches paramétriques granulaires supplémentaires à écouter et différencier; 200 au total. Import WAV/AIFF conservé.
- Limites : pas encore d'édition des boucles par glisser sur la forme d'onde, import wavetable utilisateur, extension 256 formes, SIMD NEON/AVX2 ni benchmarks temps réel. Ces éléments restent à réaliser. Pas de parité garantie avec la référence. Les tests et la compilation de ce candidat restent à lancer après validation v0.2.

## v0.3.1 — DEEP GRANULAR PERFECTION (candidat)
- Édition des marqueurs de boucle à la souris, automation et gestes hôte.
- Table HD 256 × 2048, 8 bandes (1 à 128 harmoniques), expansion de la table historique, mode historique conservé par défaut pour compatibilité. Import WAV de 1 à 256 cycles de 2048, interpolation spectrale et FFT hors thread audio, downmix stéréo, table partagée par couche, huit imports maximum par session. Chemin externe sauvegardé; fichier absent = silence en mode personnel.
- Tests de proximité HD/historique et d'atténuation harmonique, benchmark 48 kHz/128, 1/8/32 notes et 20 oscillateurs; temps moyen/max, taille statique et niveau brut. Pas de validation temps réel garantie, pas de mesure RSS ni de test d'écoute des 200 presets.
- SIMD NEON/SSE2/AVX2 non activé ni implémenté, contrôle DAW et visuel restant. Aucun résultat de compilation ni de benchmark acquis avant exécution CI.

Le correctif Windows v0.3 (pool fixe sur le tas) et son test de saturation sont repris de 125812e1.

## v0.3.2 — COSMOS ENGINE OPTIMIZATION (candidate)
- Hoist oscillator preparation and sample-source resolution outside the sample loop. Optional grouped wavetable unison path caches increments, uses ARM64 NEON or x64 SSE2 phase update; scalar fallback. Grouped path remains disabled in plugin, only enabled by benchmark/test. No AVX2 implementation yet.
- Full-engine scalar/grouped A/B test including pitch bend. Short CPU benchmark on 1/8/32 voices, endurance 60 seconds of rendered audio for 32-voice granular and grouped HD. Results not available until CI; no realtime guarantee. HD memory compression, preset listening, host automation and graphical validation remain pending.
