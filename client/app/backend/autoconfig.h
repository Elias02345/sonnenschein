#pragma once

#include <QString>
#include "settings/streamingpreferences.h"

// Sonnenschein device-profile auto-configuration.
//
// Detects the local device's display and hardware-decode capabilities and
// derives an optimal streaming profile (resolution, refresh rate, codec,
// HDR, bitrate). This is the client-side half of Sonnenschein's "zero manual
// configuration" goal: on first connect to a host the client picks perfect
// settings for the device it runs on (Steam Deck, TV box, desktop, ...).
//
// Detection is fully local and needs no host connection, so it is verifiable
// headless via `sonnenschein-client detect-profile`.
class AutoConfig
{
public:
    struct DeviceProfile
    {
        // Chosen stream geometry.
        int width = 0;
        int height = 0;
        int fps = 0;

        // Recommended encode bitrate (kbps) for the geometry above.
        int bitrateKbps = 0;

        // HDR is offered when the display reports HDR and a 10-bit codec can
        // be hardware-decoded. Actual use still depends on host capabilities.
        bool hdr = false;

        // Best codec the local hardware can decode (informational — stream
        // negotiation ultimately picks the best codec both ends support).
        StreamingPreferences::VideoCodecConfig bestCodec = StreamingPreferences::VCC_AUTO;
        QString bestCodecName = QStringLiteral("H.264");

        // Raw capability flags from the hardware-decode probe.
        bool hwAccel = false;
        bool av1 = false;
        bool av1Main10 = false;
        bool hevc = false;
        bool hevcMain10 = false;
        bool h264 = false;

        // Native display info.
        int nativeWidth = 0;
        int nativeHeight = 0;
        int nativeRefreshRate = 0;
        bool displaySupportsHdr = false;

        bool valid = false;
    };

    // Detect the profile. Initializes/tears down its own SDL video subsystem
    // and test window, so it is safe to call before the GUI starts.
    static DeviceProfile detectProfile();

    // Apply a detected profile to a StreamingPreferences instance (resolution,
    // fps, bitrate, HDR). Codec is left on VCC_AUTO so stream negotiation can
    // pick the best codec both client and host support.
    static void applyToPreferences(const DeviceProfile& profile, StreamingPreferences* prefs);

    // Render a profile as a human/machine-readable JSON object.
    static QString toJson(const DeviceProfile& profile);
};
