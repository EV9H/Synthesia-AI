#pragma once

#include <juce_core/juce_core.h>

#ifndef HARMONIA_LICENSE_ENFORCE
#define HARMONIA_LICENSE_ENFORCE 0
#endif

// Offline + optional HTTP license stub. No Lemon secrets live in the binary.
// Shipping builds: cmake -DHARMONIA_LICENSE_ENFORCE=1
// Offline format: HRM-XXXXXXXX-CCCC  (8 hex payload + 4 hex checksum)
// Legacy: SYN- plus at least 8 more characters (pre-Harmonia stub keys).
// HTTP: POST {"license_key":"..."} to HARMONIA_LICENSE_VALIDATE_URL or the
// Settings URL. 200 + body containing "valid" or "licensed" counts as yes.
class LicenseStub
{
public:
    static constexpr const char* prefix = "HRM-";
    static constexpr const char* legacyPrefix = "SYN-";

    struct Result
    {
        bool licensed{false};
        juce::String message;
    };

    static bool enforceGenerate()
    {
        return HARMONIA_LICENSE_ENFORCE != 0;
    }

    static juce::String defaultValidateUrl()
    {
        return juce::SystemStats::getEnvironmentVariable("HARMONIA_LICENSE_VALIDATE_URL", {});
    }

    static juce::uint16 checksum(const juce::String& hex8)
    {
        juce::uint32 sum = 0xA5;
        for (int i = 0; i < hex8.length(); ++i)
        {
            const int nibble = juce::String::fromUTF8("0123456789ABCDEFabcdef")
                                   .indexOfChar(hex8[i]);
            if (nibble < 0)
                continue;
            sum = (sum * 7u + (juce::uint32) (nibble % 16)) & 0xffffu;
        }
        return (juce::uint16) sum;
    }

    static juce::String sanitiseKey(const juce::String& raw)
    {
        auto s = raw.trim();
        const juce::String prefixes[] = { "LICENSE KEY:", "LICENSE:", "KEY:" };
        for (const auto& p : prefixes)
            if (s.startsWithIgnoreCase(p))
                s = s.substring(p.length()).trim();
        s = s.removeCharacters(" \t\r\n");
        return s.toUpperCase();
    }

    static juce::String makeOfflineKey(const juce::String& hex8)
    {
        const auto payload = hex8.toUpperCase().paddedLeft('0', 8).substring(0, 8);
        return "HRM-" + payload + "-" + juce::String::toHexString((int) checksum(payload))
                                          .paddedLeft('0', 4).substring(0, 4).toUpperCase();
    }

    static bool offlineKeyValid(const juce::String& key)
    {
        const auto trimmed = sanitiseKey(key);
        if (trimmed.startsWith(legacyPrefix) && trimmed.length() >= 12)
            return true;
        if (!trimmed.startsWith(prefix) || trimmed.length() < 17)
            return false;
        // HRM-XXXXXXXX-CCCC
        const auto body = trimmed.substring(4);
        if (body.length() < 13 || body[8] != '-')
            return false;
        const auto payload = body.substring(0, 8);
        const auto got = body.substring(9, 13);
        const auto expect = juce::String::toHexString((int) checksum(payload))
                                .paddedLeft('0', 4).substring(0, 4).toUpperCase();
        return got == expect;
    }

    static bool isLicensed(const juce::String& key)
    {
        return offlineKeyValid(key);
    }

    static juce::String badgeText(const juce::String& key)
    {
        return isLicensed(key) ? "LICENSED" : "TRIAL";
    }

    // Offline first. HTTP only when a URL is supplied — no embedded secrets.
    static Result validate(const juce::String& key, const juce::String& urlIn = {})
    {
        const auto trimmed = sanitiseKey(key);
        if (trimmed.isEmpty())
            return { false, "Paste a license key (HRM-XXXXXXXX-CCCC)." };

        if (offlineKeyValid(trimmed))
            return { true, "Valid offline key. Generate is unlocked. Drag MIDI always works." };

        if (trimmed.startsWith(prefix))
            return { false, "HRM- key checksum failed. Paste the full HRM-XXXXXXXX-CCCC line from your receipt, then Validate." };

        auto url = urlIn.trim();
        if (url.isEmpty())
            url = defaultValidateUrl().trim();
        if (url.isEmpty())
            return { false, "Not a valid offline key. Set HARMONIA_LICENSE_VALIDATE_URL or a Lemon validate URL in Settings." };

        juce::URL request(url);
        const auto body = juce::String("{\"license_key\":\"") + trimmed.replace("\"", "") + "\"}";
        request = request.withPOSTData(body);
        juce::String headers = "Content-Type: application/json\r\n";
        int status = 0;
        auto stream = request.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                                    .withExtraHeaders(headers)
                                                    .withConnectionTimeoutMs(4000)
                                                    .withStatusCode(&status));
        if (stream == nullptr)
            return { false, "Lemon validate URL unreachable (placeholder). Offline key still required." };

        const auto response = stream->readEntireStreamAsString();
        const bool ok = status == 200
            && (response.containsIgnoreCase("\"licensed\":true")
                || response.containsIgnoreCase("\"valid\":true")
                || response.containsIgnoreCase("\"status\":\"valid\""));
        if (ok)
            return { true, "Validated against license URL." };
        return { false, "License URL rejected the key (HTTP " + juce::String(status) + ")." };
    }
};
