# Pointer Map — Dark Souls Remastered

> All offsets sourced from [Phokz's Dark Souls Remastered Cheat Tables](https://www.nexusmods.com/darksoulsremastered/mods/798) (Nexus Mods #798).  
> The `.CT` file is included in this repo for reference only — all credit for the pointer research goes to the original author.

All values are hexadecimal unless noted otherwise.

---

## BaseB — how it is found

```
Pattern (GetB):  48 8B 05 ?? ?? ?? ?? 45 33 ED 48 8B F1 48 85 C0
Resolution:      baseBPtr = patternAddress + *(int32)(patternAddress+3) + 7
                 BaseB    = *(baseBPtr)          ← the actual game object
```

---

## Notation

```
[BaseB]           = *baseBPtr              (one dereference)
[BaseB+0x10]      = *(BaseB + 0x10)        (two dereferences total)
[[BaseB+0x10]+X]  = *(*(BaseB+0x10) + X)   (three dereferences)
```

CE Cheat Table offsets are listed **bottom-up** (last offset = closest to BaseB).  
This document lists them **top-down** for readability.

---

## Currently implemented

These stats are read by `MemoryReader::ReadPlayerStats()` and exposed via WebSocket.

### Vitals — via `ChrStat = *(BaseB + 0x10)`

| Field | Offset chain | Type | CT Description |
|-------|-------------|------|----------------|
| `hp` | `[ChrStat + 0x14]` | int32 | "Hp" |
| `maxHp` | `[ChrStat + 0x1C]` | int32 | "MaxHP" |
| `fp` | `[ChrStat + 0x20]` | int32 | "Mp" |
| `maxFp` | `[ChrStat + 0x24]` | int32 | "MaxMP" |
| `stamina` | `[ChrStat + 0x30]` | int32 | "Sp" |
| `maxStamina` | `[ChrStat + 0x34]` | int32 | "MaxSp" |

### Resources — via ChrStat

| Field | Offset chain | Type | CT Description |
|-------|-------------|------|----------------|
| `souls` | `[ChrStat + 0x94]` | int32 | "Soul" |
| `soulsTotal` | `[ChrStat + 0x98]` | int32 | "Total Get Soul" |
| `soulLevel` | `[ChrStat + 0x90]` | int32 | "SoulLv" |

### Attributes — via ChrStat

| Field | Offset chain | Type | CT Description |
|-------|-------------|------|----------------|
| `vit` | `[ChrStat + 0x40]` | int32 | "VIT" |
| `atn` | `[ChrStat + 0x48]` | int32 | "ATN" |
| `end` | `[ChrStat + 0x50]` | int32 | "END" |
| `str` | `[ChrStat + 0x58]` | int32 | "STR" |
| `dex` | `[ChrStat + 0x60]` | int32 | "DEX" |
| `res` | `[ChrStat + 0x88]` | int32 | "RES" |
| `int` | `[ChrStat + 0x68]` | int32 | "INT" |
| `fth` | `[ChrStat + 0x70]` | int32 | "FTH" |

### Resistances — via ChrStat

| Field | Offset chain | Type | CT Description |
|-------|-------------|------|----------------|
| `poisonResist` | `[ChrStat + 0x100]` | int32 | "Poison Resist" |
| `bleedResist` | `[ChrStat + 0x104]` | int32 | "Blood Resist" |
| `diseaseResist` | `[ChrStat + 0x108]` | int32 | "Disease Resist" |
| `curseResist` | `[ChrStat + 0x10C]` | int32 | "Curse Resist" |

### Game counters — direct from BaseB

| Field | Offset chain | Type | CT Description |
|-------|-------------|------|----------------|
| `deaths` | `[BaseB + 0x98]` | int32 | "Death Num" |
| `trueDeaths` | `[BaseB + 0x94]` | int32 | "True Death Num" |
| `playTime` | `[BaseB + 0xA4]` | int32 | "Play Time" (seconds) |

---

## Not yet implemented — available for extension

Add any of these by following the steps in [README.md § Adding new stats](README.md#adding-new-stats).

### Player game data (direct from BaseB)

| CT Description | Offset | Type | Notes |
|----------------|--------|------|-------|
| "ClearCount" | `[BaseB + 0x78]` | byte | NG+ count |
| "ClearState" | `[BaseB + 0x7C]` | byte | 0=none, 1=good, 2=bad |
| "Full Recover" | `[BaseB + 0x80]` | int32 | |
| "TrueDeath" | `[BaseB + 0x90]` | int32 | |
| "Resurrection counter [0]" | `[BaseB + 0xC4]` | int32 | |
| "Resurrection counter [1]" | `[BaseB + 0xC8]` | int32 | |
| "Resurrection counter [2]" | `[BaseB + 0xCC]` | int32 | |

### Player param — via ChrStat

| CT Description | Offset | Type | Notes |
|----------------|--------|------|-------|
| "ArcheType (Class)" | `[ChrStat + 0xCE]` | byte | Starting class ID |
| "Vow_Type (covenant)" | `[ChrStat + 0x113]` | byte | Active covenant |
| "CurseLv" | `[ChrStat + 0x117]` | byte | Curse level |
| "BaseMaxHP" | `[ChrStat + 0x18]` | int32 | Base HP before bonuses |
| "Gender" | `[ChrStat + 0xCA]` | byte | |

### Multiplayer stats — via ChrStat

| CT Description | Offset | Type |
|----------------|--------|------|
| "MultiPlay Count" | `[ChrStat + 0xD4]` | byte |
| "CoopPlaySuccess Count" | `[ChrStat + 0xD8]` | byte |
| "ThiefInvadePlaySuccess Count" | `[ChrStat + 0xDC]` | byte |
| "Player Rank S" | `[ChrStat + 0xE0]` | byte |
| "Player Rank A" | `[ChrStat + 0xE4]` | byte |
| "Player Rank C" | `[ChrStat + 0xE8]` | byte |

### Appearance — via ChrStat

| CT Description | Offset | Type |
|----------------|--------|------|
| "Face" | `[ChrStat + 0x114]` | byte |
| "Physique" | `[ChrStat + 0xCF]` | byte |
| "Hair" | `[ChrStat + 0x115]` | byte |
| "Hair Color - Red" | `[ChrStat + 0x4C0]` | float |
| "Hair Color - Green" | `[ChrStat + 0x4C4]` | float |
| "Hair Color - Blue" | `[ChrStat + 0x4C8]` | float |

### Gestures — via `*(*(BaseB + 0x10) + 0x568)`

| CT Description | Inner offset | Type |
|----------------|-------------|------|
| "Point Forward" | `+0x10` | byte |
| "Point Up" | `+0x14` | byte |
| "Beckon" | `+0x1C` | byte |
| "Wave" | `+0x20` | byte |
| "Bow" | `+0x24` | byte |
| "Praise The Sun" | `+0x48` | byte |

### Equipped gestures — via `*(*(BaseB + 0x10) + 0x450)`

Slots 1–7 at offsets `+0x10` through `+0x28` (step 4), type byte.

### Attunement slots — via `*(*(BaseB + 0x10) + 0x418)`

Slots 1–12 at offsets `+0x18`, `+0x20`, `+0x28` … `+0x70` (step 8), type int32.

---

## Other base pointers (from CT — not used here)

These are separate global pointers used by the CT for other features.  
Listed for reference if you want to extend the tracker beyond BaseB data.

| CT Symbol | AOB Pattern |
|-----------|------------|
| `BaseA` | `48 89 05 ?? ?? ?? ?? 8D 42` |
| `BaseC` | `48 8B 05 ?? ?? ?? ?? 0F 28 01 66 0F 7F 80 ?? ?? 00 00 C6 80` |
| `BaseD` | `48 8B 05 ?? ?? ?? ?? 80 B8 ?? 00 00 00 00 0F 84 ?? ?? ?? ?? 8B 51 24 48` |
| `BaseE` | `48 8B 05 ?? ?? ?? ?? 48 8B 88 98 0B 00 00 8B 41 3C C3` |
| `GetP` | `4C 8B 05 ?? ?? ?? ?? 48 63 C9 48 8D 04 C9` |
| `GetX` | `48 8B 05 ?? ?? ?? ?? 48 39 48 68 0F 94 C0 C3` |
| `GetZ` | `48 8B 05 ?? ?? ?? ?? FF 40 1C 48 8B C3 4D 85 E4` |
