#include "plugin.h"
#include "config.h"
#include "presence.h"

#include <Windows.h>
#include <shellapi.h>

#include <cstring>

extern HINSTANCE gModule;

namespace
{
    // menu entry ids (unique per plugin)
    enum MenuId : int
    {
        MENU_ENABLED = 1,

        MENU_STATUS, MENU_FILE, MENU_ARCH, MENU_MODULES, MENU_THREADS,
        MENU_LOCATION, MENU_BREAKPOINTS, MENU_EXCEPTIONS, MENU_TIMER, MENU_ICONS,

        MENU_RESET_TIMER_PER_TARGET,
        MENU_RESET_SESSION,

        MENU_SET_APPID, MENU_SET_IDLETEXT, MENU_SET_LARGE,
        MENU_SET_SMALL_RUNNING, MENU_SET_SMALL_PAUSED, MENU_SET_SMALL_IDLE,

        MENU_OPEN_CONFIG, MENU_RELOAD_CONFIG, MENU_ABOUT
    };

    struct ToggleEntry
    {
        MenuId id;
        const char* title;
        bool Config::* field;
    };

    struct TextEntry
    {
        MenuId id;
        const char* title;
        const char* prompt;
        std::string Config::* field;
    };

    const ToggleEntry kMainToggles[] = {
        { MENU_ENABLED, "&Enabled", &Config::enabled },
    };

    const ToggleEntry kDataToggles[] = {
        { MENU_STATUS,      "Debug &status (running / paused)",          &Config::showStatus },
        { MENU_FILE,        "Target &file name",                         &Config::showFile },
        { MENU_ARCH,        "&Architecture (x64 / x32)",                 &Config::showArch },
        { MENU_MODULES,     "Loaded &modules",                           &Config::showModules },
        { MENU_THREADS,     "&Threads",                                  &Config::showThreads },
        { MENU_LOCATION,    "Current &location (module+offset, paused)", &Config::showLocation },
        { MENU_BREAKPOINTS, "&Breakpoint hits",                          &Config::showBreakpoints },
        { MENU_EXCEPTIONS,  "&Exceptions",                               &Config::showExceptions },
        { MENU_TIMER,       "Elapsed t&imer",                            &Config::showTimer },
        { MENU_ICONS,       "Small status &icon",                        &Config::showIcons },
    };

    const ToggleEntry kOptionToggles[] = {
        { MENU_RESET_TIMER_PER_TARGET, "&Restart timer for every new target", &Config::resetTimerPerTarget },
    };

    const TextEntry kTextEntries[] = {
        { MENU_SET_APPID,         "Discord &application ID (optional)...", "Discord application ID (leave the default unless you have your own)",             &Config::appId },
        { MENU_SET_IDLETEXT,      "&Idle text...",                "Text shown while nothing is debugged", &Config::idleText },
        { MENU_SET_LARGE,         "&Large image (key or link)...",  "Large image: asset key or https link to a PNG/JPG",              &Config::largeImageKey },
        { MENU_SET_SMALL_RUNNING, "Small image: &running (key or link)...", "Small image for 'running': asset key or https link",      &Config::smallRunningKey },
        { MENU_SET_SMALL_PAUSED,  "Small image: &paused (key or link)...",  "Small image for 'paused': asset key or https link",       &Config::smallPausedKey },
        { MENU_SET_SMALL_IDLE,    "Small image: i&dle (key or link)...",    "Small image for 'idle': asset key or https link",         &Config::smallIdleKey },
    };

    void addToggle(int menu, const ToggleEntry& e, const Config& cfg)
    {
        _plugin_menuaddentry(menu, e.id, e.title);
        _plugin_menuentrysetchecked(pluginHandle, e.id, cfg.*(e.field));
    }

    void syncChecks(const Config& cfg)
    {
        for (const auto& e : kMainToggles)   _plugin_menuentrysetchecked(pluginHandle, e.id, cfg.*(e.field));
        for (const auto& e : kDataToggles)   _plugin_menuentrysetchecked(pluginHandle, e.id, cfg.*(e.field));
        for (const auto& e : kOptionToggles) _plugin_menuentrysetchecked(pluginHandle, e.id, cfg.*(e.field));
    }

    template <size_t N>
    bool handleToggle(const ToggleEntry (&table)[N], int hEntry)
    {
        for (const auto& e : table)
        {
            if (e.id != hEntry)
                continue;
            Config cfg = presence::config();
            cfg.*(e.field) = !(cfg.*(e.field));
            presence::setConfig(cfg);
            _plugin_menuentrysetchecked(pluginHandle, e.id, cfg.*(e.field));
            return true;
        }
        return false;
    }

    bool handleText(int hEntry)
    {
        for (const auto& e : kTextEntries)
        {
            if (e.id != hEntry)
                continue;
            Config cfg = presence::config();
            static char buffer[GUI_MAX_LINE_SIZE];
            buffer[0] = '\0';
            if (GuiGetLineWindow(e.prompt, buffer))   // returns false on Cancel
            {
                std::string value = buffer;
                if (value.size() > 255)
                    value.resize(255);   // Discord caps image fields at 256 chars
                cfg.*(e.field) = value;
                presence::setConfig(cfg);
                _plugin_logprintf("[Discord RPC] %s set to \"%s\"\n", e.prompt, value.c_str());
            }
            return true;
        }
        return false;
    }

