#pragma once

#include <string>

// All user-adjustable settings. Persisted as an .ini next to the plugin DLL.
struct Config
{
    // master switch
    bool enabled = true;

    // which data is pushed to Discord
    bool showStatus = true;       // Running / Paused / Loading
    bool showFile = true;         // target file name (privacy: turn off to hide what you debug)
    bool showArch = true;         // x64 / x32
    bool showModules = true;      // loaded module count
    bool showThreads = true;      // live thread count
    bool showLocation = true;     // module+offset at the current instruction while paused
    bool showBreakpoints = true;  // breakpoint hits this session
    bool showExceptions = true;   // exceptions this session
    bool showTimer = true;        // elapsed time
    bool showIcons = true;        // small status icon

    bool resetTimerPerTarget = true;

    // Discord application. The default works out of the box; set your own ID only if you want your own app name.
    std::string appId = "820011598121926736";
    std::string idleText = "Idling";

    // Images: either an asset key uploaded to your Discord application, or a direct https link to a PNG/JPG.
    // Defaults are open-source images (x64dbg logo, Twemoji icons) so no setup is needed.
    std::string largeImageKey = "https://raw.githubusercontent.com/x64dbg/x64dbg/development/src/bug.png";
    std::string smallRunningKey = "https://raw.githubusercontent.com/jdecked/twemoji/main/assets/72x72/25b6.png";
    std::string smallPausedKey = "https://raw.githubusercontent.com/jdecked/twemoji/main/assets/72x72/23f8.png";
    std::string smallIdleKey = "https://raw.githubusercontent.com/jdecked/twemoji/main/assets/72x72/1f4a4.png";
};

namespace config
{
    void init(void* moduleHandle);          // resolves the .ini path from the plugin DLL location
    const std::string& path();

    void load(Config& cfg);
    void save(const Config& cfg);
}
