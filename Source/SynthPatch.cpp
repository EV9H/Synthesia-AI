#include "SynthPatch.h"

namespace
{
    SynthPatch make(const juce::String& id, const juce::String& name, Part part)
    {
        SynthPatch p;
        p.id = id;
        p.name = name;
        p.part = part;
        return p;
    }

    const char* waveName(OscWave w)
    {
        switch (w)
        {
            case OscWave::Sine:     return "sine";
            case OscWave::Saw:      return "saw";
            case OscWave::Square:   return "square";
            case OscWave::Triangle: return "triangle";
            case OscWave::Pulse:    return "pulse";
            case OscWave::Noise:    return "noise";
            case OscWave::Tine:     return "tine";
        }
        return "saw";
    }

    OscWave waveFromName(const juce::String& n)
    {
        if (n == "sine") return OscWave::Sine;
        if (n == "square") return OscWave::Square;
        if (n == "triangle") return OscWave::Triangle;
        if (n == "pulse") return OscWave::Pulse;
        if (n == "noise") return OscWave::Noise;
        if (n == "tine") return OscWave::Tine;
        return OscWave::Saw;
    }

    void writeOsc(juce::XmlElement& xml, const char* tag, const OscSettings& o)
    {
        auto* e = xml.createNewChildElement(tag);
        e->setAttribute("wave", waveName(o.wave));
        e->setAttribute("mix", o.mix);
        e->setAttribute("detune", o.detuneCents);
        e->setAttribute("octave", o.octave);
        e->setAttribute("unison", o.unison);
        e->setAttribute("unisonDetune", o.unisonDetune);
        e->setAttribute("pw", o.pulseWidth);
        e->setAttribute("fmRatio", o.fmRatio);
        e->setAttribute("fmIndex", o.fmIndex);
    }

    OscSettings readOsc(const juce::XmlElement& xml, const char* tag, OscSettings fallback)
    {
        if (auto* e = xml.getChildByName(tag))
        {
            fallback.wave = waveFromName(e->getStringAttribute("wave"));
            fallback.mix = (float) e->getDoubleAttribute("mix", fallback.mix);
            fallback.detuneCents = (float) e->getDoubleAttribute("detune", fallback.detuneCents);
            fallback.octave = e->getIntAttribute("octave", fallback.octave);
            fallback.unison = e->getIntAttribute("unison", fallback.unison);
            fallback.unisonDetune = (float) e->getDoubleAttribute("unisonDetune", fallback.unisonDetune);
            fallback.pulseWidth = (float) e->getDoubleAttribute("pw", fallback.pulseWidth);
            fallback.fmRatio = (float) e->getDoubleAttribute("fmRatio", fallback.fmRatio);
            fallback.fmIndex = (float) e->getDoubleAttribute("fmIndex", fallback.fmIndex);
        }
        return fallback;
    }
}

void SynthPatch::writeXml(juce::XmlElement& xml) const
{
    xml.setAttribute("id", id);
    xml.setAttribute("name", name);
    xml.setAttribute("part", static_cast<int>(part));
    writeOsc(xml, "osc1", osc1);
    writeOsc(xml, "osc2", osc2);
    xml.setAttribute("subMix", subMix);
    xml.setAttribute("subOctave", subOctave);
    xml.setAttribute("noiseMix", noiseMix);
    xml.setAttribute("tineClick", tineClick);
    xml.setAttribute("ampA", amp.attack);
    xml.setAttribute("ampD", amp.decay);
    xml.setAttribute("ampS", amp.sustain);
    xml.setAttribute("ampR", amp.release);
    xml.setAttribute("fltA", filterEnv.attack);
    xml.setAttribute("fltD", filterEnv.decay);
    xml.setAttribute("fltS", filterEnv.sustain);
    xml.setAttribute("fltR", filterEnv.release);
    xml.setAttribute("filterMode", static_cast<int>(filterMode));
    xml.setAttribute("cutoff", cutoff);
    xml.setAttribute("resonance", resonance);
    xml.setAttribute("filterEnvAmount", filterEnvAmount);
    xml.setAttribute("keyTrack", keyTrack);
    xml.setAttribute("velToCutoff", velToCutoff);
    xml.setAttribute("pitchEnvAmount", pitchEnvAmount);
    xml.setAttribute("pitchEnvDecay", pitchEnvDecay);
    xml.setAttribute("lfoRate", lfoRate);
    xml.setAttribute("lfoDelay", lfoDelay);
    xml.setAttribute("lfoToPitch", lfoToPitch);
    xml.setAttribute("lfoToFilter", lfoToFilter);
    xml.setAttribute("lfoToPwm", lfoToPwm);
    xml.setAttribute("glide", glideSeconds);
    xml.setAttribute("drive", drive);
    xml.setAttribute("chorusSend", chorusSend);
    xml.setAttribute("delaySend", delaySend);
    xml.setAttribute("reverbSend", reverbSend);
    xml.setAttribute("pan", pan);
    xml.setAttribute("gain", gain);
    xml.setAttribute("wowFlutter", wowFlutter);
    xml.setAttribute("tapeSat", tapeSat);
    xml.setAttribute("vinylAmount", vinylAmount);
}

