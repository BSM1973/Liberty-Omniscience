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

## v0.3.3 — COSMOS ENGINE FINAL CONTROL (candidate)
- Baseline v0.3.1: at 48 kHz/128, 32 notes/20 oscillators, HD unison x16 1999.72% macOS and 5261.01% Windows, granular 264.457%/378.453%, analog 125.015%/108.983%. Percent of block deadline, not OS CPU usage. Not real-time capable under this stress case.
- AVX2 phase kernel isolated in its own translation unit, runtime CPUID + OSXSAVE/XGETBV, fallback SSE2. NEON retained. Experimental grouped path remains off in plugin pending validated benchmarks.
- Factory HD virtual grid removes redundant 16,785,408-byte allocation (255 intervals align with 15 legacy intervals); imported tables still use full band storage. Skip unused modulator sine.
- Processor QA renders every one of 200 presets to stereo PCM WAV, measures RMS/peak/finite and compares audio after state roundtrip with automated master value. This is not listening or real DAW validation. Host automation across DAWs, imported paths, full aliasing analysis, visual validation and further CPU optimization remain pending.

## v0.3.4 — COSMOS EXTREME OPTIMIZATION (candidate)
- v0.3.2 confirmed green, 32 notes/20 oscillators/16 unison: scalar 2192.18% macOS / 4136.05% Windows, grouped NEON 1063.18% / SSE2 1598.10%; 60s grouped 845.727% /1602.47%. All finite; still above realtime budget.
- Cache band, adjacent table frame pointers, morph coefficient and modulator increments per lane. Optional 32-sample tile kernel (lane-major scalar interpolation); preserve summation order and use per-sample processing for release. Both grouped and tiled paths remain disabled in shipping processor until native results are reviewed. No new SIMD interpolation claim: AVX2/SSE2/NEON phase kernel retained for grouped mode; tile mode scalar. Granular wrap fast path with modulo fallback.
- A/B test covers odd block lengths, pitch bend, morph change and note release; granular wrap comparison. Benchmark adds tiled mode and 60s endurance. CPU improvement not established until native reports. 200-preset QA retained from v0.3.3, listening and GUI verification remain manual. No claim of realtime capability.

## v0.3.5 — COSMOS ULTRA PERFORMANCE (candidate)
- v0.3.3 both green, 200/200 processor WAV/state checks on each OS. 32-note HD grouped NEON 1271.95%, AVX2 1724.67%; granular 350.578%/280.279%. No realtime qualification. All artifacts available via run 45864532988.
- Experimental tiled interpolation arithmetic implemented with NEON/SSE2 four float lanes and AVX2 eight float lanes, scalar gather of wavetable samples, no audio-thread heap allocation. Scalar interpolation fallback; native vs auto SIMD selectable in benchmark on same machine. No claimed speedup until measured. Full-engine A/B includes odd buffers, pitch, morph, releases.
- Optional 4097-point Hann envelope LUT with interpolated read; prewarm outside render, reference cosine retained. Added numeric error bound and engine A/B. 60s audio endurance for auto SIMD and LUT paths. Plugin stays on reference path until qualified.
- Multicore study: independent voices can be rendered to preallocated per-worker stereo buffers then reduced in deterministic voice order. Do not introduce per-block allocation, unbounded waits or OS thread creation in callback. Host scheduling/workgroup coordination and deadline measurements required before implementation. Preserve 32 MIDI voices and all 20 oscillators/unison counts; no automatic quality shedding or adaptive stealing changes. Listening, actual host and graphical fidelity validation remain outstanding.

## v0.3.6 HYPER ENGINE (candidate)
Cache base pitch and tune per oscillator, including reference path with identical arithmetic. Experimental granular plan caches loop bounds, pitch scale, spawn interval and normalization, combined with Hann LUT in benchmarks and A/B. No adaptive voice reduction. v0.3.4 verified green; best 32-note HD 617.952% macOS grouped and 1144.54% Windows tiled; 200/200 automated presets on both. v0.3.5 native SIMD interpolation still pending, no backend activation until results. Manual listening/UI/DAW and multicore implementation remain pending.

## v0.3.7 MULTICORE ENGINE candidate
Offline experimental 2/4-worker persistent pool, fixed per-voice stereo buffers, disjoint voice ranges, ordered reduction and A/B tests. Blocking mutex/condition-variable barrier explicitly not suitable for realtime callback; never instantiated in plugin. No thread scheduling or performance guarantee. Benchmarks compare single-thread and worker variants, including 60s rendered audio. v0.3.5 verified 200/200 and native SIMD A/B; HD best 217.974% macOS and 130.362% Windows. v0.3.6 pending. No change to voice budget. Manual listening/UI/host qualification pending.

