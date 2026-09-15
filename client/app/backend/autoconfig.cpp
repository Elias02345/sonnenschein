#include "autoconfig.h"

#include "streaming/session.h"
#include "streaming/streamutils.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <Limelight.h>
#include <SDL.h>

// Probe a single video format for hardware-decode support on the test window.
static bool probeHw(SDL_Window* window, int videoFormat, int w, int h, int fps)
{
    return Session::getDecoderAvailability(window,
                                           StreamingPreferences::VDS_AUTO,
                                           videoFormat, w, h, fps)
           == Session::DecoderAvailability::Hardware;
}

// Shared setup: open the video subsystem, create a throwaway test window,
// and read native display geometry into the profile. Returns the test
// window (caller must destroy it and quit the video subsystem), or nullptr
// on failure (profile left invalid).
static SDL_Window* detectDisplayGeometry(AutoConfig::DeviceProfile& p)
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "AutoConfig: SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return nullptr;
    }

    SDL_Window* testWindow = StreamUtils::createTestWindow();
    if (!testWindow) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "AutoConfig: failed to create test window: %s", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return nullptr;
    }

    // --- Native display geometry (display 0) --------------------------------
    SDL_DisplayMode desktopMode;
    SDL_Rect safeArea;
    if (StreamUtils::getNativeDesktopMode(0, &desktopMode, &safeArea)) {
        p.nativeWidth = desktopMode.w;
        p.nativeHeight = desktopMode.h;

        // Pick the highest refresh rate available at the native resolution.
        int bestRefresh = desktopMode.refresh_rate;
        int numModes = SDL_GetNumDisplayModes(0);
        for (int i = 0; i < numModes; i++) {
            SDL_DisplayMode mode;
            if (SDL_GetDisplayMode(0, i, &mode) == 0 &&
                mode.w == desktopMode.w && mode.h == desktopMode.h &&
                mode.refresh_rate > bestRefresh) {
                bestRefresh = mode.refresh_rate;
            }
        }
        // Normalize around standard rates (displays sometimes report 59/61).
        if (bestRefresh >= 58 && bestRefresh <= 62) {
            bestRefresh = 60;
        }
        else if (bestRefresh >= 28 && bestRefresh <= 32) {
            bestRefresh = 30;
        }
        p.nativeRefreshRate = bestRefresh;
    }

    // Chosen stream geometry defaults to the native display; fall back to a
    // sane 1080p60 if detection failed (e.g. headless test window).
    p.width = p.nativeWidth > 0 ? p.nativeWidth : 1920;
    p.height = p.nativeHeight > 0 ? p.nativeHeight : 1080;
    p.fps = p.nativeRefreshRate > 0 ? p.nativeRefreshRate : 60;

    return testWindow;
}

AutoConfig::DeviceProfile AutoConfig::detectProfile()
{
    DeviceProfile p;

    SDL_Window* testWindow = detectDisplayGeometry(p);
    if (!testWindow) {
        return p;
    }

    // --- General decoder capabilities (HW accel, HDR, max resolution) -------
    bool hasHwAccel = false;
    bool rendererAlwaysFullScreen = false;
    bool supportsHdr = false;
    QSize maxResolution(0, 0);
    Session::getDecoderInfo(testWindow, hasHwAccel, rendererAlwaysFullScreen, supportsHdr, maxResolution);
    p.hwAccel = hasHwAccel;
    p.displaySupportsHdr = supportsHdr;

    // Clamp the chosen geometry to what the decoder can handle.
    if (maxResolution.width() > 0 && maxResolution.height() > 0) {
        if (p.width > maxResolution.width() || p.height > maxResolution.height()) {
            p.width = maxResolution.width();
            p.height = maxResolution.height();
        }
    }

    // --- Per-codec hardware-decode probe (best first) -----------------------
    const int w = p.width, h = p.height, fps = p.fps;
    p.av1Main10 = probeHw(testWindow, VIDEO_FORMAT_AV1_MAIN10, w, h, fps);
    p.av1       = p.av1Main10 || probeHw(testWindow, VIDEO_FORMAT_AV1_MAIN8, w, h, fps);
    p.hevcMain10 = probeHw(testWindow, VIDEO_FORMAT_H265_MAIN10, w, h, fps);
    p.hevc      = p.hevcMain10 || probeHw(testWindow, VIDEO_FORMAT_H265, w, h, fps);
    p.h264      = probeHw(testWindow, VIDEO_FORMAT_H264, w, h, fps);

    if (p.av1) {
        p.bestCodec = StreamingPreferences::VCC_FORCE_AV1;
        p.bestCodecName = QStringLiteral("AV1");
    }
    else if (p.hevc) {
        p.bestCodec = StreamingPreferences::VCC_FORCE_HEVC;
        p.bestCodecName = QStringLiteral("HEVC");
    }
    else {
        p.bestCodec = StreamingPreferences::VCC_FORCE_H264;
        p.bestCodecName = QStringLiteral("H.264");
    }

    // HDR is offered only when the display reports HDR support and a 10-bit
    // codec can be hardware-decoded.
    p.hdr = supportsHdr && (p.av1Main10 || p.hevcMain10);

    // Recommended bitrate for the chosen geometry.
    p.bitrateKbps = StreamingPreferences::getDefaultBitrate(p.width, p.height, p.fps, false);

    SDL_DestroyWindow(testWindow);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    p.valid = true;
    return p;
}

