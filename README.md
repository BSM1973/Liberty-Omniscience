# Liberty Omniscience — v0.2 HYBRID SYNTHESIS

Code candidat de développement : 4 couches, 20 oscillateurs, 32 voix partagées, VA, wavetable, FM/PM, AM, lecture de sample, unisson, Hard Sync, ring modulation et saturation. Page oscillateurs avec aperçu et import WAV/AIFF. Les 100 presets v0.1 restent la base de la bibliothèque; leur validation sonore et la compatibilité des sessions restent à confirmer. Voir [roadmap](docs/ROADMAP.md) pour les limites et les tests.

Build : `cmake -S . -B build`, `cmake --build build --config Release`, `ctest --test-dir build -C Release --output-on-failure`. Pas d'installateur commercial à ce stade.
