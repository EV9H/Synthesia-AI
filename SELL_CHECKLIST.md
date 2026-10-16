# Sell checklist (Harmonia)

Shipping brand is **Harmonia**. The GitHub repo stays `EV9H/Synthesia-AI` until a dedicated rename. MIDI quality is the product. Preview audio is secondary.

## Do not do in a quality pass

- Do not rename the CMake target, bundle id, manufacturer code, or state XML mid-quality-sprint.
- Do not loosen in-key gates to chase the scorer.

## Display name (done this round)

User-facing strings now say Harmonia:

- Plugin `getName()` / standalone window title
- Wordmark in the editor
- Settings About line (`Harmonia 0.1.0 · MIDI generator for your own sounds`)
- `SynthesiaRender` `JucePlugin_Name`

Left unchanged on purpose (bundle / state / paths):

- CMake target `SynthesiaAI`
- `juce_add_plugin(SynthesiaAI)` `PRODUCT_NAME "Synthesia AI"` (VST3 folder `Synthesia AI.vst3`)
- `COMPANY_NAME "SynthesiaAI"`
- `PLUGIN_MANUFACTURER_CODE Synt` / `PLUGIN_CODE SyAI`
- State XML `SynthesiaAIState` / `SynthesiaAILocal`
- User-data folder `SynthesiaAI/`
- GitHub repo `EV9H/Synthesia-AI`

## Bundle-id / plugin rename checklist (do in one dedicated release, not a quality pass)

Do these together or DAWs will see a new plugin and lose saved instances:

1. Pick final ids: e.g. `COMPANY_NAME "Harmonia"`, `PRODUCT_NAME "Harmonia"`, target `Harmonia`, `PLUGIN_MANUFACTURER_CODE Harm`, `PLUGIN_CODE Hrmn`.
2. AU bundle id / VST3 CID: JUCE derives these from company + product + codes. Changing them registers a **new** plugin. Plan a migration note for hosts.
3. Update `CMakeLists.txt` `project()`, `juce_add_plugin()`, `JucePlugin_Name` (already Harmonia on the render host).
4. Rewrite state XML tags **with a reader that still accepts `SynthesiaAIState`** so existing sessions load.
5. Move user-data `SynthesiaAI/` → `Harmonia/` and copy presets/settings on first launch.
6. Update `Tools/validate_plugin.sh` paths (`…/VST3/Harmonia.vst3`).
7. Update README install paths and DAW “add instrument” name.
8. Rebuild macOS AU/VST3 + Windows VST3, run pluginval + Steinberg validator on the **release** binary.
9. Do **not** rename the GitHub repo in the same commit as the bundle id change unless you also update clone URLs, CI, and Pages.

## Remaining sell blockers

1. **Signed installer** — notarized macOS VST3 + AU + standalone; signed Windows VST3 + standalone. Steinberg/pluginval on those binaries.
2. **Lemon Squeezy license wiring** — replace `LicenseStub` with real serial/account gate, offline grace, no nag on the MIDI export / drag path.
3. **Cloudflare Pages** — product site (promise: MIDI generator for your own sounds), download + license redeem, not a synth demo page.
4. **Real DAW drag test** — confirm 960 PPQ SMF, one track per part (Chords/Bass/Synth/Piano/Drums), GM drum channel 10, in Ableton, Logic, and FL after any rename. `MidiExporter` already writes this; host test is still outstanding.
5. **Manual / first-run** — 30-second “generate → drag MIDI → load your piano” tour. No preview-synth tutorial as the hero path.
6. **Presets vs empty** — factory genre kits and Inspire seeds must sound musical on first generate in C major; empty-state should not look broken.

## Inspire seeds (this round)

One-click combo **Inspire seed…** on the editor: Radio Pop, Dusty Lo-fi, Night House, Chrome Trap, Velvet R&B, Wide Cue, Pop Uplift, Lo-fi Song, House Anthem, Late Pocket. Each lands a locked progression, mood, density, and rhythm. Dice Inspire still rolls a new plan.

## Per-part R / V + 960 PPQ (this round)

- Arrangement space / call-response / register pass runs only when generating **All** parts, so regenerate / vary still keep other parts locked.
- `MidiExporter` still writes 960 PPQ, named tracks, GM drum channel 10. Re-test in a real DAW before charging (blocker 4).

## Quality gates (keep)

- `out_of_key` (not a borrowed chord tone) ≤ 3%
- strong-beat chord-tone ≥ 80%
- chromatic passing ≤ 3.5%
- sampler pitch check PASS
- lint errors fail the eval run
