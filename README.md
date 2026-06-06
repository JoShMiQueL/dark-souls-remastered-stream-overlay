# Dark Souls Remastered — Stream Tracker

A native DLL mod for **Dark Souls Remastered** that reads game memory and exposes player stats through a WebSocket server, designed for live streaming overlays in OBS.

> **Read-only.** The mod never writes to game memory.

---

## How it works

The DLL is injected as a `dinput8.dll` proxy — it loads alongside the game without patching any files, forwards all DirectInput calls to the real system DLL, and runs a background thread that scans for the game's internal data structures and serves them over WebSocket on port 3000.

---

## Demo

<!-- Screenshot or GIF of the OBS overlay in action -->
![OBS overlay demo](docs/images/obs_overlay.png)

<!-- Screenshot of the OBS Browser Source configuration panel -->
![OBS Browser Source setup](docs/images/obs_setup.png)

---

## Features

- **DLL proxy** — loads via `dinput8.dll` hijacking, no game files modified
- **Pattern scanning** — finds the player struct at runtime, compatible across patches
- **WebSocket server** — real-time data push, only sends on actual stat changes
- **Modular HTTP overlay** — pick which stats to show and in what order via URL params
- **OBS-friendly** — transparent background, fully styled with OBS Custom CSS
- **Initial state push** — new WebSocket clients receive the current state immediately on connect
- **Graceful shutdown** — clean thread and socket teardown on game exit

---

## Exposed stats

| URL param | JSON key | Description |
|-----------|----------|-------------|
| `hp` | `hp` / `maxHp` | Current and max HP |
| `fp` | `fp` / `maxFp` | Focus Points (mana) |
| `stamina` | `stamina` / `maxStamina` | Stamina |
| `souls` | `souls` | Current souls |
| `soulsTotal` | `soulsTotal` | Total souls collected |
| `soulLevel` | `soulLevel` | Soul level |
| `deaths` | `deaths` | Death counter |
| `trueDeaths` | `trueDeaths` | True death counter |
| `playTime` | `playTime` | Play time in seconds |
| `vit` | `vit` | Vitality |
| `atn` | `atn` | Attunement |
| `end` | `end` | Endurance |
| `str` | `str` | Strength |
| `dex` | `dex` | Dexterity |
| `res` | `res` | Resistance |
| `int` | `int` | Intelligence |
| `fth` | `fth` | Faith |
| `poisonResist` | `poisonResist` | Poison resistance |
| `bleedResist` | `bleedResist` | Bleed resistance |
| `diseaseResist` | `diseaseResist` | Disease resistance |
| `curseResist` | `curseResist` | Curse resistance |

---

## Requirements

- Windows 10/11 x64
- Dark Souls Remastered (Steam)
- Visual Studio 2019 or 2022 (for building)

---

## Building

```powershell
.\scripts\build.ps1
```

The script locates MSBuild automatically and builds a Release x64 `dinput8.dll` into `build\Release\`.

Alternatively, open `DarkSoulsTracker.vcxproj` in Visual Studio and build **Release | x64**.

---

## Installation

1. Build the project (or download a release)
2. Copy `build\Release\dinput8.dll` to your game folder:
   ```
   C:\Program Files (x86)\Steam\steamapps\common\DARK SOULS REMASTERED\
   ```
3. Launch the game — the WebSocket server starts automatically on port 3000

> **Backup** any existing `dinput8.dll` in the game folder before installing.

---

## OBS Setup

### Basic — all stats

Add a **Browser Source** in OBS:

| Field | Value |
|-------|-------|
| URL | `http://localhost:3000/` |
| Width | 400 |
| Height | 300 |
| Shutdown source when not visible | ✓ |

### Selective — specific stats in custom order

Use `?stat=` params. Order in the URL = render order on screen.

```
http://localhost:3000/?stat=hp&stat=deaths
http://localhost:3000/?stat=deaths&stat=souls&stat=soulLevel
http://localhost:3000/?stat=hp&stat=stamina&stat=fp
```

### Styling with OBS Custom CSS