AutoConfig::DeviceProfile AutoConfig::detectProfileCheap()
{
    // Easy Mode runs this at the top of Session::initialize(), i.e.
    // immediately before that same function does its own full hardware
    // decoder probe (creating/tearing down real VAAPI/EGL/DRM decoder
    // contexts) for the actual stream. Doing a SECOND full probe cycle here
    // — as detectProfile() does for the standalone `detect-profile`
    // diagnostic command — means back-to-back GPU/decoder context
    // teardown+recreate in the same process, right at the most sensitive
    // moment of stream startup. On a fresh CLI-launched process (e.g. every
    // Steam Deck / Decky stream launch) this is *the only* stream that
    // process will ever start, so it always pays the full cost right
    // before it matters most.
    //
    // This cheap variant only queries display geometry (resolution/refresh
    // rate) — no decoder is ever created. Codec choice stays on AUTO
    // (negotiated for real by Session::initialize() right after), and HDR
    // eligibility is left to the real negotiation rather than guessed here.
    DeviceProfile p;

    SDL_Window* testWindow = detectDisplayGeometry(p);
    if (!testWindow) {
        return p;
    }

    p.bitrateKbps = StreamingPreferences::getDefaultBitrate(p.width, p.height, p.fps, false);

    SDL_DestroyWindow(testWindow);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    p.valid = true;
    return p;
}

void AutoConfig::applyToPreferences(const DeviceProfile& profile, StreamingPreferences* prefs)
{
    if (!profile.valid || prefs == nullptr) {
        return;
    }
    prefs->width = profile.width;
    prefs->height = profile.height;
    prefs->fps = profile.fps;
    prefs->bitrateKbps = profile.bitrateKbps;
    prefs->enableHdr = profile.hdr;
    // Leave the codec on AUTO so stream negotiation picks the best codec that
    // BOTH the client (probed above) and the host support.
    prefs->videoCodecConfig = StreamingPreferences::VCC_AUTO;
}

void AutoConfig::applyEasyMode(StreamingPreferences* prefs)
{
    // Detection opens a short-lived SDL test window, so probe once per
    // process and reuse the result for every stream launch.
    static DeviceProfile s_CachedProfile;
    static bool s_ProbeDone = false;

    if (prefs == nullptr || prefs->settingsMode != StreamingPreferences::SM_EASY) {
        return;
    }

    if (!s_ProbeDone) {
        s_CachedProfile = detectProfileCheap();
        s_ProbeDone = true;
    }

    if (!s_CachedProfile.valid) {
        return;
    }

    DeviceProfile profile = s_CachedProfile;

    switch (prefs->easyQuality) {
    case StreamingPreferences::EQ_QUALITY:
        profile.bitrateKbps = profile.bitrateKbps * 4 / 3;
        break;
    case StreamingPreferences::EQ_SMOOTHNESS:
        // Lighter encode/decode load at full native refresh: cap the stream
        // geometry at 1080p while preserving the display's aspect ratio.
        if (profile.width > 1920) {
            profile.height = qMax(2, (profile.height * 1920 / profile.width) & ~1);
            profile.width = 1920;
            profile.bitrateKbps = StreamingPreferences::getDefaultBitrate(
                profile.width, profile.height, profile.fps, false);
        }
        break;
    case StreamingPreferences::EQ_AUTO:
    default:
        break;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Easy mode: applying auto profile %dx%d@%d, %d kbps, HDR %s (quality preset %d)",
                profile.width, profile.height, profile.fps, profile.bitrateKbps,
                profile.hdr ? "on" : "off", static_cast<int>(prefs->easyQuality));

    applyToPreferences(profile, prefs);
}

QString AutoConfig::toJson(const DeviceProfile& profile)
{
    QJsonObject root;
    root["valid"] = profile.valid;

    QJsonObject display;
    display["native_width"] = profile.nativeWidth;
    display["native_height"] = profile.nativeHeight;
    display["native_refresh_hz"] = profile.nativeRefreshRate;
    display["supports_hdr"] = profile.displaySupportsHdr;
    root["display"] = display;

    QJsonObject decode;
    decode["hardware_acceleration"] = profile.hwAccel;
    decode["h264"] = profile.h264;
    decode["hevc"] = profile.hevc;
    decode["hevc_main10"] = profile.hevcMain10;
    decode["av1"] = profile.av1;
    decode["av1_main10"] = profile.av1Main10;
    root["hw_decode"] = decode;

    QJsonObject rec;
    rec["width"] = profile.width;
    rec["height"] = profile.height;
    rec["fps"] = profile.fps;
    rec["bitrate_kbps"] = profile.bitrateKbps;
    rec["codec"] = profile.bestCodecName;
    rec["hdr"] = profile.hdr;
    root["recommended"] = rec;

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}
