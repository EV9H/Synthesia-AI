# Sell checklist (next commercial round)

MIDI quality is the product. Preview audio is secondary. Do these before charging money.

## Do not do in a quality pass

- Do not rename the plugin, bundle id, or manufacturer string mid-quality-sprint.
- Do not loosen in-key gates to chase the scorer.

## Remaining sell blockers

1. **Name / branding** — pick the shipping name, AU/VST3 bundle ids, and a one-line promise (“MIDI generator for your own sounds”).
2. **Installer** — signed VST3 + standalone for macOS and Windows; Steinberg/pluginval on the release binary.
3. **Licensing / trial** — serial or account gate, offline grace, no nag in the MIDI export path.
4. **Drag-to-DAW** — confirm 960 PPQ SMF, one track per part (Chords/Bass/Synth/Piano/Drums), GM drum channel 10. Already how `MidiExporter` writes; re-test in Ableton, Logic, FL after the rename.
5. **Inspire presets** — ship 6–12 one-click seeds that land a good 8-bar plan (one per genre plus a couple of song-form variants). Label them by use, not internal ids.
6. **Per-part R / V** — regenerate and variation still have to keep other parts locked. Manual check on the shipping build.
7. **Manual / first-run** — 30-second “generate → drag MIDI → load your piano” tour. No preview-synth tutorial as the hero path.
8. **Presets vs empty** — factory genre kits must sound musical on first generate in C major; empty-state should not look broken.

## Quality gates (keep)

- `out_of_key` (not a borrowed chord tone) ≤ 3%
- strong-beat chord-tone ≥ 80%
- chromatic passing ≤ 3.5%
- sampler pitch check PASS
- lint errors fail the eval run
