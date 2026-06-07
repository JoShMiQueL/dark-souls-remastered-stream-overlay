# Dark Souls Remastered — Stream Tracker

A native DLL mod for **Dark Souls Remastered** that reads game memory and exposes player stats through a WebSocket server, designed for live streaming overlays in OBS.

> **Read-only.** The mod never writes to game memory.

---

## How it works

The DLL is injected as a `dinput8.dll` proxy — it loads alongside the game without patching any files, forwards all DirectInput calls to the real system DLL, and runs a background thread that scans for the game's internal data structures and serves them over WebSocket (port 3000 by default, configurable via `dstracker.ini`).

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
- **Configurable port** — set via `dstracker.ini`, defaults to 3000
- **Modular HTTP overlay** — pick which stats to show and in what order via URL params
- **OBS-friendly** — transparent background, fully styled with OBS Custom CSS
- **Initial state push** — new WebSocket clients receive the current state immediately on connect
- **Multi-PC support** — run the game on one machine and capture stats from the streaming PC
- **Graceful shutdown** — clean WebSocket close frames, proper thread teardown on game exit
- **Auto-recovery** — re-scans memory pointers after consecutive read failures

---

## Exposed stats

These are the available variables you can use in `{brackets}` in your templates. Use the **JSON key** column values.

| JSON key | Description |
|----------|-------------|
| `hp` / `maxHp` | Current and max HP |
| `fp` / `maxFp` | Focus Points (mana) |
| `stamina` / `maxStamina` | Stamina |
| `souls` | Current souls |
| `soulsTotal` | Total souls collected |
| `soulLevel` | Soul level |
| `deaths` | Death counter |
| `trueDeaths` | True death counter |
| `playTime` | Play time (seconds, format as H:MM:SS in templates) |
| `vit` | Vitality |
| `atn` | Attunement |
| `end` | Endurance |
| `str` | Strength |
| `dex` | Dexterity |
| `res` | Resistance |
| `int` | Intelligence |
| `fth` | Faith |
| `poisonResist` | Poison resistance |
| `bleedResist` | Bleed resistance |
| `diseaseResist` | Disease resistance |
| `curseResist` | Curse resistance |
| `ngPlus` | NG+ count (0 = first playthrough) |
| `archetype` | Starting class ID |
| `covenant` | Active covenant ID |

**Example usage:**
```
?HP:%20{hp}/{maxHp}%20|Souls:%20{souls}%20|SL:%20{soulLevel}
```

---

## Installation

