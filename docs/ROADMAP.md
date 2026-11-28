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

## v0.3.14 MASTER VALIDATION candidate
Reuse immutable imported WAV samples and HD tables for matching path, size and modification timestamp; avoid repeated state restore consuming session limits. Missing sources still clear on restore; invalid layer guard. QA creates a stereo 2048-sample WAV, imports as sample and table, restores twelve times, checks retained counts and missing-file behavior. AIFF and visual editor qualification still pending. v0.3.12 green 200/200 presets and 189/189 DSP matrix both platforms; offline multicore deadlines unresolved. v0.3.13 pending.

## v0.3.15 ENGINE FINAL CONTROL candidate
AIFF PCM16 stereo 48 kHz fixture import and restore; nine actual editor snapshots (900/1200/1800 widths, layer/oscillator/granular pages), sibling bounds intersection and canvas containment checks. Not a complete visual or text-legibility certification. v0.3.13 green 200/200 presets and 189/189 matrix per platform. Offline multicore endurance HD 90.7054% mac/62.2895% Windows; misses 2095/41. Real-time multicore remains disabled. v0.3.14 pending. No new DSP defaults.

## v0.4.0 FILTER PERFECTION foundation candidate
16 selectable new filter responses using TPT one-pole and cascaded state variable stages, plus historical and bypass. Two filters per layer, series/parallel, resonance, input drive, parallel balance and output trim. New filter page and 16 dedicated procedural presets; preserve 200 legacy presets with default historical filter and bypass second. Schema 5 migration. Impulse/frequency tests and 216 preset QA; 12 editor snapshots. Not 70 distinct circuit models yet: Ladder, Comb, Formant, self oscillation, modulation, response plot and automatic compensation remain future work. v0.3.15 must pass before merge. Multicore offline only.

## v0.4.1 ANALOG FILTER LEGENDS candidate
Eight analog-inspired responses (not circuit-accurate emulation), bounded nonlinear feedback, two internal substeps, transistor/diode/OTA-inspired limiting, optional bounded resonance gain compensation (not loudness matching), 8 factory presets. UI measures actual per-filter small-signal impulse responses off audio thread; nonlinear response and self-oscillation not certified. High-resonance and cutoff-jump tests, 224 preset checks retained. v0.4.0 pending. Full aliasing, musical validation, reliable self oscillation and model calibration pending. No multicore realtime activation.

## v0.4.2 SPECTRAL FILTER UNIVERSE candidate
16 new selectable responses: four fractional-delay comb variants with feedback below unity, five three-band vowel formants, two allpass, two bell, two shelf and triple resonance. RBJ biquads, fixed comb buffers (9602 floats per filter state, ~9.4 MiB additional voice pool), no allocation in tick. Filter plot cutoff editing A or Shift B using host gestures. 240 presets, comb impulse polarity and allpass magnitude tests plus existing stability tests. IDs appended; normalized host choice automation scaling across model-count changes still requires host validation. No calibrated self-oscillation or alias reduction claim; nonlinear saturation remains unchanged. v0.4.1 pending; no realtime multicore activation.

## v0.4.3 COSMOS CREATIVE FILTERS candidate
12 selectable responses: 3 continuous SVF blends, harmonic/odd resonators, 2 dry/allpass phasers, 2 damped combs, 3 interpolated vowel banks. Independent A/B morph, common signed key tracking/velocity and per-voice filter ADSR depth, update at 16-sample intervals (and render segment start); zero default modulation preserves existing filter settings. Dedicated modulation page, 15 screenshots, 252 factory presets. Impulse stability at three morph positions and ADSR lifecycle tests. Not LFO phaser sweep or full modulation matrix, not calibrated self-oscillation. CPU overhead and normalized automation compatibility remain to measure. v0.4.2 pending.

## v0.4.4 ULTIMATE FILTER COLLECTION candidate
18 new selectable responses (70 plus historical/bypass), three envelope-following Vactrol/LPG-inspired models, two asymmetric SVF with first-order ADAA waveshaping, four inharmonic metallic banks, four multiband variants, four hybrid biquad chains, one amplitude-bounded self-resonator (input excited, radius above unity at high resonance). 270 factory presets. Tests across 44.1/48/96 kHz, oscillation 440 Hz after excitation and one folded harmonic comparison. Avoid zeroing 9602-sample comb buffers on voice reuse; invalidate readable history instead. Allocated memory remains ~9.4 MiB for combs, no full aliasing or existing ladder self-oscillation calibration claim. No realtime multicore, listening or GUI inspection certification. v0.4.3 pending.

