---
name: adding-stats
description: Step-by-step workflow for exposing a new game stat through the WebSocket overlay.
---

# Adding a New Stat

Follow these steps in order:

1. **Find the offset** in `POINTER_MAP.md`.
   The "Not yet implemented" section lists available stats with their offset chains and types.

2. **Add a field** to `PlayerStats` in `include/MemoryReader.h`.
   Follow the existing naming convention (camelCase). Add it in the correct section
   (Vitals, Resources, Attributes, etc.). Update `operator==` to include the new field.

3. **Read it** in `MemoryReader::ReadPlayerStats()` in `src/MemoryReader.cpp`.
   Use the `ReadInt()` helper with the correct pointer chain offset.

4. **Serialize it** in `WebSocketServer::StatsToJson()` in `src/WebSocketServer.cpp`.
   Add a JSON key matching the field name.

5. **Add an HTML block** in `WebSocketServer::StatBlock()` in `src/WebSocketServer.cpp`.
   Follow the existing pattern for the stat type (single value, paired value, etc.).
   Add the corresponding URL param token to `validStats`.

6. **Rebuild** and test with the game running.

## Checklist for the PR

- [ ] Field added to `PlayerStats` struct
- [ ] `operator==` updated
- [ ] Memory read added in `ReadPlayerStats()`
- [ ] JSON serialization added in `StatsToJson()`
- [ ] HTML block added in `StatBlock()`
- [ ] `validStats` updated with the URL param token
- [ ] README table updated with the new stat
- [ ] `POINTER_MAP.md` entry moved from "Not yet implemented" to "Currently implemented"
