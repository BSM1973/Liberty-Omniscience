# Liberty Omniscience — v0.1 COSMOS FOUNDATION

Instrument virtuel original de Liberty Music Software, C++20 / JUCE 8. Dark Cosmos.

Première base : 4 couches, 32 voix, quatre formes d'onde, filtre passe-bas, ADSR, gain, pan et accordage par couche, 100 patches de développement paramétriques, navigateur avec recherche, sauvegarde de l'état et automation. Les patches nécessitent une validation d'écoute.

Build local : `cmake -S . -B build`, `cmake --build build --config Release`, `ctest --test-dir build -C Release --output-on-failure`. Pour les tests DSP seuls : `-DOMNI_PLUGIN=OFF`.

Voir [roadmap](docs/ROADMAP.md). Les builds de développement ne sont pas signés ni notarisés. La parité fonctionnelle avec la référence reste un objectif à long terme.
