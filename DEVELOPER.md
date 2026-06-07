# Dark Souls Tracker - Developer Guide

This document contains technical information for developers who want to build, modify, or extend the Dark Souls Tracker.

---

## Building

```powershell
.\scripts\build.ps1
```

The script locates MSBuild automatically (via `vswhere` or known VS install paths) and builds a Release x64 `dinput8.dll` into `build\Release\`.

Alternatively, open `DarkSoulsTracker.vcxproj` in Visual Studio and build **Release | x64**.

**Requirements:**
- Windows 10/11 x64
- Visual Studio 2022 or 2026 (for building)
- C++17

---

## Project structure

```
DarkSoulsTracker/
├── src/
│   ├── dllmain.cpp           # DLL entry point, game loop thread
│   ├── DirectInputProxy.cpp  # Forwards DirectInput8Create to system DLL
│   ├── MemoryReader.cpp      # Pattern scan + pointer chain reads
│   ├── WebSocketServer.cpp   # HTTP + WebSocket server
│   └── DebugConsole.cpp      # Console + log file output
├── include/
│   ├── MemoryReader.h        # PlayerStats struct (add new stats here)
│   ├── StatRegistry.h        # Data-driven stat ↔ JSON/display mapping
│   ├── WebSocketServer.h
│   └── DebugConsole.h
├── scripts/
│   └── build.ps1             # MSBuild wrapper (supports VS 2019/2022/2026)
├── build/                    # Compiled output (git-ignored)
├── DarkSoulsTracker.rc       # Version resource embedded in the DLL
├── POINTER_MAP.md            # Full pointer reference from the Cheat Table
├── All_DarkSoulsRemastered_CheatTables.CT  # Source Cheat Table (reference only)
├── DarkSoulsTracker.vcxproj
└── README.md
```

---

## Adding new stats

**Workflow:**

1. **Find the memory offset** — Look up the offset chain in [`POINTER_MAP.md`](POINTER_MAP.md) or use Cheat Engine to locate the value in game memory

2. **Add the field to PlayerStats** — Add a new `int32_t` field to the `PlayerStats` struct in `include/MemoryReader.h` (**before** the `valid` field):
   ```cpp
   struct PlayerStats {
       // ... existing fields ...
       int32_t newStat;  // Add your new stat here
       bool valid;       // Keep this last
   };
   ```

3. **Read the value** — Add the pointer chain read in `MemoryReader::ReadPlayerStats()` in `src/MemoryReader.cpp`:
   ```cpp
   stats.newStat = ReadInt32(baseB + offset1 + offset2);
   ```

4. **Register for JSON** — Add a `JsonField` entry in `include/StatRegistry.h` to expose it in the WebSocket JSON:
   ```cpp
   static const JsonField JSON_FIELDS[] = {
       // ... existing fields ...
       {"newStat", offsetof(PS, newStat)},
   };
   ```

5. **Register for display** — Add a `DisplayStat` entry in `include/StatRegistry.h` for the overlay:
   ```cpp
   static const DisplayStat DISPLAY_STATS[] = {
       // ... existing stats ...
       {"newStat", "New Stat Label", nullptr},
   };
   ```

6. **Rebuild** — Run `.\scripts\build.ps1` and copy the new DLL to your game folder

**Automatic benefits:**
- The stat is automatically available in `{newStat}` for URL templates
- JSON serialization works without additional code
- Change detection includes the new stat automatically
- The overlay system can display it without HTML changes

**Example:** Adding a new "Luck" stat
```cpp
// MemoryReader.h
int32_t luck;

// MemoryReader.cpp
stats.luck = ReadInt32(baseB + 0x1A4);

// StatRegistry.h
{"luck", offsetof(PS, luck)},
{"luck", "Luck", nullptr},
```

After rebuilding, users can use `{luck}` in their templates:
```
?Luck:%20{luck}
```

---

## Debug

The debug console is **disabled by default**.  
To enable it for development, set `#define ENABLE_DEBUG_CONSOLE 1` in `include/DebugConsole.h` and rebuild.  
A `dstracker.log` file is written to the game folder regardless of this setting.

---

## Code style

- C++17 (`stdcpp17` in vcxproj)
- Warning level 3 (`/W3`), SDL checks enabled
- `WIN32_LEAN_AND_MEAN` always defined
- Include winsock2.h before windows.h to avoid redefinition warnings
- Use `sprintf_s` and other secure CRT functions (MSVC)
- `#pragma comment(lib, ...)` for link dependencies

---

## Rules

- **Never write to game memory.** This mod is strictly read-only.
- Do not add external dependencies — the project uses only Win32 APIs and the C++ standard library.
- Do not modify `.vcxproj` without good reason; it is the canonical build definition.
- Keep the WebSocket server minimal — no frameworks, no npm, just raw WinSock.
- All new stats must follow the data-driven workflow documented above.

---

## Testing

No automated tests. The DLL requires Dark Souls Remastered running on Windows to test.

---

## No Linter

Compiler warnings (`/W3 + /WX-`) are the quality gate. No external linter is configured.
