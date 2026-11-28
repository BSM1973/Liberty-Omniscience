# OMNISCIENCE — v0.7.9 pre-commercial QC
Source baseline v0.7.8: a9cce534, workflow 46169311283, both platforms successful. No new effects and no DSP changes in 0.7.9.

## Measured baseline
At 48 kHz / 128 samples, 40 FX spectral FFT2048 mean budget macOS 39.9378%, Windows 58.4728%, 0 deadline misses in the 1-second scenario. v0.7.7: 53.5524% / 70.1569%, 90 misses each. Mixed serial 60 seconds: 28.8429% / 61.0991%; allocated FX bytes 77598248 versus 83495848 in v0.7.7. Ten-minute mixed run: finite throughout, 82/225000 and 5/225000 deadline misses, respectively. These are CI runner measurements, not guaranteed host performance.
300/300 reference profiles finite and peak <= 0.9001 on both platforms; 47 trims. Alias measurement (21 kHz component relative to 9 kHz carrier, mean across 10 analog FX): 1x -7.79 dB, 2x -27.08 dB, 4x -30.89 dB, 8x -57.49 dB, 16x -59.50 dB. A single-frequency probe is not a full-band alias specification.
Automated MIDI Learn and FX state/automation roundtrip passed. Layout geometry tests and generated screenshots passed at 900/1024/1200/1800 pixels. Factory regression checks passed in v0.7.8.

## New gates
Require all 300 profile rows, measured peak and finite values, all 50 alias probe rows, and finite 40-FX endurance of at least 600 seconds. Report deadline misses, do not silently classify them as zero. Native AU validation via auval, VST3 architecture and package checks, existing isolated Developer ID signing/notarization/stapling, installer download verification via SHA256. Windows installer remains unsigned unless an existing Authenticode process is confirmed.

## Manual release gates
Not completed by CI: actual listening to 100 modes/300 profiles with representative music, real DAW AU/VST3 loading and PDC including changes during playback, real 13-inch screen legibility and accents, installer install/uninstall smoke tests on physical systems. Do not advertise final acoustic or no-dropout certification until these are completed. Commercial packages are for pre-commercial checks; no store publication is performed.