## v0.3.8 REALTIME PERFECTION candidate
Activate qualified single-thread tiled interpolation on macOS NEON and Windows runtime AVX2/SSE2 fallback, with original scalar paths for unsupported modulation. Apple-only granular LUT/prepared path based on v0.3.6 measurements; Windows retains reference granular. Processor QA compares optimized output against reference processor after state restore (peak error tolerance 1e-4). Offline worker pool remains outside plugin. Native build and full preset comparison pending; no real-time deadline guarantee or Apple Audio Workgroup integration. Manual listening, GUI and host testing remain outstanding.

## v0.3.9 PROFESSIONAL ENGINE candidate
Optional sine LUT and square phase wrap optimization, prepared ADSR coefficients, numerical equivalence tests at 44.1/48/96 kHz. Coherent imported wavetable spectral residual probe (not comprehensive aliasing certification). Benchmark p95/p99 and deadline miss counts, analog prepared scenario, corrected parallel HD backend label. v0.3.7 native multicore benchmark 4 workers HD 127.811% macOS/62.9178% Windows; 60s 117.358%/63.1763%, peaks exceed deadline. Offline pool remains offline. Analog/ADSR experimental until measured; v0.3.8 processor QA pending. Manual host, UI and listening qualification pending.

## v0.3.10 MASTER ENGINE candidate
Offline pool compacts active voice indices at block boundary, balances active indices across four workers, skips empty voice ranges and bypasses worker rendezvous for one active voice. Ordered reduction and MIDI lifecycle A/B including silence and releases. Additional active-pool scenarios in CPU benchmark and 60s endurance; p95/p99/deadline metrics retained. v0.3.8 native processor optimized vs reference 200/200 presets both platforms, four-worker HD 96.383% macOS/56.6282% Windows short, 113.307%/56.3384% endurance. No realtime pool activation: blocking condition variables remain. Apple Audio Workgroups requires host-provided workgroup context, lifecycle and QoS integration, not added here. Analog/ADSR activation pending v0.3.9 results. Full aliasing/listening/DAW/UI validation pending.

## v0.3.11 ULTIMATE ENGINE candidate
Adaptive participating worker count bounded by active voice count (offline pool only), deterministic reduction, additional 0–4 voice transition A/B. Benchmark separates sine LUT and ADSR plan. v0.3.9 green: 60s four-worker HD macOS 121.89% mean, P99 291.413%, 13565/22500 misses; Windows 67.3273%, P99 79.6838%, 82 misses. Combined analog optimization macOS 59.8326% vs 69.6082% reference, Windows 92.2659% vs 82.5698%; not enabled. No realtime pool or workgroup integration. Full spectral, listening, GUI, host tests pending.

## v0.3.12 FINAL QUALIFICATION candidate
Actual JUCE processor callback matrix: seven representative factory presets, 44.1/48/96 kHz, 64/128/512 sample buffers, 1/8/32 simultaneous MIDI notes (189 scenarios). Sample-offset notes, pitch wheel, sustain, volume automation and state restore; optimized vs scalar numerical pass criteria (relative RMS and peak error <1e-4, finite, audible signal). Callback deadline metrics reported separately; no deadline qualification inferred from green CI. Existing 200 WAV checks retained. v0.3.10 green: Windows offline four-worker HD endurance 42.4072% mean, 84.225% max, zero misses/22500; macOS 108.844%, 9181 misses. Multicore not in callback, no manual host listening/UI qualification. No changes to plugin DSP defaults pending isolated benchmarks.

## v0.3.13 REFERENCE EDITION candidate
Enable tested analog sine LUT and prepared ADSR on Apple Silicon only, disable both in QA reference. Windows analog remains unchanged. Dark Cosmos metallic blue panel gradients and knob tracks; update stale footer version. Grain page navigation no longer changes oscillator sound; wavetable import controls visible only in wavetable mode; capture intended import mode at file chooser launch. Existing 200 preset and 189 processor QA matrix retained. v0.3.11 green; mac combined analog 52.5896% vs reference 73.1778%, Windows 94.327% vs 88.0404%. Shared-run variability and multicore deadlines remain unresolved; no realtime multicore activation. UI screenshot and manual listening/host verification remain pending.
