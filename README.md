# Synthesia AI

An AI-powered MIDI generation plugin for DAWs (Ableton Live, FL Studio, etc.). Built with JUCE, this VST3/AU plugin generates musical MIDI sequences using either offline algorithmic generation or AI/LLM-based generation.

## Features

### Current (MVP)
- **Three generators**:
  - **Genre engine** (default): offline arrangements with chord progressions, bass, synth lead/arp, piano and drums, each on its own MIDI channel. Genres: Pop, Lo-fi / Jazz, EDM / House, Trap / Hip-hop, R&B / Neo-soul, Cinematic. Drums use the General MIDI percussion map on channel 10.
  - **Markov Generator**: classic single-line melody (no internet required)
  - **LLM planner**: OpenAI-compatible endpoint (your key, configurable base URL + model) returns a compact JSON arrangement plan only — form, chords, energy/density, motif, parts, tempo/swing, genre. The genre engine realises every note, so nothing is out of key. Without a key, prompt keywords pick genre/energy offline.
  
- **UI Controls**:
  - Genre, part (All / Chords / Bass / Synth / Piano / Drums), key and scale
  - Form (loop or song: intro / verse / pre / chorus / drop / bridge / break / outro) and length (1-32 bars)
  - Chord input (symbols like `Am9 Fmaj9` or Roman numerals, `|` for bar groups) with a lock so regenerating keeps the progression
  - Per-part locks (keep drums, regenerate bass, etc.)
  - Tempo (60-180 bpm, defaults follow the genre) and grid / quantization
  - Style/prompt text input; **Plan** fills sections/chords (editable) then **Generate MIDI** realises them
  - Settings: API key, base URL, model, last/session token counts
  
- **Preview & Export**:
  - Built-in synth with a distinct timbre per part, plus per-genre drum kits built from CC0 one-shot samples (synthesized fallback if a slot is missing). See `Samples/README.md`.
  - Export multi-track `.mid`, separate files per part, or a single part
  - MIDI output routing to DAW tracks
  
- **Non-Blocking Generation**: All generation happens off the audio thread for smooth performance

### Roadmap
- **AI Timbre/Patch Generation**: Generate synth patches and timbres using AI
- **More generator types**: Genetic algorithms, reinforcement learning
- **Style presets**: Pre-configured generation styles
- **MIDI drag & drop**: Direct drag-drop into DAW piano roll

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

### Windows

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Run tests
cd build
ctest -C Release --output-on-failure

# Plugin location
build/SynthesiaAI_artefacts/Release/VST3/SynthesiaAI.vst3
```

### macOS

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Run tests
cd build
ctest -C Release --output-on-failure

# Plugin locations
build/SynthesiaAI_artefacts/Release/VST3/SynthesiaAI.vst3
build/SynthesiaAI_artefacts/Release/AU/SynthesiaAI.component
```

### Linux (for development/testing)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cd build && ctest --output-on-failure
```

Note: Linux builds produce only the Standalone version (no VST3/AU for DAW use on Linux).

### Offline render host (headless "DAW")

`SynthesiaRender` loads the plugin processor, generates a clip with the genre
arranger (`--generator genre`, default), Markov (`markov`), or the LLM planner
(`llm`), plays it through the built-in synth and bounces WAV (and optional
multi-track `.mid`). `--plan file.json` / `--plan-json` feed a compact arrangement
plan through the same arranger path. `--style` is the free-text brief; without an
API key the planner uses a keyword heuristic. `--part` selects chords, bass,
synth, piano, drums, or all five. `--split-midi` also writes one `.mid` per part.
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
   - **Export**: Click "Export MIDI..." for a multi-track file, separate files per part, or one part only, then drag into Live

### In FL Studio

1. **Load the Plugin**:
   - Add "Synthesia AI" to the channel rack (as an instrument)
   
2. **Generate MIDI**:
   - Same as above: select parameters and click "Generate MIDI"
   
3. **Use the Output**:
   - **Audition**: Preview using the built-in synth
   - **Export & Import**: Click "Export as .mid", then drag the file into FL Studio's piano roll
   - **Alternative**: Route MIDI output to another channel (FL Studio 21.2+)

### Using the LLM planner

1. Click **Settings**
2. Enter your OpenAI-compatible API key, base URL (default `https://api.openai.com/v1/chat/completions`) and model (default `gpt-4o-mini`)
3. Save. Token usage from the last request and the session is shown in this dialog.
4. Select **LLM (OpenAI API)** from Generator
5. Type a prompt, e.g. `late-night synthwave, chorus should explode`
6. Click **Plan** — the UI fills genre, tempo, form, length, and a chord chart you can edit
7. Click **Generate MIDI** — the genre engine writes notes in key from that plan

With no key, Plan still works: keywords in the prompt map to genre, energy and density.

**Note**: API keys are stored in plugin state and saved with your project. Never share project files containing API keys.

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
│   ├── SimpleSynth.{h,cpp}         # Preview synth (timbre per part, sample + synth drums)
│   ├── DrumLibrary.{h,cpp}         # Embedded CC0 one-shots for the preview kit
│   ├── MidiExporter.{h,cpp}        # Multi-track / per-part MIDI export
│   ├── Music/MusicTheory.{h,cpp}   # Scales, chords, voicings, chord-symbol parser
│   └── Generators/
│       ├── MidiGenerator.{h,cpp}   # Base generator interface
│       ├── ArrangementGenerator.*  # Genre-aware multi-part arranger
│       ├── ArrangementPlan.*       # LLM/heuristic JSON plan schema
│       ├── GenreProfiles.{h,cpp}   # Per-genre progressions and patterns
│       ├── MarkovGenerator.{h,cpp} # Algorithmic melody generator
│       └── LLMGenerator.{h,cpp}    # LLM planner (JSON plan only)
├── Tools/plans/                    # Example arrangement-plan JSON files
├── Samples/                        # CC0 preview drum one-shots + licenses
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

[Specify your license here]

## Contributing

Contributions welcome! Areas of interest:
- Additional generator algorithms
- UI/UX improvements
- More comprehensive tests
- Documentation improvements
