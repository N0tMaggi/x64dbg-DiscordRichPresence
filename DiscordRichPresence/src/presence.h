#pragma once

#include "config.h"

#include <string>

enum class DebugStatus
{
    Idle,
    Loading,
    Running,
    Paused
};

// Owns the Discord connection and the debug-session state shown in the presence.
// Thread-safe: x64dbg calls us from the debug thread (callbacks) and the GUI thread (menu).
namespace presence
{
    void start(const Config& cfg);   // load config, spawn sender thread, connect if enabled
    void stop();                     // clear presence, disconnect, join sender thread

    // configuration access (copy in / copy out so the menu never races the sender)
    Config config();
    void setConfig(const Config& cfg);   // applies + saves + reconnects when app id / enabled changed

    // session events
    void onInitDebug(const char* path);
    void onCreateProcess(const char* path);
    void onStopDebug();
    void onPause();
    void onResume();
    void onModuleLoaded();
    void onModuleUnloaded();
    void onThreadCreated();
    void onThreadExited();
    void onBreakpointHit();
    void onException();

    void resetSession();                 // zero counters + timer
}