SynthPatch SynthPatch::fromXml(const juce::XmlElement& xml)
{
    SynthPatch p;
    p.id = xml.getStringAttribute("id");
    p.name = xml.getStringAttribute("name", p.id);
    p.part = static_cast<Part>(juce::jlimit(1, 4, xml.getIntAttribute("part", 1)));
    p.osc1 = readOsc(xml, "osc1", p.osc1);
    p.osc2 = readOsc(xml, "osc2", p.osc2);
    p.subMix = (float) xml.getDoubleAttribute("subMix", p.subMix);
    p.subOctave = xml.getIntAttribute("subOctave", p.subOctave);
    p.noiseMix = (float) xml.getDoubleAttribute("noiseMix", p.noiseMix);
    p.tineClick = (float) xml.getDoubleAttribute("tineClick", p.tineClick);
    p.amp.attack = (float) xml.getDoubleAttribute("ampA", p.amp.attack);
    p.amp.decay = (float) xml.getDoubleAttribute("ampD", p.amp.decay);
    p.amp.sustain = (float) xml.getDoubleAttribute("ampS", p.amp.sustain);
    p.amp.release = (float) xml.getDoubleAttribute("ampR", p.amp.release);
    p.filterEnv.attack = (float) xml.getDoubleAttribute("fltA", p.filterEnv.attack);
    p.filterEnv.decay = (float) xml.getDoubleAttribute("fltD", p.filterEnv.decay);
    p.filterEnv.sustain = (float) xml.getDoubleAttribute("fltS", p.filterEnv.sustain);
    p.filterEnv.release = (float) xml.getDoubleAttribute("fltR", p.filterEnv.release);
    p.filterMode = static_cast<FilterMode>(xml.getIntAttribute("filterMode", 1));
    p.cutoff = (float) xml.getDoubleAttribute("cutoff", p.cutoff);
    p.resonance = (float) xml.getDoubleAttribute("resonance", p.resonance);
    p.filterEnvAmount = (float) xml.getDoubleAttribute("filterEnvAmount", p.filterEnvAmount);
    p.keyTrack = (float) xml.getDoubleAttribute("keyTrack", p.keyTrack);
    p.velToCutoff = (float) xml.getDoubleAttribute("velToCutoff", p.velToCutoff);
    p.pitchEnvAmount = (float) xml.getDoubleAttribute("pitchEnvAmount", p.pitchEnvAmount);
    p.pitchEnvDecay = (float) xml.getDoubleAttribute("pitchEnvDecay", p.pitchEnvDecay);
    p.lfoRate = (float) xml.getDoubleAttribute("lfoRate", p.lfoRate);
    p.lfoDelay = (float) xml.getDoubleAttribute("lfoDelay", p.lfoDelay);
    p.lfoToPitch = (float) xml.getDoubleAttribute("lfoToPitch", p.lfoToPitch);
    p.lfoToFilter = (float) xml.getDoubleAttribute("lfoToFilter", p.lfoToFilter);
    p.lfoToPwm = (float) xml.getDoubleAttribute("lfoToPwm", p.lfoToPwm);
    p.glideSeconds = (float) xml.getDoubleAttribute("glide", p.glideSeconds);
    p.drive = (float) xml.getDoubleAttribute("drive", p.drive);
    p.chorusSend = (float) xml.getDoubleAttribute("chorusSend", p.chorusSend);
    p.delaySend = (float) xml.getDoubleAttribute("delaySend", p.delaySend);
    p.reverbSend = (float) xml.getDoubleAttribute("reverbSend", p.reverbSend);
    p.pan = (float) xml.getDoubleAttribute("pan", p.pan);
    p.gain = (float) xml.getDoubleAttribute("gain", p.gain);
    p.wowFlutter = (float) xml.getDoubleAttribute("wowFlutter", p.wowFlutter);
    p.tapeSat = (float) xml.getDoubleAttribute("tapeSat", p.tapeSat);
    p.vinylAmount = (float) xml.getDoubleAttribute("vinylAmount", p.vinylAmount);
    return p;
}

int PatchLibrary::pitchedIndex(Part part)
{
    switch (part)
    {
        case Part::Chords: return 0;
        case Part::Bass:   return 1;
        case Part::Synth:  return 2;
        case Part::Piano:  return 3;
        default:           return 0;
    }
}

