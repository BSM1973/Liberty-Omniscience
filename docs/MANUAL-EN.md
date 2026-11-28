# OMNISCIENCE — User manual
**Liberty Music Software · v0.7.9 · DARK COSMOS**

## Installation
Apple Silicon macOS: extract the archive, open the PKG and follow the installer. Restart your DAW. AU and VST3 are installed in the system Audio/Plug-Ins folders. Windows x64: open the EXE, follow the installer and restart your DAW; rescan VST3 if necessary. An unsigned Windows EXE may trigger SmartScreen. Distribution targets ARM64 AU/VST3 and Windows x64 VST3; Intel macOS and AAX are not advertised. No manual disabling of system protections is required.

## Getting started
Create an instrument track, load Omniscience, select a preset and play a MIDI keyboard or the DAW piano roll. Start at a moderate listening level. The browser offers 370 factory presets. Save the project before major changes.

## Synthesis architecture
Four layers, five oscillators per layer, up to 32 voices. Select a layer to adjust oscillators, envelopes, filters and levels. The engine includes waveform, wavetable and granular synthesis. The modulation pages and matrix map sources to destinations with adjustable depth. Macros group performance controls. Monitor master level when combining layers.

## NEXUS
The XY area and morphing allow timbral changes. Trajectories, snapshots, motion and mutation provide variations and recall. Store a snapshot before modifying a configuration you want to retain.

## FX racks
Open NEXUS then FX. Choose one of four layer racks or the master. Each rack has eight slots, 40 in total. Select and enable an effect; adjust Dry/Wet, input, output and effect-specific A/B controls. Use serial or parallel routing and the Up/Down controls to reorder effects. Search and favorites simplify browsing.
The 100 effects span utilities, dynamics, analog, modulation, delay, reverb and spectral/creative processing. Three profiles per effect provide 300 settings. Forty-seven profiles have output trims calibrated against one reference signal; actual peaks and loudness remain signal-dependent.

## Quality, latency and performance
Analog effects 19–28 offer 1x, 2x, 4x, 8x and 16x oversampling. Higher factors generally reduce aliasing at additional CPU cost. FIR oversampling adds 24 samples above 1x. Spectral effects 79–89 offer 512, 1024, 2048 and 4096 point FFTs, approximately 10.7, 21.3, 42.7 and 85.3 ms per slot at 48 kHz. Serial latencies add; parallel paths and layers are aligned. The plug-in reports calculated latency to the host. Avoid resolution changes during critical recording: live host adaptation is DAW-dependent.
Use moderate settings while recording and higher quality during mixing when CPU allows. Creative delays and granular delays are not all compensation latency. Time Stretch is bounded real-time granular processing rather than offline editing. Formant-envelope correction is approximate.

## MIDI Learn, automation and state
Select the effect, enable MIDI Learn, then move the MIDI controller. Clear CC removes the assignment. Plug-in parameters are available for host automation as supported by the DAW. Save and reopen the project to verify state. Effect IDs and saved output gains are preserved; trims apply only when explicitly selecting a profile.

## Monitoring and troubleshooting
The spectral 24-band display, gain reduction, sample peaks and stereo correlation aid monitoring. Not all parameter-response drawings are measured acoustic responses. Increase the DAW buffer or reduce polyphony, FFT size or oversampling if overloaded; disable unused effects. If there is no sound, check the MIDI track, channel, enabled layers and master.
When reporting a problem include version, operating system, DAW, sample rate, buffer size and reproducible steps. https://libertymusicsoftware.com/

## Status
This manual accompanies pre-commercial qualification. Build and reference-signal tests and installers do not replace listening tests or actual DAW checks. See QC-v0.7.9.md.