The HTML has no embedded styles (only `background:transparent`). Paste your CSS directly into the **Custom CSS** field of the Browser Source.

**Available selectors:**

```css
/* Containers */
#stat-hp, #stat-fp, #stat-stamina,
#stat-souls, #stat-soulsTotal, #stat-soulLevel,
#stat-deaths, #stat-trueDeaths, #stat-playTime,
#stat-vit, #stat-atn, #stat-end, #stat-str,
#stat-dex, #stat-res, #stat-int, #stat-fth,
#stat-poisonResist, #stat-bleedResist,
#stat-diseaseResist, #stat-curseResist { }

/* Parts */
.stat  { }   /* each row */
.label { }   /* "HP: ", "Deaths: " … */
.value { }   /* the number */
.sep   { }   /* " / " between paired values */

/* Individual values */
#hp, #maxHp, #fp, #maxFp, #stamina, #maxStamina,
#souls, #soulsTotal, #soulLevel,
#deaths, #trueDeaths, #playTime { }
```

**Example — deaths counter only, large red text:**
```css
body { margin: 0; }
#stat-deaths { font-size: 48px; font-weight: bold; color: #cc0000; }
.label { display: none; }
```

---

## WebSocket API

Connect to `ws://localhost:3000/ws` to receive JSON on every stat change:

```json
{
  "hp": 450, "maxHp": 1000,
  "fp": 80,  "maxFp": 100,
  "stamina": 93, "maxStamina": 93,
  "souls": 12500, "soulsTotal": 847300, "soulLevel": 42,
  "deaths": 7, "trueDeaths": 3, "playTime": 18340,
  "vit": 20, "atn": 12, "end": 20, "str": 16,
  "dex": 14, "res": 11, "int": 10, "fth": 10,
  "poisonResist": 87, "bleedResist": 67,
  "diseaseResist": 57, "curseResist": 47
}
```

- Data is pushed **only when a value changes** (no polling noise)
- On connect, the current state is sent immediately
- Reconnection is handled automatically by the overlay page (3 s retry)

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
│   ├── WebSocketServer.h
│   └── DebugConsole.h
├── scripts/
│   └── build.ps1             # MSBuild wrapper
├── build/                    # Compiled output (git-ignored)
├── POINTER_MAP.md            # Full pointer reference from the Cheat Table
├── All_DarkSoulsRemastered_CheatTables.CT  # Source Cheat Table (reference only)
├── DarkSoulsTracker.vcxproj
└── README.md
```

---

## Adding new stats

1. Look up the offset chain in [`POINTER_MAP.md`](POINTER_MAP.md)
2. Add a field to `PlayerStats` in `include/MemoryReader.h`
3. Read it in `MemoryReader::ReadPlayerStats()` (`src/MemoryReader.cpp`)
4. Serialize it in `WebSocketServer::StatsToJson()` (`src/WebSocketServer.cpp`)
5. Add an HTML block in `WebSocketServer::StatBlock()` (`src/WebSocketServer.cpp`)
6. Rebuild

---

## Debug

The debug console is **disabled by default**.  
To enable it for development, set `#define ENABLE_DEBUG_CONSOLE 1` in `include/DebugConsole.h` and rebuild.  
A `dstracker.log` file is written to the game folder regardless of this setting.

---

## Troubleshooting

| Problem | Likely cause |
|---------|-------------|
| Game doesn't start | Wrong architecture — make sure you built **x64** |
| Stats show `-` | Game not fully loaded yet — the mod waits 5 s on startup |
| WebSocket won't connect | Port 3000 in use; check with `netstat -an \| findstr 3000` |
| Stats stuck / wrong | Load a save game first; stats are only valid in-game |

---

## License & credits

- Pointer map sourced from **[Phokz's Dark Souls Remastered Cheat Tables](https://www.nexusmods.com/darksoulsremastered/mods/798)** (Nexus Mods #798)
- Not affiliated with FromSoftware or Bandai Namco
- For educational and personal streaming use only
- **Use at your own risk** online (read-only, but any mod carries risk)
