#include "presence.h"
#include "pluginmain.h"

#include "DiscordSDK/include/discord_rpc.h"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
    // " · " as UTF-8, written as bytes so the source encoding never matters
    const char* const kSep = " \xC2\xB7 ";

    // Discord allows ~5 updates / 20s; stepping through code can fire far more than that.
    constexpr auto kMinUpdateInterval = std::chrono::seconds(4);

    struct Session
    {
        DebugStatus status = DebugStatus::Idle;
        std::string file;
        std::string location;       // only valid while paused
        int modules = 0;
        int threads = 0;
        unsigned breakpointHits = 0;
        unsigned exceptions = 0;
        int64_t startTime = 0;      // debug session start (unix seconds)
    };

    std::mutex gMutex;
    std::condition_variable gWake;
    std::thread gWorker;
    bool gStop = false;
    bool gDirty = false;
    bool gConnected = false;

    Config gCfg;
    Session gSession;
    int64_t gAppStart = 0;

    int64_t nowSeconds()
    {
        using namespace std::chrono;
        return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    }

    std::string baseName(const char* path)
    {
        std::string s = path ? path : "";
        const size_t slash = s.find_last_of("\\/");
        return slash == std::string::npos ? s : s.substr(slash + 1);
    }

    // Discord limits most fields to 128 bytes; never cut a UTF-8 sequence in half.
    std::string clamp(std::string s, size_t maxBytes = 127)
    {
        if (s.size() <= maxBytes)
            return s;
        size_t cut = maxBytes;
        while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80)
            --cut;
        return s.substr(0, cut);
    }

    std::string format(const char* fmt, unsigned long long a)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), fmt, a);
        return buf;
    }

    void join(std::string& out, const std::string& part)
    {
        if (!out.empty())
            out += kSep;
        out += part;
    }

    const char* archName()
    {
#ifdef _WIN64
        return "x64";
#else
        return "x32";
#endif
    }

    const char* debuggerName()
    {
#ifdef _WIN64
        return "x64dbg";
#else
        return "x32dbg";
#endif
    }

    // gMutex must be held
    void connect()
    {
        if (gConnected || !gCfg.enabled || gCfg.appId.empty())
            return;
        DiscordEventHandlers handlers = {};
        Discord_Initialize(gCfg.appId.c_str(), &handlers, 1, nullptr);
        gConnected = true;
    }

    // gMutex must be held
    void disconnect()
    {
        if (!gConnected)
            return;
        Discord_ClearPresence();
        Discord_Shutdown();
        gConnected = false;
    }

    // gMutex must be held
    void send()
    {
        if (!gConnected)
            return;

        const Config& c = gCfg;
        const Session& s = gSession;
        const bool active = s.status != DebugStatus::Idle;

        // line 1: what is going on
        std::string details;
        if (!active)
        {
            details = c.showStatus ? c.idleText : std::string();
        }
        else
        {
            static const char* labels[] = { "", "Loading", "Running", "Paused" };
            if (c.showStatus)
                details = labels[static_cast<int>(s.status)];
            if (c.showFile && !s.file.empty())
            {
                if (!details.empty())
                    details += ": ";
                details += s.file;
            }
        }
        if (details.empty())
            details = "Using x64dbg";

        // line 2: the technical details
        std::string state;
        if (c.showArch)
            join(state, archName());
        if (active)
        {
            if (c.showModules && s.modules > 0)
                join(state, format("%llu modules", s.modules));
            if (c.showThreads && s.threads > 0)
                join(state, format("%llu threads", s.threads));
            if (c.showLocation && s.status == DebugStatus::Paused && !s.location.empty())
                join(state, s.location);
            if (c.showBreakpoints && s.breakpointHits > 0)
                join(state, format("%llu BP hits", s.breakpointHits));
            if (c.showExceptions && s.exceptions > 0)
                join(state, format("%llu exceptions", s.exceptions));
        }

        details = clamp(details);
        state = clamp(state);

        const char* smallKey = nullptr;
        const char* smallText = nullptr;
        if (c.showIcons)
        {
            switch (s.status)
            {
            case DebugStatus::Running: smallKey = c.smallRunningKey.c_str(); smallText = "Running"; break;
            case DebugStatus::Paused:  smallKey = c.smallPausedKey.c_str();  smallText = "Paused";  break;
            case DebugStatus::Idle:    smallKey = c.smallIdleKey.c_str();    smallText = "Idle";    break;
            default: break;
            }
            if (smallKey && !*smallKey)
                smallKey = nullptr;
        }

        DiscordRichPresence p = {};
        p.details = details.c_str();
        p.state = state.empty() ? nullptr : state.c_str();
        p.largeImageKey = c.largeImageKey.c_str();
        p.largeImageText = debuggerName();
        p.smallImageKey = smallKey;
        p.smallImageText = smallText;
        if (c.showTimer)
            p.startTimestamp = (active && c.resetTimerPerTarget) ? s.startTime : gAppStart;
        Discord_UpdatePresence(&p);
    }

    // gMutex must NOT be held
    void markDirty()
    {
        {
            std::lock_guard<std::mutex> lock(gMutex);
            gDirty = true;
        }
        gWake.notify_one();
    }

    void workerLoop()
    {
        std::unique_lock<std::mutex> lock(gMutex);
        while (true)
        {
            gWake.wait(lock, [] { return gDirty || gStop; });
            if (gStop)
                break;
            gDirty = false;
            send();
            // coalesce everything that happens within the interval into one update
            gWake.wait_for(lock, kMinUpdateInterval, [] { return gStop; });
        }
    }

    std::string locationOf(duint addr)
    {
        char mod[MAX_MODULE_SIZE] = {};
        if (DbgGetModuleAt(addr, mod) && mod[0])
        {
            const duint base = DbgModBaseFromName(mod);
            if (base && addr >= base)
            {
                char buf[MAX_MODULE_SIZE + 32];
                snprintf(buf, sizeof(buf), "%s+0x%llX", mod, static_cast<unsigned long long>(addr - base));
                return buf;
            }
        }
        return format("0x%llX", addr);
    }
}