juce::Array<SynthPatch> PatchLibrary::all()
{
    juce::Array<SynthPatch> patches;

    {
        auto p = make("lofi-rhodes", "Lo-fi Rhodes", Part::Chords);
        p.osc1 = { OscWave::Tine, 1.0f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 2.4f };
        p.osc2 = { OscWave::Sine, 0.28f, 6.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.noiseMix = 0.04f;
        p.tineClick = 0.22f;
        p.amp = { 0.008f, 1.4f, 0.55f, 0.45f };
        p.filterEnv = { 0.004f, 0.6f, 0.35f, 0.4f };
        p.cutoff = 2800.0f; p.resonance = 0.18f; p.filterEnvAmount = 1.0f;
        p.lfoRate = 0.35f; p.lfoToPitch = 6.0f; p.wowFlutter = 0.32f;
        p.chorusSend = 0.42f; p.delaySend = 0.10f; p.reverbSend = 0.24f;
        p.tapeSat = 0.22f; p.vinylAmount = 0.22f; p.drive = 0.16f; p.gain = 0.68f;
        patches.add(p);
    }
    {
        auto p = make("lofi-ep", "Dusty EP", Part::Piano);
        p.osc1 = { OscWave::Tine, 1.0f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 1.8f };
        p.osc2 = { OscWave::Triangle, 0.2f, 4.0f, 1, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.tineClick = 0.18f;
        p.amp = { 0.004f, 1.1f, 0.4f, 0.35f };
        p.filterEnv = { 0.002f, 0.5f, 0.2f, 0.3f };
        p.cutoff = 3400.0f; p.resonance = 0.16f; p.filterEnvAmount = 1.3f;
        p.wowFlutter = 0.22f; p.chorusSend = 0.28f; p.reverbSend = 0.20f;
        p.tapeSat = 0.18f; p.vinylAmount = 0.16f; p.drive = 0.12f; p.gain = 0.66f;
        patches.add(p);
    }
    {
        auto p = make("lofi-round-bass", "Lo-fi Round Bass", Part::Bass);
        p.osc1 = { OscWave::Sine, 1.0f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Triangle, 0.25f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.subMix = 0.45f; p.subOctave = -1;
        p.amp = { 0.008f, 0.22f, 0.8f, 0.12f };
        p.filterEnv = { 0.004f, 0.16f, 0.4f, 0.1f };
        p.cutoff = 420.0f; p.resonance = 0.28f; p.filterEnvAmount = 1.6f;
        p.glideSeconds = 0.04f; p.drive = 0.28f; p.tapeSat = 0.3f;
        p.reverbSend = 0.04f; p.gain = 0.85f;
        patches.add(p);
    }
    {
        auto p = make("lofi-mute-lead", "Muted Lo-fi Lead", Part::Synth);
        p.osc1 = { OscWave::Saw, 0.7f, 0.0f, 0, 3, 7.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Triangle, 0.4f, 8.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.amp = { 0.02f, 0.35f, 0.55f, 0.28f };
        p.filterEnv = { 0.01f, 0.28f, 0.3f, 0.22f };
        p.cutoff = 1100.0f; p.resonance = 0.4f; p.filterEnvAmount = 1.4f;
        p.lfoRate = 5.2f; p.lfoDelay = 0.3f; p.lfoToPitch = 6.0f;
        p.chorusSend = 0.28f; p.delaySend = 0.18f; p.reverbSend = 0.2f;
        p.wowFlutter = 0.2f; p.gain = 0.55f;
        patches.add(p);
    }

    {
        auto p = make("house-stab", "House Stab", Part::Chords);
        p.osc1 = { OscWave::Saw, 0.85f, 0.0f, 0, 5, 12.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Square, 0.35f, 7.0f, 0, 1, 0.0f, 0.42f, 1.0f, 0.0f };
        p.amp = { 0.004f, 0.22f, 0.45f, 0.18f };
        p.filterEnv = { 0.001f, 0.14f, 0.15f, 0.12f };
        p.cutoff = 3200.0f; p.resonance = 0.38f; p.filterEnvAmount = 2.6f; p.velToCutoff = 0.5f;
        p.chorusSend = 0.28f; p.delaySend = 0.08f; p.reverbSend = 0.14f;
        p.drive = 0.24f; p.gain = 0.58f;
        patches.add(p);
    }
    {
        auto p = make("house-organ", "House Organ", Part::Piano);
        p.osc1 = { OscWave::Square, 0.7f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Sine, 0.55f, 0.0f, 1, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.subMix = 0.2f;
        p.amp = { 0.002f, 0.08f, 0.85f, 0.08f };
        p.filterMode = FilterMode::LowPass; p.cutoff = 4200.0f; p.resonance = 0.12f;
        p.filterEnvAmount = 0.5f;
        p.chorusSend = 0.22f; p.reverbSend = 0.10f; p.drive = 0.16f; p.gain = 0.54f;
        patches.add(p);
    }
    {
        auto p = make("house-pluck-bass", "House Pluck Bass", Part::Bass);
        p.osc1 = { OscWave::Saw, 0.9f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Square, 0.25f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.subMix = 0.55f; p.subOctave = -1;
        p.amp = { 0.003f, 0.16f, 0.55f, 0.1f };
        p.filterEnv = { 0.001f, 0.09f, 0.12f, 0.08f };
        p.cutoff = 380.0f; p.resonance = 0.55f; p.filterEnvAmount = 3.2f; p.velToCutoff = 0.45f;
        p.glideSeconds = 0.02f; p.drive = 0.38f; p.gain = 0.8f;
        patches.add(p);
    }
    {
        auto p = make("house-supersaw", "Peak Supersaw", Part::Synth);
        p.osc1 = { OscWave::Saw, 1.0f, 0.0f, 0, 7, 14.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Saw, 0.35f, 12.0f, 0, 3, 8.0f, 0.5f, 1.0f, 0.0f };
        p.amp = { 0.018f, 0.24f, 0.72f, 0.20f };
        p.filterEnv = { 0.025f, 0.30f, 0.42f, 0.22f };
        p.cutoff = 3800.0f; p.resonance = 0.28f; p.filterEnvAmount = 2.0f;
        p.lfoRate = 0.28f; p.lfoToFilter = 0.22f;
        p.chorusSend = 0.48f; p.delaySend = 0.18f; p.reverbSend = 0.24f;
        p.drive = 0.14f; p.gain = 0.44f;
        patches.add(p);
    }

    {
        auto p = make("trap-dark-pad", "Trap Dark Pad", Part::Chords);
        p.osc1 = { OscWave::Saw, 0.8f, 0.0f, 0, 5, 10.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Triangle, 0.45f, 10.0f, -1, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.amp = { 0.10f, 0.7f, 0.72f, 0.55f };
        p.filterEnv = { 0.06f, 0.8f, 0.42f, 0.45f };
        p.cutoff = 1100.0f; p.resonance = 0.32f; p.filterEnvAmount = 1.8f;
        p.chorusSend = 0.42f; p.delaySend = 0.14f; p.reverbSend = 0.36f;
        p.drive = 0.14f; p.gain = 0.50f;
        patches.add(p);
    }
    {
        auto p = make("trap-808", "Trap 808", Part::Bass);
        p.osc1 = { OscWave::Sine, 1.0f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Sine, 0.18f, 0.0f, 1, 1, 0.0f, 0.5f, 2.0f, 0.4f };
        p.subMix = 0.62f; p.subOctave = -1;
        p.noiseMix = 0.04f;
        p.amp = { 0.002f, 0.42f, 0.88f, 0.22f };
        p.filterMode = FilterMode::LowPass; p.cutoff = 220.0f; p.resonance = 0.12f;
        p.filterEnvAmount = 1.1f; p.filterEnv = { 0.001f, 0.08f, 0.35f, 0.08f };
        p.pitchEnvAmount = 14.0f; p.pitchEnvDecay = 0.055f;
        p.glideSeconds = 0.09f; p.drive = 0.32f; p.tapeSat = 0.12f;
        p.reverbSend = 0.0f; p.gain = 0.90f;
        patches.add(p);
    }
    {
        auto p = make("trap-bell", "Trap Bell", Part::Synth);
        p.osc1 = { OscWave::Tine, 0.9f, 0.0f, 0, 1, 0.0f, 0.5f, 2.0f, 3.2f };
        p.osc2 = { OscWave::Sine, 0.4f, 0.0f, 1, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.tineClick = 0.12f; p.noiseMix = 0.03f;
        p.amp = { 0.002f, 0.9f, 0.15f, 0.4f };
        p.filterEnv = { 0.001f, 0.35f, 0.1f, 0.3f };
        p.filterMode = FilterMode::LowPass; p.cutoff = 5600.0f; p.resonance = 0.18f;
        p.filterEnvAmount = 1.4f;
        p.delaySend = 0.32f; p.reverbSend = 0.34f; p.chorusSend = 0.18f;
        p.gain = 0.46f;
        patches.add(p);
    }
    {
        auto p = make("trap-pluck", "Trap Pluck", Part::Piano);
        p.osc1 = { OscWave::Saw, 0.55f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Sine, 0.7f, 0.0f, 1, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.amp = { 0.002f, 0.18f, 0.05f, 0.16f };
        p.filterEnv = { 0.001f, 0.1f, 0.05f, 0.1f };
        p.filterMode = FilterMode::HighPass; p.cutoff = 380.0f; p.resonance = 0.22f;
        p.filterEnvAmount = 0.55f;
        p.delaySend = 0.18f; p.reverbSend = 0.16f; p.chorusSend = 0.12f; p.gain = 0.48f;
        patches.add(p);
    }

    {
        auto p = make("rnb-warm-ep", "Warm EP", Part::Chords);
        p.osc1 = { OscWave::Tine, 1.0f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 2.0f };
        p.osc2 = { OscWave::Sine, 0.35f, 3.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.tineClick = 0.14f;
        p.amp = { 0.01f, 1.2f, 0.62f, 0.4f };
        p.filterEnv = { 0.006f, 0.7f, 0.4f, 0.35f };
        p.cutoff = 3100.0f; p.resonance = 0.15f; p.filterEnvAmount = 1.1f;
        p.chorusSend = 0.46f; p.delaySend = 0.12f; p.reverbSend = 0.26f;
        p.drive = 0.12f; p.gain = 0.64f;
        patches.add(p);
    }
    {
        auto p = make("rnb-ep", "Soft EP", Part::Piano);
        p.osc1 = { OscWave::Tine, 0.95f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 1.6f };
        p.osc2 = { OscWave::Triangle, 0.22f, 5.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.tineClick = 0.1f;
        p.amp = { 0.006f, 1.0f, 0.5f, 0.38f };
        p.cutoff = 3600.0f; p.resonance = 0.14f; p.filterEnvAmount = 1.0f;
        p.chorusSend = 0.36f; p.reverbSend = 0.24f; p.gain = 0.62f;
        patches.add(p);
    }
    {
        auto p = make("rnb-round-bass", "R&B Round Bass", Part::Bass);
        p.osc1 = { OscWave::Sine, 0.9f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Saw, 0.18f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.subMix = 0.5f; p.subOctave = -1;
        p.amp = { 0.01f, 0.28f, 0.82f, 0.14f };
        p.filterEnv = { 0.006f, 0.2f, 0.45f, 0.12f };
        p.cutoff = 480.0f; p.resonance = 0.3f; p.filterEnvAmount = 1.8f;
        p.glideSeconds = 0.05f; p.drive = 0.26f; p.gain = 0.82f;
        patches.add(p);
    }
    {
        auto p = make("rnb-soft-pad", "Soft Pad", Part::Synth);
        p.osc1 = { OscWave::Saw, 0.6f, 0.0f, 0, 5, 9.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Triangle, 0.55f, 8.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.amp = { 0.18f, 0.7f, 0.78f, 0.7f };
        p.filterEnv = { 0.2f, 0.8f, 0.55f, 0.6f };
        p.cutoff = 2100.0f; p.resonance = 0.18f; p.filterEnvAmount = 1.2f;
        p.lfoRate = 0.22f; p.lfoToFilter = 0.18f;
        p.chorusSend = 0.52f; p.delaySend = 0.16f; p.reverbSend = 0.36f;
        p.gain = 0.46f;
        patches.add(p);
    }

    {
        auto p = make("cine-strings", "Cinematic Strings", Part::Chords);
        p.osc1 = { OscWave::Saw, 0.85f, 0.0f, 0, 5, 11.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Saw, 0.5f, 8.0f, 0, 3, 7.0f, 0.5f, 1.0f, 0.0f };
        p.noiseMix = 0.03f;
        p.amp = { 0.35f, 1.1f, 0.85f, 1.1f };
        p.filterEnv = { 0.4f, 1.2f, 0.7f, 1.0f };
        p.cutoff = 2000.0f; p.resonance = 0.14f; p.filterEnvAmount = 1.5f; p.keyTrack = 0.5f;
        p.lfoRate = 0.16f; p.lfoToFilter = 0.12f; p.lfoToPitch = 3.0f;
        p.chorusSend = 0.48f; p.delaySend = 0.10f; p.reverbSend = 0.55f;
        p.drive = 0.06f; p.gain = 0.48f;
        patches.add(p);
    }
    {
        auto p = make("cine-piano", "Film Piano", Part::Piano);
        p.osc1 = { OscWave::Sine, 0.7f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Triangle, 0.45f, 4.0f, 1, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.noiseMix = 0.02f;
        p.amp = { 0.004f, 2.2f, 0.18f, 0.55f };
        p.filterEnv = { 0.002f, 1.4f, 0.15f, 0.4f };
        p.cutoff = 4200.0f; p.resonance = 0.10f; p.filterEnvAmount = 1.2f;
        p.reverbSend = 0.42f; p.delaySend = 0.08f; p.chorusSend = 0.10f;
        p.gain = 0.58f;
        patches.add(p);
    }
    {
        auto p = make("cine-sub", "Cinematic Sub", Part::Bass);
        p.osc1 = { OscWave::Sine, 1.0f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Triangle, 0.12f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.subMix = 0.65f; p.subOctave = -1;
        p.amp = { 0.05f, 0.4f, 0.9f, 0.5f };
        p.cutoff = 180.0f; p.resonance = 0.1f; p.filterEnvAmount = 0.6f;
        p.filterMode = FilterMode::LowPass;
        p.reverbSend = 0.08f; p.drive = 0.2f; p.gain = 0.88f;
        patches.add(p);
    }
    {
        auto p = make("cine-air", "Air Lead", Part::Synth);
        p.osc1 = { OscWave::Saw, 0.45f, 0.0f, 1, 3, 8.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Sine, 0.7f, 6.0f, 1, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.noiseMix = 0.08f;
        p.amp = { 0.22f, 0.8f, 0.7f, 0.85f };
        p.filterEnv = { 0.25f, 1.0f, 0.5f, 0.7f };
        p.filterMode = FilterMode::HighPass; p.cutoff = 650.0f; p.resonance = 0.16f;
        p.filterEnvAmount = 0.9f;
        p.lfoRate = 4.5f; p.lfoDelay = 0.4f; p.lfoToPitch = 8.0f;
        p.chorusSend = 0.36f; p.delaySend = 0.22f; p.reverbSend = 0.48f;
        p.gain = 0.42f;
        patches.add(p);
    }

    {
        auto p = make("pop-bright-keys", "Pop Bright Keys", Part::Chords);
        p.osc1 = { OscWave::Saw, 0.55f, 0.0f, 0, 3, 8.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Sine, 0.55f, 4.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.amp = { 0.006f, 0.45f, 0.62f, 0.28f };
        p.filterEnv = { 0.004f, 0.22f, 0.35f, 0.2f };
        p.cutoff = 4000.0f; p.resonance = 0.18f; p.filterEnvAmount = 1.8f;
        p.chorusSend = 0.26f; p.delaySend = 0.08f; p.reverbSend = 0.16f;
        p.drive = 0.10f; p.gain = 0.60f;
        patches.add(p);
    }
    {
        auto p = make("pop-piano", "Pop Piano", Part::Piano);
        p.osc1 = { OscWave::Sine, 0.75f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Triangle, 0.4f, 3.0f, 1, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.noiseMix = 0.025f;
        p.amp = { 0.002f, 1.6f, 0.12f, 0.32f };
        p.filterEnv = { 0.001f, 0.9f, 0.08f, 0.25f };
        p.cutoff = 5600.0f; p.resonance = 0.10f; p.filterEnvAmount = 1.5f; p.keyTrack = 0.45f;
        p.reverbSend = 0.14f; p.delaySend = 0.05f; p.gain = 0.64f;
        patches.add(p);
    }
    {
        auto p = make("pop-pluck-bass", "Pop Pluck Bass", Part::Bass);
        p.osc1 = { OscWave::Saw, 0.7f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Sine, 0.5f, 0.0f, 0, 1, 0.0f, 0.5f, 1.0f, 0.0f };
        p.subMix = 0.4f;
        p.amp = { 0.004f, 0.18f, 0.6f, 0.1f };
        p.filterEnv = { 0.002f, 0.1f, 0.2f, 0.08f };
        p.cutoff = 520.0f; p.resonance = 0.4f; p.filterEnvAmount = 2.4f;
        p.drive = 0.3f; p.gain = 0.8f;
        patches.add(p);
    }
    {
        auto p = make("pop-pluck-lead", "Pop Pluck", Part::Synth);
        p.osc1 = { OscWave::Saw, 0.5f, 0.0f, 0, 3, 8.0f, 0.5f, 1.0f, 0.0f };
        p.osc2 = { OscWave::Square, 0.35f, 6.0f, 1, 1, 0.0f, 0.38f, 1.0f, 0.0f };
        p.amp = { 0.003f, 0.16f, 0.12f, 0.14f };
        p.filterEnv = { 0.001f, 0.12f, 0.08f, 0.1f };
        p.cutoff = 2600.0f; p.resonance = 0.35f; p.filterEnvAmount = 2.0f;
        p.lfoToPwm = 0.15f; p.lfoRate = 0.7f;
        p.delaySend = 0.18f; p.reverbSend = 0.16f; p.chorusSend = 0.15f;
        p.gain = 0.55f;
        patches.add(p);
    }

    return patches;
}

SynthPatch PatchLibrary::find(const juce::String& id)
{
    for (const auto& p : all())
        if (p.id == id || p.name == id)
            return p;
    return {};
}

juce::StringArray PatchLibrary::idsFor(Part part)
{
    juce::StringArray ids;
    for (const auto& p : all())
        if (p.part == part)
            ids.add(p.id);
    return ids;
}

juce::StringArray PatchLibrary::namesFor(Part part)
{
    juce::StringArray names;
    for (const auto& p : all())
        if (p.part == part)
            names.add(p.name);
    return names;
}

SynthPatch PatchLibrary::defaultFor(Genre genre, Part part)
{
    const char* id = "pop-bright-keys";
    switch (part)
    {
        case Part::Chords:
            switch (genre)
            {
                case Genre::LoFiJazz:  id = "lofi-rhodes"; break;
                case Genre::House:     id = "house-stab"; break;
                case Genre::Trap:      id = "trap-dark-pad"; break;
                case Genre::RnB:       id = "rnb-warm-ep"; break;
                case Genre::Cinematic: id = "cine-strings"; break;
                default:               id = "pop-bright-keys"; break;
            }
            break;
        case Part::Bass:
            switch (genre)
            {
                case Genre::LoFiJazz:  id = "lofi-round-bass"; break;
                case Genre::House:     id = "house-pluck-bass"; break;
                case Genre::Trap:      id = "trap-808"; break;
                case Genre::RnB:       id = "rnb-round-bass"; break;
                case Genre::Cinematic: id = "cine-sub"; break;
                default:               id = "pop-pluck-bass"; break;
            }
            break;
        case Part::Synth:
            switch (genre)
            {
                case Genre::LoFiJazz:  id = "lofi-mute-lead"; break;
                case Genre::House:     id = "house-supersaw"; break;
                case Genre::Trap:      id = "trap-bell"; break;
                case Genre::RnB:       id = "rnb-soft-pad"; break;
                case Genre::Cinematic: id = "cine-air"; break;
                default:               id = "pop-pluck-lead"; break;
            }
            break;
        case Part::Piano:
            switch (genre)
            {
                case Genre::LoFiJazz:  id = "lofi-ep"; break;
                case Genre::House:     id = "house-organ"; break;
                case Genre::Trap:      id = "trap-pluck"; break;
                case Genre::RnB:       id = "rnb-ep"; break;
                case Genre::Cinematic: id = "cine-piano"; break;
                default:               id = "pop-piano"; break;
            }
            break;
        default:
            break;
    }
    auto found = find(id);
    if (found.id.isEmpty())
        found.part = part;
    return found;
}

namespace
{
    void writeBus(juce::XmlElement& xml, const char* prefix, const BusComp& b)
    {
        xml.setAttribute(juce::String(prefix) + "Comp", b.amount);
        xml.setAttribute(juce::String(prefix) + "Atk", b.attack);
        xml.setAttribute(juce::String(prefix) + "Rel", b.release);
        xml.setAttribute(juce::String(prefix) + "Ratio", b.ratio);
        xml.setAttribute(juce::String(prefix) + "Thresh", b.thresh);
    }

    BusComp readBus(const juce::XmlElement& xml, const char* prefix, BusComp fallback)
    {
        fallback.amount = (float) xml.getDoubleAttribute(juce::String(prefix) + "Comp", fallback.amount);
        fallback.attack = (float) xml.getDoubleAttribute(juce::String(prefix) + "Atk", fallback.attack);
        fallback.release = (float) xml.getDoubleAttribute(juce::String(prefix) + "Rel", fallback.release);
        fallback.ratio = (float) xml.getDoubleAttribute(juce::String(prefix) + "Ratio", fallback.ratio);
        fallback.thresh = (float) xml.getDoubleAttribute(juce::String(prefix) + "Thresh", fallback.thresh);
        return fallback;
    }

    BusComp bus(float amount, float attack, float release, float ratio, float thresh)
    {
        return { amount, attack, release, ratio, thresh };
    }
}

void MixSettings::writeXml(juce::XmlElement& xml) const
{
    xml.setAttribute("id", id);
    xml.setAttribute("name", name);
    xml.setAttribute("sidechain", sidechainAmount);
    xml.setAttribute("scAttack", sidechainAttack);
    xml.setAttribute("scRelease", sidechainRelease);
    xml.setAttribute("scBass", sidechainBass);
    xml.setAttribute("comp", compAmount);
    xml.setAttribute("outputGain", outputGain);
    xml.setAttribute("musicWidth", musicWidth);
    writeBus(xml, "drum", drum);
    writeBus(xml, "bass", bass);
    writeBus(xml, "music", music);
    writeBus(xml, "master", master);
}

MixSettings MixSettings::fromXml(const juce::XmlElement& xml)
{
    MixSettings m = MixLibrary::find(xml.getStringAttribute("id"));
    if (m.id.isEmpty())
        m = MixLibrary::defaultFor(Genre::Pop);
    m.name = xml.getStringAttribute("name", m.name);
    m.sidechainAmount = (float) xml.getDoubleAttribute("sidechain", m.sidechainAmount);
    m.sidechainAttack = (float) xml.getDoubleAttribute("scAttack", m.sidechainAttack);
    m.sidechainRelease = (float) xml.getDoubleAttribute("scRelease", m.sidechainRelease);
    m.sidechainBass = (float) xml.getDoubleAttribute("scBass", m.sidechainBass);
    m.compAmount = (float) xml.getDoubleAttribute("comp", m.compAmount);
    m.outputGain = (float) xml.getDoubleAttribute("outputGain", m.outputGain);
    m.musicWidth = (float) xml.getDoubleAttribute("musicWidth", m.musicWidth);
    m.drum = readBus(xml, "drum", m.drum);
    m.bass = readBus(xml, "bass", m.bass);
    m.music = readBus(xml, "music", m.music);
    m.master = readBus(xml, "master", m.master);
    return m;
}

juce::Array<MixSettings> MixLibrary::all()
{
    juce::Array<MixSettings> mixes;

    {
        MixSettings m;
        m.id = "house-pump";
        m.name = "House Pump";
        m.sidechainAmount = 0.72f;
        m.sidechainAttack = 0.012f;
        m.sidechainRelease = 0.16f;
        m.sidechainBass = 0.78f;
        m.compAmount = 0.52f;
        m.outputGain = 1.35f;
        m.musicWidth = 1.55f;
        m.drum = bus(0.32f, 0.016f, 0.10f, 2.6f, 0.30f);
        m.bass = bus(0.28f, 0.020f, 0.14f, 2.3f, 0.34f);
        m.music = bus(0.24f, 0.024f, 0.16f, 2.1f, 0.36f);
        m.master = bus(0.22f, 0.035f, 0.20f, 1.6f, 0.44f);
        mixes.add(m);
    }
    {
        MixSettings m;
        m.id = "trap-808";
        m.name = "Trap 808";
        m.sidechainAmount = 0.48f;
        m.sidechainAttack = 0.010f;
        m.sidechainRelease = 0.22f;
        m.sidechainBass = 0.10f;
        m.compAmount = 0.50f;
        m.outputGain = 1.28f;
        m.musicWidth = 1.48f;
        m.drum = bus(0.30f, 0.018f, 0.11f, 2.5f, 0.32f);
        m.bass = bus(0.20f, 0.028f, 0.20f, 1.8f, 0.38f);
        m.music = bus(0.24f, 0.026f, 0.18f, 2.2f, 0.36f);
        m.master = bus(0.20f, 0.040f, 0.22f, 1.5f, 0.44f);
        mixes.add(m);
    }
    {
        MixSettings m;
        m.id = "lofi-glue";
        m.name = "Lo-fi Glue";
        m.sidechainAmount = 0.16f;
        m.sidechainAttack = 0.018f;
        m.sidechainRelease = 0.28f;
        m.sidechainBass = 0.28f;
        m.compAmount = 0.40f;
        m.outputGain = 0.95f;
        m.musicWidth = 1.12f;
        m.drum = bus(0.22f, 0.024f, 0.18f, 2.0f, 0.36f);
        m.bass = bus(0.22f, 0.026f, 0.18f, 2.0f, 0.36f);
        m.music = bus(0.18f, 0.032f, 0.24f, 1.8f, 0.38f);
        m.master = bus(0.16f, 0.050f, 0.28f, 1.5f, 0.46f);
        mixes.add(m);
    }
    {
        MixSettings m;
        m.id = "rnb-smooth";
        m.name = "R&B Smooth";
        m.sidechainAmount = 0.32f;
        m.sidechainAttack = 0.014f;
        m.sidechainRelease = 0.24f;
        m.sidechainBass = 0.42f;
        m.compAmount = 0.46f;
        m.outputGain = 1.02f;
        m.musicWidth = 1.38f;
        m.drum = bus(0.26f, 0.020f, 0.15f, 2.2f, 0.34f);
        m.bass = bus(0.24f, 0.024f, 0.16f, 2.1f, 0.36f);
        m.music = bus(0.22f, 0.030f, 0.20f, 1.9f, 0.38f);
        m.master = bus(0.18f, 0.045f, 0.24f, 1.5f, 0.46f);
        mixes.add(m);
    }
    {
        MixSettings m;
        m.id = "cine-wide";
        m.name = "Cinematic Wide";
        m.sidechainAmount = 0.08f;
        m.sidechainAttack = 0.025f;
        m.sidechainRelease = 0.36f;
        m.sidechainBass = 0.16f;
        m.compAmount = 0.38f;
        m.outputGain = 0.92f;
        m.musicWidth = 1.62f;
        m.drum = bus(0.18f, 0.028f, 0.22f, 2.0f, 0.38f);
        m.bass = bus(0.18f, 0.032f, 0.26f, 1.7f, 0.38f);
        m.music = bus(0.16f, 0.036f, 0.30f, 1.6f, 0.40f);
        m.master = bus(0.14f, 0.055f, 0.30f, 1.4f, 0.48f);
        mixes.add(m);
    }
    {
        MixSettings m;
        m.id = "pop-radio";
        m.name = "Pop Radio";
        m.sidechainAmount = 0.28f;
        m.sidechainAttack = 0.012f;
        m.sidechainRelease = 0.18f;
        m.sidechainBass = 0.55f;
        m.compAmount = 0.48f;
        m.outputGain = 1.22f;
        m.musicWidth = 1.42f;
        m.drum = bus(0.28f, 0.018f, 0.12f, 2.4f, 0.32f);
        m.bass = bus(0.26f, 0.022f, 0.14f, 2.2f, 0.34f);
        m.music = bus(0.22f, 0.026f, 0.16f, 2.0f, 0.36f);
        m.master = bus(0.20f, 0.040f, 0.20f, 1.6f, 0.44f);
        mixes.add(m);
    }

    return mixes;
}

juce::StringArray MixLibrary::names()
{
    juce::StringArray n;
    for (const auto& m : all())
        n.add(m.name);
    return n;
}

juce::StringArray MixLibrary::ids()
{
    juce::StringArray n;
    for (const auto& m : all())
        n.add(m.id);
    return n;
}

MixSettings MixLibrary::find(const juce::String& id)
{
    for (const auto& m : all())
        if (m.id == id || m.name == id)
            return m;
    return {};
}

MixSettings MixLibrary::defaultFor(Genre genre)
{
    switch (genre)
    {
        case Genre::House:     return find("house-pump");
        case Genre::Trap:      return find("trap-808");
        case Genre::LoFiJazz:  return find("lofi-glue");
        case Genre::RnB:       return find("rnb-smooth");
        case Genre::Cinematic: return find("cine-wide");
        default:               return find("pop-radio");
    }
}