## v0.4.5 FILTER PROFESSIONAL QUALITY CONTROL candidate
No additional models. New CosmosFilterQA target produces 5184 coherent-tone scenarios (72 selections, 3 sample rates, 3 resonance values, 2 drive levels, 2 auto gain settings, 2 tones), 2592 impulse response points and 40 actual engine vs independent serial/parallel reference scenarios. Reports measured RMS/peak, fundamental gain, THD, unclassified residual (not isolated alias energy), time/sample, finite/bounded checks. Frequency axis labels in real response plot. Existing 270 preset QA and geometry checks retained. No claim of individually calibrated slopes, compensation or self oscillation before reports; no unvalidated DSP parameter changes. v0.4.4 must pass first.

## v0.4.6 FILTER MASTER VALIDATION candidate
No new models. Add measured grid peak and two octave-slope estimates (not complete asymptotic calibration), 243 resonance-tail scenarios, 9 isolated folded-third harmonic probes, memory sizes. Normalize self-resonator nonlinear damping to 48 kHz reference and test steady RMS across 44.1/48/96 kHz. Cache identical per-voice cutoff ratios inside render segment, preserving coefficient calculation order at changes. Test Comb reset vs fresh states. Fix frequency-label edge clipping. Existing compensation curves and other filter voicings unchanged until v0.4.5 reports are available; measured data, subjective audition and host automation verification pending. v0.4.4 green (270 presets, 189 processor scenarios, 15 screenshots per platform); v0.4.5 in progress.

## v0.5.0 MODULATION UNIVERSE candidate
Eight per-voice note-retriggered LFOs with sine, triangle, rising/falling ramp, square and deterministic sample/hold; bipolar depth, phase, 0.01–40 Hz or seven tempo divisions; one route per LFO to filter A/B cutoff, amplitude or pan, all layers or one. Existing 270 presets retained, all routes default off. Filter modulation updates at render segment start and 16 samples, amplitude/pan sample accurate. Dedicated eight-tab page, 18 screenshots, APVTS schema 8 migration, unit/engine LFO tests. Not yet 12 envelopes or 64-route matrix; no transport phase lock or custom wave editor. v0.4.6 pending.

## v0.5.1 ADVANCED LFO STUDIO candidate
12 shapes, bipolar/unipolar, note retrigger/free-running/host PPQ sync (tempo division and playing host required; free-run fallback). Free phases advanced in serial render including silence; offline parallel benchmark renderRange does not advance global clocks, advanced modes not qualified there. Four added targets: pitch ±12 semitones per unit, wavetable morph, grain position, grain density. Tiling disabled on oscillator-modulated layers; unchanged paths retained. Actual JUCE overview of eight static waveform shapes, no live phase cursor yet. Schema 9 and 270 unchanged presets. Test shape range, random continuity and actual added destinations. Host normalized automation for expanded choices, transport loops, CPU and visual inspection pending. v0.5.0 must pass first.

## v0.5.2 COSMOS ENVELOPE UNIVERSE candidate
12 independent note-triggered polyphonic modulation envelopes (in addition to legacy volume/filter ADSR), ADSR/AHDSR/fixed multisegment attack-hold-intermediate-decay-release, linear/quadratic/sqrt curves, gate/loop/one-shot (note release stops playback), seconds or quarter-note units following BPM, signed depth and velocity scaling. One route per envelope to existing eight destinations, all or selected layer. Precomputed increments, active sources only, no new callback allocation. Real JUCE page with 12 tabs and draggable sustain/intermediate levels plus time knobs, schema 10 migration, 270 presets unchanged, 21 screenshots. Tests 27 mode/curve/play combinations, timing and real twelfth envelope route. Arbitrary multi-point drawing, live cursors, 64 matrix routes, host timing and performance verification remain future work. v0.5.1 pending.

