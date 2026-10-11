#include "config.h"

#include <Windows.h>

namespace
{
    std::string gIniPath;

    bool readBool(const char* section, const char* key, bool def)
    {
        return GetPrivateProfileIntA(section, key, def ? 1 : 0, gIniPath.c_str()) != 0;
    }

    void writeBool(const char* section, const char* key, bool value)
    {
        WritePrivateProfileStringA(section, key, value ? "1" : "0", gIniPath.c_str());
    }

    std::string readString(const char* section, const char* key, const std::string& def)
    {
        char buf[512] = {};
        GetPrivateProfileStringA(section, key, def.c_str(), buf, sizeof(buf), gIniPath.c_str());
        return buf;
    }

    void writeString(const char* section, const char* key, const std::string& value)
    {
        WritePrivateProfileStringA(section, key, value.c_str(), gIniPath.c_str());
    }
}

namespace config
{
    void init(void* moduleHandle)
    {
        char dll[MAX_PATH] = {};
        GetModuleFileNameA(static_cast<HMODULE>(moduleHandle), dll, MAX_PATH);
        gIniPath = dll;
        const size_t dot = gIniPath.rfind('.');
        gIniPath = (dot == std::string::npos ? gIniPath : gIniPath.substr(0, dot)) + ".ini";
    }

    const std::string& path()
    {
        return gIniPath;
    }

    void load(Config& c)
    {
        c.enabled = readBool("General", "Enabled", c.enabled);
        c.resetTimerPerTarget = readBool("General", "ResetTimerPerTarget", c.resetTimerPerTarget);

        c.showStatus = readBool("Display", "Status", c.showStatus);
        c.showFile = readBool("Display", "FileName", c.showFile);
        c.showArch = readBool("Display", "Architecture", c.showArch);
        c.showModules = readBool("Display", "Modules", c.showModules);
        c.showThreads = readBool("Display", "Threads", c.showThreads);
        c.showLocation = readBool("Display", "Location", c.showLocation);
        c.showBreakpoints = readBool("Display", "Breakpoints", c.showBreakpoints);
        c.showExceptions = readBool("Display", "Exceptions", c.showExceptions);
        c.showTimer = readBool("Display", "Timer", c.showTimer);
        c.showIcons = readBool("Display", "Icons", c.showIcons);

        c.appId = readString("Discord", "ApplicationId", c.appId);
        c.idleText = readString("Discord", "IdleText", c.idleText);
        c.largeImageKey = readString("Discord", "LargeImageKey", c.largeImageKey);
        c.smallRunningKey = readString("Discord", "SmallImageRunning", c.smallRunningKey);
        c.smallPausedKey = readString("Discord", "SmallImagePaused", c.smallPausedKey);
        c.smallIdleKey = readString("Discord", "SmallImageIdle", c.smallIdleKey);
    }

    void save(const Config& c)
    {
        writeBool("General", "Enabled", c.enabled);
        writeBool("General", "ResetTimerPerTarget", c.resetTimerPerTarget);

        writeBool("Display", "Status", c.showStatus);
        writeBool("Display", "FileName", c.showFile);
        writeBool("Display", "Architecture", c.showArch);
        writeBool("Display", "Modules", c.showModules);
        writeBool("Display", "Threads", c.showThreads);
        writeBool("Display", "Location", c.showLocation);
        writeBool("Display", "Breakpoints", c.showBreakpoints);
        writeBool("Display", "Exceptions", c.showExceptions);
        writeBool("Display", "Timer", c.showTimer);
        writeBool("Display", "Icons", c.showIcons);

        writeString("Discord", "ApplicationId", c.appId);
        writeString("Discord", "IdleText", c.idleText);
        writeString("Discord", "LargeImageKey", c.largeImageKey);
        writeString("Discord", "SmallImageRunning", c.smallRunningKey);
        writeString("Discord", "SmallImagePaused", c.smallPausedKey);
        writeString("Discord", "SmallImageIdle", c.smallIdleKey);
    }
}
