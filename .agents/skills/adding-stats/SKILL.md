---
name: adding-stats
description: Step-by-step workflow for exposing a new game stat through the WebSocket overlay.
---

# Adding a New Stat

Follow these steps in order:

1. **Find the offset** in `POINTER_MAP.md`.
   The "Not yet implemented" section lists available stats with their offset chains and types.

2. **Add a field** to `PlayerStats` in `include/MemoryReader.h`.
   Follow the existing naming convention (camelCase). Place it **before** the `valid` field.
   The `operator==` uses `memcmp` up to `valid`, so new fields are included automatically.

3. **Read it** in `MemoryReader::ReadPlayerStats()` in `src/MemoryReader.cpp`.
   Use `ReadInt()` for 4-byte values or `ReadByte()` for single-byte values (widens to `int32_t`).

4. **Register it** in `include/StatRegistry.h`:
   - Add a `JsonField` entry in `JSON_FIELDS[]` (drives JSON serialization).
   - Add a `DisplayStat` entry in `DISPLAY_STATS[]` (drives overlay rendering).
     Set `pairedKey` for paired stats (e.g. `"maxHp"` for `"hp"`), or `nullptr` for simple stats.

5. **Rebuild** and test with the game running.

> No manual HTML, JSON serialization, or `operator==` changes are needed — the data-driven
> registry in `StatRegistry.h` handles all of that automatically.

## Checklist for the PR

- [ ] Field added to `PlayerStats` struct (before `valid`)
- [ ] Memory read added in `ReadPlayerStats()`
- [ ] `JsonField` entry added in `StatRegistry.h`
- [ ] `DisplayStat` entry added in `StatRegistry.h`
- [ ] README stats table updated with the new stat
- [ ] `POINTER_MAP.md` entry moved from "Not yet implemented" to "Currently implemented"
