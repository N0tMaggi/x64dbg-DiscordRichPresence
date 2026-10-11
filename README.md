# x64dbg Discord Rich Presence

Shows what you are debugging in x64dbg / x32dbg as your Discord status.

Works out of the box. No Discord setup needed.

## What it shows

- Status: running / paused
- Target file name
- Architecture (x64 / x32)
- Loaded modules and threads
- Current location (`module+offset`) while paused
- Breakpoint hits and exceptions
- Elapsed timer

Every item can be switched on or off in the plugin menu.

## Install

1. Download the latest zip from [Releases](../../releases/latest).
2. Extract it into your x64dbg folder. The zip already has the right layout:
   - `x64\plugins\DiscordRichPresence.dp64`
   - `x32\plugins\DiscordRichPresence.dp32`
3. Start x64dbg / x32dbg. Discord must be running.

## Settings

Open **Plugins > Discord Rich Presence** in x64dbg.

| Menu | What it does |
| --- | --- |
| Enabled | Turn the plugin on or off |
| Shown data | Tick what gets shown (file name, location, ...) |
| Settings | Optional: own Discord application ID, idle text, own images |
| Reset session | Reset counters and timer |
| Open / reload config file | Edit `DiscordRichPresence.ini` (next to the plugin) by hand |

Settings are saved automatically.

**Privacy:** untick *Target file name* and *Current location* if you don't want to show what you are analyzing.

### Own images / app (optional)

By default the images are loaded from open-source links (x64dbg logo, Twemoji icons).
In **Settings** you can enter your own Discord application ID, and for each image either an
asset key of your application or a direct `https://` link to a PNG/JPG.

## Build

Requires Visual Studio 2026 (toolset v145) with the C++ desktop workload.

```
msbuild DiscordRichPresence.sln -p:Configuration=Release -p:Platform=x64
msbuild DiscordRichPresence.sln -p:Configuration=Release -p:Platform=x86
```

Output: `bin\x64\Release\DiscordRichPresence.dp64` and `bin\Win32\Release\DiscordRichPresence.dp32`.

Or open `DiscordRichPresence.sln` in Visual Studio and build.

## Release

Push a tag like `v1.0.0`. GitHub Actions builds both versions and publishes a release with the zip.

```
git tag v1.0.0
git push origin v1.0.0
```

## Credits

- [discord-rpc](https://github.com/discord/discord-rpc) (MIT) - in `third_party/DiscordSDK`
- [x64dbg plugin SDK](https://github.com/x64dbg/x64dbg) - in `third_party/pluginsdk`
- Icons: [Twemoji](https://github.com/jdecked/twemoji) (CC-BY 4.0)
- Based on the original x64dbg-DiscordRPC plugin