namespace presence
{
    void start(const Config& cfg)
    {
        {
            std::lock_guard<std::mutex> lock(gMutex);
            gCfg = cfg;
            gAppStart = nowSeconds();
            gStop = false;
            gDirty = true;
            connect();
        }
        gWorker = std::thread(workerLoop);
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(gMutex);
            gStop = true;
        }
        gWake.notify_all();
        if (gWorker.joinable())
            gWorker.join();

        std::lock_guard<std::mutex> lock(gMutex);
        disconnect();
    }

    Config config()
    {
        std::lock_guard<std::mutex> lock(gMutex);
        return gCfg;
    }

    void setConfig(const Config& cfg)
    {
        {
            std::lock_guard<std::mutex> lock(gMutex);
            const bool reconnect = cfg.appId != gCfg.appId || cfg.enabled != gCfg.enabled;
            gCfg = cfg;
            config::save(gCfg);
            if (reconnect)
                disconnect();
            connect();
        }
        markDirty();
    }

    void onInitDebug(const char* path)
    {
        {
            std::lock_guard<std::mutex> lock(gMutex);
            gSession = Session();
            gSession.status = DebugStatus::Loading;
            gSession.file = baseName(path);
            gSession.startTime = nowSeconds();
        }
        markDirty();
    }

    void onCreateProcess(const char* path)
    {
        {
            std::lock_guard<std::mutex> lock(gMutex);
            if (path && *path)
                gSession.file = baseName(path);   // also covers attach, which skips CB_INITDEBUG's name
            gSession.status = DebugStatus::Running;
            gSession.modules = 1;
            gSession.threads = 1;
        }
        markDirty();
    }

    void onStopDebug()
    {
        {
            std::lock_guard<std::mutex> lock(gMutex);
            gSession = Session();
        }
        markDirty();
    }

    void onPause()
    {
        // we are on the debug thread and the debuggee is paused, so reading cip is safe
        const std::string loc = locationOf(DbgValFromString("cip"));
        {
            std::lock_guard<std::mutex> lock(gMutex);
            if (gSession.status == DebugStatus::Idle)
                return;
            gSession.status = DebugStatus::Paused;
            gSession.location = loc;
        }
        markDirty();
    }

    void onResume()
    {
        {
            std::lock_guard<std::mutex> lock(gMutex);
            if (gSession.status == DebugStatus::Idle)
                return;
            gSession.status = DebugStatus::Running;
            gSession.location.clear();
        }
        markDirty();
    }

    void onModuleLoaded()   { { std::lock_guard<std::mutex> l(gMutex); ++gSession.modules; } markDirty(); }
    void onModuleUnloaded() { { std::lock_guard<std::mutex> l(gMutex); if (gSession.modules > 0) --gSession.modules; } markDirty(); }
    void onThreadCreated()  { { std::lock_guard<std::mutex> l(gMutex); ++gSession.threads; } markDirty(); }
    void onThreadExited()   { { std::lock_guard<std::mutex> l(gMutex); if (gSession.threads > 0) --gSession.threads; } markDirty(); }
    void onBreakpointHit()  { { std::lock_guard<std::mutex> l(gMutex); ++gSession.breakpointHits; } markDirty(); }
    void onException()      { { std::lock_guard<std::mutex> l(gMutex); ++gSession.exceptions; } markDirty(); }

    void resetSession()
    {
        {
            std::lock_guard<std::mutex> lock(gMutex);
            gSession.breakpointHits = 0;
            gSession.exceptions = 0;
            gSession.startTime = nowSeconds();
            gAppStart = nowSeconds();
        }
        markDirty();
    }
}