## v0.5.3 COSMOS MOD MATRIX PRO candidate
64 extra independent routes, 154 source selections (8 LFO, 12 envelopes, velocity, key, modwheel, channel/poly pressure, pitch, CC0–127), 12 DSP destinations (existing eight plus FM/AM depth and ratio, filter A/B resonance), target layer/all, signed amount, uni/bipolar conversion, enabled. Existing direct 20 routes retained. Sources evaluated once per voice/sample, independent of direct depth for matrix. MIDI state preallocated outside callback; no audio callback allocation. Eight banks of eight rows, APVTS schema 11, 270 presets default inactive. Tests 64 additive routes and representative source classes; 24 screenshots. Host automation/CPU and full MIDI tests pending. FM/AM changes act on compatible oscillators in target layer, no per-oscillator targeting yet. v0.5.2 pending.

## v0.5.4 COSMOS INTELLIGENT MODULATION candidate
Eight assignable macro source parameters, per-route oscillator target 0/all or 1–5 within layer (oscillator destinations only), four signed transfer curves, optional 0–200ms MIDI source smoothing, eight global per-voice LFO depth destinations with bounded one-sample delayed feedback, first-active-voice route contribution telemetry, route detail and macro page. Right-click route number learns next incoming CC source; GUI timer applies host gesture, no parameter notification from MIDI thread. Schema 12, 270 presets unchanged, 27 layout screenshots. Tests macro route 64, targeted oscillator, smoothing and state roundtrip. No generic knob drag-and-drop, modulated rotary halos, macro renaming or complete DAW validation yet. CPU/memory measurements pending. v0.5.3 must pass first.

## v0.5.5 COSMOS VISUAL MODULATION DESIGNER candidate
Drag source picker to supported layer/filter/oscillator/grain knobs creates a free route, right-click supported knobs arms next CC learning; routes are not overwritten. Emerald halo depicts signed first-active-voice matrix contribution (not exact normalized DSP parameter value). Active LFO phase telemetry, five-point piecewise linear custom transfer curve per route, editable macro labels saved as state properties. Schema 13, 270 factory presets, 30 screenshots, curve and phase tests. Free-point multisegment envelopes still pending; custom route curve is fixed x positions. Expanded choice host normalized automation still needs testing. v0.5.4 pending.

## v0.5.6 COSMOS MODULATION PERFORMANCE candidate
Separate optional free-point envelope mode, 2–16 points with fixed time endpoints (initial value zero), draggable internal x/y and last y, double-click insert, right-click remove, shift-click outgoing linear/quadratic/sqrt segment curve. Existing structure choices unchanged; free total duration uses segmentTime (seconds or beats), endpoint sustain or automatic release for loop/one-shot. Fixed memory and sanitized ordered positions, state schema 14. Cache route layer ranges, reduce temporary source array, publish LFO phase only last sample, avoid unchanged overview repaint. Exact A/B of route layer range optimization, real processor MIDI learn CC74 and free point state tests, 33 screenshots. 48k/128 benchmark inactive32,64 routes8/32, 60 rendered seconds32 voices with all 20 sources; deadline percentages not OS CPU. No direct cross-version audio comparison yet; actual host gestures and free editor interaction not qualified. v0.5.5 pending.

### v0.5.6 geometry hotfix
Run 46026158001 compiled and passed DSP, 270 presets, 189 processor scenarios, MIDI Learn and state tests on both platforms; layout QA failed. Classic envelope free-point toggle occupied y398..418 while the structure combo starts at y417; horizontal overlap created a one-pixel intersection. Move toggle to y396..415 and reduce width to 130, leaving a two-pixel gap above the selector. Free-point mode retains dedicated row. Keep the complete 33-screenshot geometry gate and include page/width/rectangles in collision diagnostics. DSP unchanged. Rebuild required.

