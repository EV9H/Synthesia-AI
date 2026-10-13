# Synthesia AI

An AI-powered MIDI generation plugin for DAWs (Ableton Live, FL Studio, etc.). Built with JUCE, this VST3/AU plugin generates musical MIDI sequences using either offline algorithmic generation or AI/LLM-based generation.

## Features

### Current (MVP)
- **Dual Generator System**:
  - **Markov Generator**: Offline algorithmic MIDI generation using Markov chains (no internet required)
  - **LLM Generator**: AI-powered MIDI generation using OpenAI-compatible APIs
  
- **Rich UI Controls**:
  - Key and scale selection (Major, Minor, Dorian, Mixolydian, Pentatonic)
  - Length control (1-16 bars)
  - Tempo sync with quantization (1/16, 1/8, 1/4, 1/2 notes)
  - Style/prompt text input for AI generation
  
- **Preview & Export**:
  - Built-in simple synth for real-time audio preview
  - Export generated MIDI as `.mid` files
  - MIDI output routing to DAW tracks
  
- **Non-Blocking Generation**: All generation happens off the audio thread for smooth performance

### Roadmap
- **AI Timbre/Patch Generation**: Generate synth patches and timbres using AI
- **More generator types**: Genetic algorithms, reinforcement learning
- **Style presets**: Pre-configured generation styles
- **MIDI drag & drop**: Direct drag-drop into DAW piano roll
- **Multi-track generation**: Generate bass, melody, chords separately

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
- `MarkovGenerator`: Offline algorithmic generation using Markov chains for note selection
- `LLMGenerator`: Calls OpenAI-compatible APIs with structured prompts, parses JSON responses

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

`SynthesiaRender` loads the plugin processor, generates a clip with the Markov
generator, plays it through the built-in synth with a running host transport and
bounces it to WAV (and optionally `.mid`). It exits non-zero if the render is
silent, clips, or plays a different number of notes than the clip contains.

```bash
cmake --build build --target SynthesiaRender
build/SynthesiaRender_artefacts/Release/SynthesiaRender \
    --key A --scale minor --bars 8 --quant 1/16 --tempo 128 --seed 7 \
    --out demo.wav --midi demo.mid

# Render the demo set to MP3 + MIDI with level checks (needs ffmpeg)
Tools/render_demos.sh build/demos
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
   - Choose your key and scale
   - Set length and quantization
   - Enter a style prompt (e.g., "jazzy chords with walking bass")
   - Click "Generate MIDI"
   
3. **Use the Output**:
   - **Audition**: The built-in synth plays the generated MIDI immediately
   - **Record**: Enable recording on the track and play to capture MIDI
   - **Route**: Disable Synthesia AI's audio, create another instrument track, route MIDI to it
   - **Export**: Click "Export as .mid" to save the file, then drag into Live

### In FL Studio

1. **Load the Plugin**:
   - Add "Synthesia AI" to the channel rack (as an instrument)
   
2. **Generate MIDI**:
   - Same as above: select parameters and click "Generate MIDI"
   
3. **Use the Output**:
   - **Audition**: Preview using the built-in synth
   - **Export & Import**: Click "Export as .mid", then drag the file into FL Studio's piano roll
   - **Alternative**: Route MIDI output to another channel (FL Studio 21.2+)

### Using LLM Generator

1. Click the "Settings" button
2. Enter your OpenAI API key (or OpenAI-compatible endpoint key)
3. Click "Save"
4. Select "LLM (OpenAI API)" from the Generator dropdown
5. Generate MIDI as usual

The LLM generator will send structured prompts to the AI and parse the returned JSON for notes.

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
│   ├── SimpleSynth.{h,cpp}         # Preview synth
│   ├── MidiExporter.{h,cpp}        # MIDI file export
│   └── Generators/
│       ├── MidiGenerator.{h,cpp}   # Base generator interface
│       ├── MarkovGenerator.{h,cpp} # Algorithmic generator
│       └── LLMGenerator.{h,cpp}    # AI/LLM generator
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
