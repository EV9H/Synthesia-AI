#pragma once

#include <juce_core/juce_core.h>

// Offline license-key stub. Easy to swap for a signed/online check later:
// keep isLicensed() as the single gate used by the UI badge and README.
class LicenseStub
{
public:
    static constexpr const char* prefix = "SYN-";
    static constexpr int minLength = 12;

    static bool isLicensed(const juce::String& key)
    {
        const auto trimmed = key.trim();
        return trimmed.startsWith(prefix) && trimmed.length() >= minLength;
    }

    static juce::String badgeText(const juce::String& key)
    {
        return isLicensed(key) ? "LICENSED" : "TRIAL";
    }
};