1. Download the latest release from [GitHub Releases](https://github.com/yourusername/DarkSoulsTracker/releases)
2. Copy `dinput8.dll` to your game folder:
   ```
   C:\Program Files (x86)\Steam\steamapps\common\DARK SOULS REMASTERED\
   ```
3. Launch the game — the WebSocket server starts automatically on port 3000

> **Backup** any existing `dinput8.dll` in the game folder before installing.

---

## Requirements

- Windows 10/11 x64
- Dark Souls Remastered (Steam)
- OBS Studio (for overlays)

> **For developers:** See [DEVELOPER.md](DEVELOPER.md) for build requirements and technical details.

---

## Configuration

Create a `dstracker.ini` file in the same folder as `dinput8.dll` to customize settings:

```ini
[Server]
Port=3000
```

| Key | Default | Description |
|-----|---------|-------------|
| `Port` | `3000` | WebSocket/HTTP server port |

If the file doesn't exist, all defaults are used.

---

## OBS Setup

### Basic — all stats

Add a **Browser Source** in OBS:

| Field | Value |
|-------|-------|
| URL | `http://localhost:3000/` |
| Width | 400 |
| Height | 300 |
| Shutdown source when not visible | yes |

### Custom formatting — simplified syntax

The simplified syntax uses templates directly in the URL. You can reference any stat variable in `{brackets}`.

**Single line:**
```
http://localhost:3000/?Vida:%20{hp}/{maxHp}%20-%20{playTime}
```

**Multiple lines (pipe separator):**
```
http://localhost:3000/?Vida:%20{hp}/{maxHp}|Almas:%20{souls}|Muertes:%20{deaths}
```

**Multiple lines (named parameters):**
```
http://localhost:3000/?line1=Vida:%20{hp}/{maxHp}&line2=Almas:%20{souls}&line3=Muertes:%20{deaths}
```

**Available variables:** All stat keys from the table above (`{hp}`, `{maxHp}`, `{fp}`, `{souls}`, `{deaths}`, etc.)

### URL Builder

For complex overlays, use an online URL encoder/formatter to build your URLs:

1. Write your template in plain text: `Vida: {hp}/{maxHp} - Tiempo: {playTime}`
2. Use a URL encoder (e.g., [urlencoder.org](https://www.urlencoder.org/)) to encode it
3. Paste the encoded text after `?` in your OBS Browser Source

**Example workflow:**
```
Plain text: Vida: {hp}/{maxHp} - Tiempo: {playTime}
Encoded: Vida:%20{hp}/{maxHp}%20-%20Tiempo:%20{playTime}
Final URL: http://localhost:3000/?Vida:%20{hp}/{maxHp}%20-%20Tiempo:%20{playTime}
```

### Custom HTML overlays

For complete control over your overlay design, you can create your own HTML file and connect to the WebSocket directly.

**When to use custom HTML:**
- Complex layouts (grids, bars, progress indicators)
- Custom animations and transitions
- Integration with other web services
- When you need full CSS/JavaScript control

**How to use:**

1. Copy `docs/raw-template.html` as a starting point
2. Modify the HTML/CSS to match your design
3. Add the file as a **Local File** Browser Source in OBS
4. The template connects to `ws://localhost:3000/ws` automatically

**Example features in raw-template.html:**
- HP bar with percentage visualization
- Custom styling and colors
- Emoji integration
- Auto-reconnect on connection loss

**WebSocket data format:**
```json
{
  "hp": 450, "maxHp": 1000,
  "fp": 80, "maxFp": 100,
  "stamina": 93, "maxStamina": 93,
  "souls": 12500, "soulsTotal": 847300, "soulLevel": 42,
  "deaths": 7, "trueDeaths": 3, "playTime": 18340,
  ...
}
```

**Comparison:**
- **URL templates:** Quick setup, simple overlays, no HTML editing
- **Custom HTML:** Full control, complex designs, requires HTML/CSS knowledge

### Multi-PC setup (game PC + streaming PC)

The WebSocket server listens on all network interfaces by default, so you can run the game on one machine and capture stats from a different PC on the same network:

1. Install the DLL on the **game PC** as usual
2. Find the game PC's local IP (e.g. `192.168.1.50`)
3. On the **streaming PC**, add a Browser Source pointing to the game PC:
   ```
   http://192.168.1.50:3000/?HP:%20{hp}/{maxHp}|Souls:%20{souls}
   ```
4. The WebSocket will connect across the network and update in real time

> **Firewall:** make sure the game PC allows inbound connections on the configured port (default 3000). You may need to add a Windows Firewall rule.

> **Security note:** the server accepts connections from any IP. Only use this on trusted local networks.

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
#stat-diseaseResist, #stat-curseResist,
#stat-ngPlus, #stat-archetype, #stat-covenant { }

/* Parts */
.stat  { }   /* each row */
.label { }   /* "HP: ", "Deaths: " … */
.value { }   /* the number */
.sep   { }   /* " / " between paired values */

/* Individual values */
#hp, #maxHp, #fp, #maxFp, #stamina, #maxStamina,
#souls, #soulsTotal, #soulLevel,
#deaths, #trueDeaths, #playTime,
#ngPlus, #archetype, #covenant { }
```

**Example — deaths counter only, large red text:**
```css
body { margin: 0; }
#stat-deaths { font-size: 48px; font-weight: bold; color: #cc0000; }
.label { display: none; }
```

---

## WebSocket API

Connect to `ws://<host>:<port>/ws` to receive JSON on every stat change:

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
  "diseaseResist": 57, "curseResist": 47,
  "ngPlus": 1, "archetype": 3, "covenant": 2
}
```

- Data is pushed **only when a value changes** (no polling noise)
- On connect, the current state is sent immediately
- Reconnection is handled automatically by the overlay page (3 s retry)
- `playTime` is raw seconds in JSON; the overlay page formats it as `H:MM:SS`

---

## Developer Documentation

For build instructions, project structure, adding new stats, and technical details, see [DEVELOPER.md](DEVELOPER.md).

---

## Troubleshooting

| Problem | Likely cause |
|---------|-------------|
| Game doesn't start | Wrong architecture — make sure you built **x64** |
| Stats show `-` | Game not fully loaded yet — the mod waits 5 s on startup |
| WebSocket won't connect | Port in use; check with `netstat -an \| findstr 3000` |
| Stats stuck / wrong | Load a save game first; stats are only valid in-game |
| Can't connect from another PC | Check Windows Firewall allows the port; verify correct IP |

---

## License & credits

- Pointer map sourced from **[Phokz's Dark Souls Remastered Cheat Tables](https://www.nexusmods.com/darksoulsremastered/mods/798)** (Nexus Mods #798)
- Not affiliated with FromSoftware or Bandai Namco
- For educational and personal streaming use only
- **Use at your own risk** online (read-only, but any mod carries risk)