## v0.5.7 MODULATION PROFESSIONAL QUALITY CONTROL candidate
No new synthesis features. Test each 8 LFO, 12 free envelopes with 16 points each, 8 macros, all 64 route slots, MIDI channel/poly pressure/pitch/CC isolation, transport PPQ seek/tempo/fallback, and real JUCE drop-target callback/halo based on processor telemetry with additional screenshot. Improve pending knob MIDI learn (activate only when CC captured, no competing learns), reject invalid drag sources, avoid dormant direct modulation disabling oscillator fast path, reuse GUI patch snapshot. Cross-version regression downloads same-platform v0.5.5 factory audio artifact and checks 270 PCM16 renders within 4 counts. Retain 60-second modulation benchmark; results v0.5.6 pending. No live OS pointer gesture, host automation certification, subjective listening or full CPU real-time guarantee. v0.5.5 both green. Baseline artifact access required for regression.

### Integration after v0.5.6 hotfix
Run 46034567203 passed both platforms, 270 presets, 189 DSP scenarios, 33 screenshots, filter QC and MIDI learn. Preserve the validated y396..415 toggle rectangle and improved collision diagnostics. Modulation endurance (48 kHz, 128 samples, 32 voices, 64 routes): macOS mean 71.4574% of deadline, P99 125.388%, 508/22500 misses; Windows mean 156.684%, P99 178.436%, 22500/22500 misses. All samples finite. Real-time performance remains open; no certification. v0.5.7 tests must execute before advancing.

## v0.6.0 COSMOS NEXUS XY ENGINE
Optional four-layer XY gain mixer, bilinear/equal-power laws, one-pole axis and enable smoothing 0–500 ms, manual locks, mouse-release center, inversion, macro or global last-received CC source per axis, independent learn and clear, APVTS state/automation, sources 163/164 in matrix. Existing source IDs retained but normalized choice automation may shift; verify in hosts. Macro overrides MIDI/manual, CC overrides manual after first receipt; learned mapping persists, live CC value does not. Locks apply to manual controls only. Gain meters are blend coefficients, not measured audio level. Per-voice local smoothing matches global state advanced in serial render; no new audio allocation. Offline parallel renderRange NEXUS not qualified. Schema 15, 270 factory presets disabled, 36 geometry captures plus halo, retained cross-version tests, gain law/corners/audio/MIDI macro matrix/smoothing/state tests. Full parameter interpolation in v0.6.1. v0.5.7 both green and cross-version PCM 270/270 per OS; modulation 32-voice CPU budget remains exceeded. NEXUS performance/host automation and manual interaction pending.

## v0.6.1 COSMOS ADVANCED MORPH ZONE
Optional continuous parameter interpolation across four layers, weighted linear/quadratic/sqrt curves, log interpolation of positive frequency/time/ratio, family switches and tune/gain protections, amount 0–1, non-destructive local patch, NEXUS subpage and state schema 16. Discrete modes/assets retained; existing XY gain mix provides crossfade. Block-rate parameter interpolation using smoothed XY at segment start, not sample-accurate; global LFO/matrix morph and individual parameter locks not yet implemented. 39 geometry captures, state and DSP tests, historical regression retained. v0.6.0 both green. Build pending.

## v0.6.2 COSMOS MOTION RECORDING
Fixed capacity 1024 uniformly spaced XY samples; audio-block recording of manual axes, 1–32 bars assuming 4/4 or 1–120 seconds, loop/reverse/ping-pong/one-shot, speed/intensity/rotation, NEXUS playback override of manual/macro/CC, PPQ anchored when playing with free clock fallback, saved clip XML and parameter migration schema 17. Atomic sample publication (no dynamic allocation in audio callback; concurrent state mutation not fully qualified), record/stop/clear UI and plot, 42 geometry captures. Block-rate sampling, one-shot holds last value, live recording trace only after stop, no overdub, quantization, undo, external file import/export or global LFO/matrix morph yet. Existing presets motion off. DSP core tests and historical regression retained. Build pending.

## v0.6.3 CREATIVE TRAJECTORY DESIGNER
50 generated paths in ten families, 128-point editable path with linear or cubic Bezier interpolation, open/closed, undo/redo of geometry, search/favorites, explicit apply to existing 1024-sample motion clip, scale/center/easing/macro scaling and periodic variation, existing rotation and tempo settings. Applied sampled motion and controls persist (schema 18); original editable control geometry is not yet persisted. Variation is deterministic periodic not stochastic; LFO deformation, external import/export, overdub/quantization and live preview of unapplied edits remain open. 45 geometry captures and factory bounds/closure tests; historical DSP gate retained. Build pending.
