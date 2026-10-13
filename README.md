# Synthesia AI

An AI-powered MIDI generation plugin for DAWs (Ableton Live, FL Studio, etc.). Built with JUCE, this VST3/AU plugin generates musical MIDI sequences using either offline algorithmic generation or AI/LLM-based generation.

## Features

### Current (MVP)
- **Three generators**:
  - **Genre engine** (default): offline arrangements with chord progressions, bass, synth lead/arp, piano and drums, each on its own MIDI channel. Genres: Pop, Lo-fi / Jazz, EDM / House, Trap / Hip-hop, R&B / Neo-soul, Cinematic. Drums use the General MIDI percussion map on channel 10.
  - **Markov Generator**: classic single-line melody (no internet required)
  - **LLM planner**: OpenAI-compatible endpoint (your key, configurable base URL + model) returns a compact JSON arrangement plan only — form, chords, energy/density, motif, parts, tempo/swing, genre. The genre engine realises every note, so nothing is out of key. Without a key, prompt keywords pick genre/energy offline.
  
- **UI Controls**:
  - Dark, resizable editor with **Generate / Plan / Import / Style / Sound** tabs, readable 15pt fonts, and a persistent piano-roll footer
  - Genre, part (All / Chords / Bass / Synth / Piano / Drums), key and scale
  - Form (loop or song: intro / verse / pre / chorus / drop / bridge / break / outro) and length (1-32 bars)
  - Chord input (symbols like `Am9 Fmaj9` or Roman numerals, `|` for bar groups) with a lock so regenerating keeps the progression
  - Per-part locks (keep drums, regenerate bass, etc.) and per-part **mute / solo** on the piano roll
  - Tempo (60-180 bpm, defaults follow the genre) and grid / quantization
  - Style/prompt text input; **Plan** fills sections/chords (editable) then **Generate MIDI** realises them
  - **MIDI import**: file chooser, drag-drop, or host MIDI capture. Detects key/scale/tempo, per-bar chords (editable in the lock field) and part roles
  - Import modes: **Accompany** (keep the user's part, write the rest + drums), **Extend** (continue N bars with song form), **Rewrite** (same chords, new voicings/melody/bass), **Restyle** (same harmony/melody in another genre groove)
  - **Style profiles**: learn chord vocabulary, voicings, rhythm grids, bass approaches, swing and melody intervals from several `.mid` files (or a folder). Named JSON profiles live in user app data; a **Blend** slider mixes them with the selected genre (arranger, LLM plan, and import modes)
  - **Factory presets** (~10 across pop, lo-fi, house, trap, neo-soul, cinematic, song form, locked jazz changes). Save your own; the host restores full generator state plus the last clip via `getStateInformation`
  - Settings: OpenAI or **Groq** preset (or a custom OpenAI-compatible URL), API key, model, last/session token counts, offline license key
  
- **Preview & Export**:
  - Built-in preview synth with per-part patches: 2–3 oscillators (supersaw unison, sub, noise, FM tine), ADSR amp + filter envelopes, resonant LP/HP/BP, pitch envelope, LFO (vibrato / filter / PWM), glide, then chorus, tempo-synced stereo delay, reverb, tape saturation, EQ, glue compressor and limiter. Genre-aware defaults layer **CC0 FreePats multisamples** (upright piano, DX EP/Rhodes, strings, organ, nylon pluck) with synth for supersaw / house stabs / 808 / bass. Mix bus: per-part HPF/mud/air, bass mono below 120 Hz, Haas/width on the music bus, kick-triggered sidechain with a smoothed envelope, gentler per-bus comps (about 2–4 dB GR) and master glue + oversampled-style true-peak limiter at −1 dBTP. Target loudness is about −10…−12 LUFS (lo-fi / R&B / cinematic −13…−14). Drums stay on the CC0 sampled kits with hat round-robins, pedal chicks, and open-hat choke. Sample pack ~11 MB; licenses in `Samples/README.md`. `Tools/analyze_mix.sh` prints LUFS, true peak, LRA, band RMS and side RMS.
  - **Sound** tab: pick a part and a patch, then tweak A/D/S/R, cutoff, resonance, drive, reverb and delay sends, plus Mix preset / sidechain / comp. Saved with host state and factory/user presets (genre defaults).
  - **Drag a part (or All parts) out of the plugin as `.mid`** into a DAW; also **Export MIDI** for multi-track, per-part files, or a single part
  - MIDI output routing to DAW tracks
  
- **Non-Blocking Generation**: All generation happens off the audio thread for smooth performance

### Roadmap
- **AI Timbre/Patch Generation**: Generate synth patches and timbres using AI
- **More generator types**: Genetic algorithms, reinforcement learning
- **Licensing**: Signed / online license check (the current build is an offline `SYN-` stub + Trial badge)

## Design Decisions

### Plugin Architecture: Instrument vs MIDI Effect

This plugin is implemented as a **software instrument** (synth) rather than a pure MIDI effect, for the following reasons:

1. **FL Studio Compatibility**: FL Studio doesn't handle pure MIDI effect plugins in the same way as other DAWs. Instrument plugins provide better compatibility.

2. **Preview Synth**: Users can immediately hear generated MIDI without routing to another instrument, making iteration faster.

3. **MIDI Output**: The plugin still outputs MIDI, so you can:
   - Route MIDI to other instruments in your DAW
   - Export as `.mid` files for any use
   - Use the built-in preview synth

4. **Universal Workflow**: Works consistently in Ableton Live, FL Studio, and other DAWs that support VST3/AU instruments.

### Generator Architecture

The plugin uses a **pluggable generator interface** (`MidiGenerator` base class) that makes it easy to add new generation methods:

```cpp
class MidiGenerator {
    virtual juce::MidiBuffer generate(const MidiGenerationParams& params) = 0;
};
```

Current implementations:
- `ArrangementGenerator`: Genre-aware multi-part arranger (chords, bass, synth, piano, drums) using `GenreProfiles` and `MusicTheory`
- `MarkovGenerator`: Offline algorithmic generation using Markov chains for note selection
- `LLMGenerator`: Asks an OpenAI-compatible chat endpoint for an `ArrangementPlan` JSON object (never notes). `ArrangementPlan` validates/repairs the schema; missing keys fall back to a keyword heuristic.
- `ArrangementPlan`: Compact form/chords/energy/density/motif schema. The arranger realises MIDI from the plan.
- `MidiImporter`: Loads a `.mid` (or captured host MIDI), estimates key/scale/tempo/chords/part roles, and overlays the user's part for Accompany / Extend / Restyle.
- `StyleProfile`: Learns a named style from MIDI and blends it with a `GenreProfile` at generate time.

**Extension Point**: New generators can be added by:
1. Inheriting from `MidiGenerator`
2. Implementing the `generate()` method
3. Registering in `PluginProcessor::setGeneratorType()`

For AI timbre generation, the same pattern applies: create a `TimbrerGenerator` interface with similar pluggability.

## Building

### Prerequisites

- CMake 3.15 or higher
- C++17 compatible compiler
- Git (for JUCE submodule)

CMake always builds **VST3 + Standalone**. On macOS it also builds **AU**. There is no required CI; build locally with the commands below.

### Windows (VST3 + Standalone)

Visual Studio 2019 or later (x64). From a Developer Command Prompt or PowerShell:

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target SynthesiaAI_VST3 SynthesiaAI_Standalone

# Plugin location
build/SynthesiaAI_artefacts/Release/VST3/Synthesia AI.vst3
build/SynthesiaAI_artefacts/Release/Standalone/Synthesia AI.exe
```

Ninja also works: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`.

### macOS (VST3 + AU + Standalone)

Xcode command-line tools (or the Xcode generator):

```bash
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target SynthesiaAI_VST3 SynthesiaAI_AU SynthesiaAI_Standalone

# Plugin locations
build/SynthesiaAI_artefacts/Release/VST3/Synthesia AI.vst3
build/SynthesiaAI_artefacts/Release/AU/Synthesia AI.component
build/SynthesiaAI_artefacts/Release/Standalone/Synthesia AI.app
```

Universal binaries are optional; omit `-DCMAKE_OSX_ARCHITECTURES` for a native-only build.

### Linux (development)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build --config Release --target SynthesiaAI_VST3 SynthesiaAI_Standalone SynthesiaRender
```

Linux produces both Standalone and VST3 (`build/SynthesiaAI_artefacts/Release/VST3/`). AU is macOS-only.

### Offline render host (headless "DAW")

`SynthesiaRender` loads the plugin processor, generates a clip with the genre
arranger (`--generator genre`, default), Markov (`markov`), or the LLM planner
(`llm`), plays it through the built-in synth and bounces WAV (and optional
multi-track `.mid`). `--plan file.json` / `--plan-json` feed a compact arrangement
plan through the same arranger path. `--style` is the free-text brief; without an
API key the planner uses a keyword heuristic. `--import file.mid` analyses a clip
(key, chords, part roles) and `--import-mode accompany|extend|rewrite|restyle`
keeps or rewrites it through the arranger. `--style-profile file.json` and
`--style-blend 0..1` mix a learned style with the genre. `--preset <id>` loads a factory
(or user) preset; `--list-presets` prints ids. `--sidechain on|off` enables kick-bus
ducking (default on; house pumps hard, trap leaves the 808). `--part` selects chords, bass, synth,
piano, drums, or all five. `--split-midi` also writes one `.mid` per part.
It exits non-zero if the render is silent, clips, or plays a different number of
notes than the clip contains.

```bash
cmake --build build --target SynthesiaRender
build/SynthesiaRender_artefacts/Release/SynthesiaRender \
    --genre lofi --part all --key A --scale minor --bars 8 --seed 7 \
    --out demo.wav --midi demo.mid --split-midi

# Mix + per-part solos, song-form, locked chords, and LLM-plan demos
Tools/render_demos.sh build/demos

# Realise a hand-written plan (same path as the LLM JSON)
build/SynthesiaRender_artefacts/Release/SynthesiaRender \
    --generator llm --plan Tools/plans/synthwave-explode.json \
    --style "late-night synthwave, chorus should explode" \
    --out /tmp/synthwave.wav --midi /tmp/synthwave.mid

# MIDI import: accompany a piano melody in lo-fi
build/SynthesiaRender_artefacts/Release/SynthesiaRender \
    --import Tools/midi/piano-melody-8bar.mid --import-mode accompany \
    --genre lofi --out /tmp/accompany.wav

# Learn a style from a folder of MIDI, then generate at 100% blend
build/SynthesiaRender_artefacts/Release/SynthesiaRender \
    --write-style-fixtures Tools/midi/style
build/SynthesiaRender_artefacts/Release/SynthesiaRender \
    --learn-style Tools/midi/style --save-style-json Tools/midi/style/neo-soul.json
build/SynthesiaRender_artefacts/Release/SynthesiaRender \
    --genre pop --style-profile Tools/midi/style/neo-soul.json --style-blend 1 \
    --out /tmp/style.wav

# Factory preset (productized defaults)
build/SynthesiaRender_artefacts/Release/SynthesiaRender \
    --preset lofi-am-dusty --out /tmp/preset.wav --midi /tmp/preset.mid
```

## Installation

### Windows

1. Copy `SynthesiaAI.vst3` to:
   - `C:\Program Files\Common Files\VST3\`

2. Rescan plugins in your DAW

### macOS

1. Copy `SynthesiaAI.vst3` to:
   - `/Library/Audio/Plug-Ins/VST3/` (all users) or
   - `~/Library/Audio/Plug-Ins/VST3/` (current user)

2. Copy `SynthesiaAI.component` to:
   - `/Library/Audio/Plug-Ins/Components/` (all users) or
   - `~/Library/Audio/Plug-Ins/Components/` (current user)

3. Rescan plugins in your DAW

## Usage

### In Ableton Live

1. **Load the Plugin**:
   - Create a new MIDI track
   - Add "Synthesia AI" as an instrument
   
2. **Generate MIDI**:
   - Choose a genre (tempo and grid follow it) and which part to write
   - Choose your key and scale
   - Set length, tempo and quantization
   - For the LLM generator, enter a style prompt
   - Click "Generate MIDI"
   
3. **Use the Output**:
   - **Audition**: The built-in synth plays the generated MIDI immediately
   - **Record**: Enable recording on the track and play to capture MIDI
   - **Route**: Disable Synthesia AI's audio, create another instrument track, route MIDI to it
   - **Drag to Live**: after Generate, drag **All parts** or a single part chip onto a track / the Arrangement view (drops a `.mid`)
   - **Export**: Click **Export MIDI** for a multi-track file, separate files per part, or one part only

### In FL Studio

1. **Load the Plugin**:
   - Add "Synthesia AI" to the channel rack (as an instrument)
   
2. **Generate MIDI**:
   - Same as above: select parameters and click "Generate MIDI"
   
3. **Use the Output**:
   - **Audition**: Preview using the built-in synth
   - **Drag to piano roll**: drag a part chip (or All parts) out of the plugin onto the playlist / piano roll
   - **Export**: Click **Export MIDI**, then drag the file into FL Studio's piano roll if you prefer a chooser
   - **Alternative**: Route MIDI output to another channel (FL Studio 21.2+)

### Using the LLM planner

1. Click **Settings**
2. Pick **OpenAI** (default `https://api.openai.com/v1/chat/completions`, model `gpt-4o-mini`) or **Groq** (base `https://api.groq.com/openai/v1`, chat URL `.../chat/completions`, model `openai/gpt-oss-120b`). **Custom** leaves the URL and model as you type them.
3. Enter your API key. It is stored only in local user app data (`SynthesiaAI/settings.xml`) and is never written to logs. Host project state stores generator settings and the clip, not the key.
4. Save. Token usage from the last request and the session is shown in this dialog.
5. Select **LLM planner** from Engine on the Generate tab
6. Type a prompt, e.g. `late-night synthwave, chorus should explode`
7. Click **Plan** — the UI fills genre, tempo, form, length, and a chord chart you can edit
8. Click **Generate MIDI** — the genre engine writes notes in key from that plan

With no key, Plan still works: keywords in the prompt map to genre, energy and density.

### Quick start (users)

1. Load **Synthesia AI** as an instrument.
2. Pick a **Preset** (Pop Radio, Lo-fi Dust, Peak Hour House, Trap Drop, Neo-Soul Pocket, Cinematic Pads, Lo-fi Song, House Anthem, Jazz Changes, Pop Uplift) or set key / genre yourself.
3. Click **Generate MIDI**. Mute or solo parts on the piano roll.
4. Drag **All parts** (or Chords / Bass / Synth / Piano / Drums) into your DAW, or click **Export MIDI**.
5. Optional: **Plan** tab for an LLM/heuristic arrangement, **Import** to accompany a `.mid`, **Style** to learn from your MIDI.
6. The header shows a **TRIAL** badge until you enter a license key starting with `SYN-` (12+ characters) in Settings. That check is an offline stub and can be replaced later.

### Presets and host recall

Factory presets ship in the binary. **Save** writes a JSON preset to user app data (`SynthesiaAI/presets`). DAW save/restore (`getStateInformation` / `setStateInformation`) keeps generator type, UI params, mute/solo, style, license key, and the last generated clip. API keys stay on the local machine only.

### MIDI import

Drop a `.mid` onto the plugin, use **Import MIDI...**, or **Capture MIDI** from the host. Detected chords land in the Chords field (locked, editable) and work with Plan / Generate. Then pick a mode:

- **Accompany** — keep the imported melody/part, write bass, harmony and drums around the detected chords
- **Extend** — continue the idea out to the Length slider (song form)
- **Rewrite** — same chords, new melody, bass and voicings
- **Restyle** — keep melody/harmony, re-render drums and groove in the selected genre

### Style profiles

**Learn style...** takes several `.mid` files or a folder (drop works too). Named JSON profiles are stored in user app data. The **Style** selector and **Blend** slider mix them with the current genre: chord Markov, voicings/extensions, bass approaches, swing, rhythm grids and melody intervals. Blend 0% is the genre alone; 100% follows the learned tendencies. Plan, Generate and import modes all see the blend.

## Testing

The project includes comprehensive unit tests:

- `GeneratorTests`: Validates MIDI generation, length constraints, note validation
- `JSONParsingTests`: Tests JSON parsing, markdown code block handling, value validation
- `ScaleTests`: Verifies scale generation and music theory correctness

Run tests with:
```bash
cd build
ctest --output-on-failure
```

## CI/CD

GitHub Actions automatically builds and tests on:
- Windows (latest)
- macOS (latest)

Artifacts (VST3/AU binaries) are uploaded for each successful build.

## Project Structure

```
.
├── Source/
│   ├── PluginProcessor.{h,cpp}     # Main audio processor
│   ├── PluginEditor.{h,cpp}        # UI implementation
│   ├── SimpleSynth.{h,cpp}         # Preview synth (patches, FX, sample + synth drums)
│   ├── SynthPatch.{h,cpp}          # Per-part preview patches, mix presets, genre defaults
│   ├── DrumLibrary.{h,cpp}         # Embedded CC0 one-shots for the preview kit
│   ├── InstrumentLibrary.{h,cpp}   # CC0 piano/EP/strings/organ/pluck multisamples
│   ├── MidiExporter.{h,cpp}        # Multi-track / per-part MIDI export
│   ├── MidiImporter.{h,cpp}        # MIDI file / capture analysis and overlay modes
│   ├── StyleProfile.{h,cpp}        # Learn/save style JSON and blend with genre profiles
│   ├── PresetStore.{h,cpp}         # Factory + user generator presets
│   ├── LicenseStub.h               # Offline SYN- license check (swap later)
│   ├── Music/MusicTheory.{h,cpp}   # Scales, chords, voicings, chord-symbol parser
│   └── Generators/
│       ├── MidiGenerator.{h,cpp}   # Base generator interface
│       ├── ArrangementGenerator.*  # Genre-aware multi-part arranger
│       ├── ArrangementPlan.*       # LLM/heuristic JSON plan schema
│       ├── GenreProfiles.{h,cpp}   # Per-genre progressions and patterns
│       ├── MarkovGenerator.{h,cpp} # Algorithmic melody generator
│       └── LLMGenerator.{h,cpp}    # LLM planner (JSON plan only)
├── Tools/plans/                    # Example arrangement-plan JSON files
├── Tools/midi/                     # Small MIDI fixtures for import / style-learning demos
├── Samples/                        # CC0 preview drums + pitched instruments + licenses
├── Tests/                          # Unit tests
├── CMakeLists.txt                  # Build configuration
├── .github/workflows/build.yml     # CI configuration
└── README.md                       # This file
```

## Extension Points

### Adding New Generators

1. Create a new class inheriting from `MidiGenerator`
2. Implement `generate(const MidiGenerationParams&)`
3. Add to `GeneratorType` enum
4. Update `PluginProcessor::setGeneratorType()`

### AI Timbre/Patch Generation (Future)

The architecture is designed to support timbre generation:

1. Create `TimbrerGenerator` interface similar to `MidiGenerator`
2. Implement `generate(const TimbreParams&)` returning synth parameters
3. Add UI controls for timbre generation
4. Store generated patches alongside MIDI data

Suggested approach:
- Use LLM to generate synthesizer parameters as JSON
- Validate and map to VST3 parameter space
- Allow export as preset files (.vstpreset)

## License

[Specify your product license here.]

The in-plugin **TRIAL / LICENSED** badge is an offline stub (`LicenseStub`): a key that starts with `SYN-` and is at least 12 characters counts as licensed. Swap `LicenseStub::isLicensed` for a signed or online check when you ship.

## Contributing

Contributions welcome! Areas of interest:
- Additional generator algorithms
- UI/UX improvements
- More comprehensive tests
- Documentation improvements
