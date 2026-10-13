#pragma once

#include "Generators/MidiGenerator.h"
#include <juce_core/juce_core.h>
#include <optional>

struct GeneratorPreset
{
    juce::String id;
    juce::String name;
    juce::String description;
    GeneratorType generator{GeneratorType::Genre};
    MidiGenerationParams params;
    bool factory{true};
};

class PresetStore
{
public:
    static juce::Array<GeneratorPreset> factoryPresets();
    static GeneratorPreset factoryByIndex(int index);
    static std::optional<GeneratorPreset> find(const juce::String& idOrName);

    static juce::File userDirectory();
    static juce::StringArray listUserIds();
    static juce::StringArray listAllIds();
    static bool saveUser(const juce::String& name, const MidiGenerationParams& params,
                         GeneratorType generator);
    static std::optional<GeneratorPreset> loadUser(const juce::String& id);

    static void writeParams(juce::XmlElement& xml, const MidiGenerationParams& params);
    static MidiGenerationParams readParams(const juce::XmlElement& xml);

    static juce::var paramsToVar(const MidiGenerationParams& params);
    static MidiGenerationParams paramsFromVar(const juce::var& value);

    static juce::String generatorName(GeneratorType type);
    static GeneratorType generatorFromName(const juce::String& name);

    static juce::String sanitiseId(const juce::String& name);
};