    void onMenuEntry(CBTYPE, void* callbackInfo)
    {
        const int entry = static_cast<PLUG_CB_MENUENTRY*>(callbackInfo)->hEntry;

        if (handleToggle(kMainToggles, entry) || handleToggle(kDataToggles, entry) || handleToggle(kOptionToggles, entry) || handleText(entry))
            return;

        switch (entry)
        {
        case MENU_RESET_SESSION:
            presence::resetSession();
            _plugin_logputs("[Discord RPC] Session counters and timer reset");
            break;
        case MENU_OPEN_CONFIG:
            ShellExecuteA(nullptr, "open", config::path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case MENU_RELOAD_CONFIG:
        {
            Config cfg = presence::config();
            config::load(cfg);
            presence::setConfig(cfg);
            syncChecks(cfg);
            _plugin_logputs("[Discord RPC] Config reloaded");
            break;
        }
        case MENU_ABOUT:
            MessageBoxA(hwndDlg,
                "Discord Rich Presence for x64dbg\n\n"
                "Shows your debugging status in Discord.\n"
                "Choose what gets shared under Plugins > Discord Rich Presence.\n\n"
                "Tip: turn off \"Target file name\" if you don't want to show what you're analyzing.",
                PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
            break;
        }
    }

    // --- debugger callbacks -------------------------------------------------

    void onInitDebug(CBTYPE, void* info)     { presence::onInitDebug(static_cast<PLUG_CB_INITDEBUG*>(info)->szFileName); }
    void onCreateProcess(CBTYPE, void* info) { presence::onCreateProcess(static_cast<PLUG_CB_CREATEPROCESS*>(info)->DebugFileName); }
    void onStopDebug(CBTYPE, void*)          { presence::onStopDebug(); }
    void onPause(CBTYPE, void*)              { presence::onPause(); }
    void onResume(CBTYPE, void*)             { presence::onResume(); }
    void onLoadDll(CBTYPE, void*)            { presence::onModuleLoaded(); }
    void onUnloadDll(CBTYPE, void*)          { presence::onModuleUnloaded(); }
    void onCreateThread(CBTYPE, void*)       { presence::onThreadCreated(); }
    void onExitThread(CBTYPE, void*)         { presence::onThreadExited(); }
    void onBreakpoint(CBTYPE, void*)         { presence::onBreakpointHit(); }
    void onException(CBTYPE, void*)          { presence::onException(); }
}

//Initialize plugin data
bool pluginInit(PLUG_INITSTRUCT*)
{
    config::init(gModule);
    Config cfg;
    config::load(cfg);
    config::save(cfg);   // writes defaults on first run so the .ini is easy to find and edit
    presence::start(cfg);

    _plugin_registercallback(pluginHandle, CB_INITDEBUG, onInitDebug);
    _plugin_registercallback(pluginHandle, CB_CREATEPROCESS, onCreateProcess);
    _plugin_registercallback(pluginHandle, CB_STOPDEBUG, onStopDebug);
    _plugin_registercallback(pluginHandle, CB_PAUSEDEBUG, onPause);
    _plugin_registercallback(pluginHandle, CB_RESUMEDEBUG, onResume);
    _plugin_registercallback(pluginHandle, CB_LOADDLL, onLoadDll);
    _plugin_registercallback(pluginHandle, CB_UNLOADDLL, onUnloadDll);
    _plugin_registercallback(pluginHandle, CB_CREATETHREAD, onCreateThread);
    _plugin_registercallback(pluginHandle, CB_EXITTHREAD, onExitThread);
    _plugin_registercallback(pluginHandle, CB_BREAKPOINT, onBreakpoint);
    _plugin_registercallback(pluginHandle, CB_EXCEPTION, onException);
    _plugin_registercallback(pluginHandle, CB_MENUENTRY, onMenuEntry);
    return true;
}

//Deinitialize plugin data
bool pluginStop()
{
    presence::stop();
    return true;
}

//Build the menu under Plugins > Discord Rich Presence
void pluginSetup()
{
    const Config cfg = presence::config();

    for (const auto& e : kMainToggles)
        addToggle(hMenu, e, cfg);
    _plugin_menuaddseparator(hMenu);

    const int hData = _plugin_menuadd(hMenu, "&Shown data");
    for (const auto& e : kDataToggles)
        addToggle(hData, e, cfg);

    const int hSettings = _plugin_menuadd(hMenu, "&Settings");
    for (const auto& e : kOptionToggles)
        addToggle(hSettings, e, cfg);
    _plugin_menuaddseparator(hSettings);
    for (const auto& e : kTextEntries)
        _plugin_menuaddentry(hSettings, e.id, e.title);

    _plugin_menuaddseparator(hMenu);
    _plugin_menuaddentry(hMenu, MENU_RESET_SESSION, "&Reset session counters && timer");
    _plugin_menuaddentry(hMenu, MENU_OPEN_CONFIG, "&Open config file");
    _plugin_menuaddentry(hMenu, MENU_RELOAD_CONFIG, "Re&load config file");
    _plugin_menuaddseparator(hMenu);
    _plugin_menuaddentry(hMenu, MENU_ABOUT, "&About");

    _plugin_logputs("[Discord RPC] Started");
}
