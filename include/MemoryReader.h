#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <type_traits>

// ---------------------------------------------------------------------------
// PlayerStats
// All fields sourced from Phokz's Cheat Table (All_DarkSoulsRemastered_CheatTables.CT).
// Pointer chains are relative to BaseB (see POINTER_MAP.md for the full reference).
//
// To add a new stat:
//   1. Find the offset chain in POINTER_MAP.md
//   2. Add the field here (before 'valid')
//   3. Read it in MemoryReader::ReadPlayerStats()
//   4. Add a JsonField entry in StatRegistry.h
//   5. Add a DisplayStat entry in StatRegistry.h
// ---------------------------------------------------------------------------
class MemoryReader {
public:
    struct PlayerStats {
        // --- Vitals (BaseB -> [+0x10] = ChrStat) ---
        int32_t hp         = 0;  // [ChrStat+0x14]
        int32_t maxHp      = 0;  // [ChrStat+0x1C]
        int32_t fp         = 0;  // Focus Points / Mana  [ChrStat+0x20]
        int32_t maxFp      = 0;  // [ChrStat+0x24]
        int32_t stamina    = 0;  // [ChrStat+0x30]
        int32_t maxStamina = 0;  // [ChrStat+0x34]

        // --- Resources (BaseB -> [+0x10] = ChrStat) ---
        int32_t souls      = 0;  // [ChrStat+0x94]
        int32_t soulsTotal = 0;  // Total souls collected ever [ChrStat+0x98]
        int32_t soulLevel  = 0;  // [ChrStat+0x90]

        // --- Attributes (BaseB -> [+0x10] = ChrStat) ---
        int32_t vit        = 0;  // Vitality    [ChrStat+0x40]
        int32_t atn        = 0;  // Attunement  [ChrStat+0x48]
        int32_t end        = 0;  // Endurance   [ChrStat+0x50]
        int32_t str        = 0;  // Strength    [ChrStat+0x58]
        int32_t dex        = 0;  // Dexterity   [ChrStat+0x60]
        int32_t res        = 0;  // Resistance  [ChrStat+0x88]
        int32_t intl       = 0;  // Intelligence [ChrStat+0x68]
        int32_t fth        = 0;  // Faith       [ChrStat+0x70]

        // --- Resistances (BaseB -> [+0x10] = ChrStat) ---
        int32_t poisonResist  = 0; // [ChrStat+0x100]
        int32_t bleedResist   = 0; // [ChrStat+0x104]
        int32_t diseaseResist = 0; // [ChrStat+0x108]
        int32_t curseResist   = 0; // [ChrStat+0x10C]

        // --- Game counters (direct from BaseB) ---
        int32_t deaths        = 0; // Death Num      [BaseB+0x98]
        int32_t trueDeaths    = 0; // True Death Num [BaseB+0x94]
        int32_t playTime      = 0; // seconds        [BaseB+0xA4]

        // --- Game data ---
        int32_t ngPlus        = 0; // NG+ count (byte widened) [BaseB+0x78]
        int32_t archetype     = 0; // Starting class ID (byte) [ChrStat+0xCE]
        int32_t covenant      = 0; // Active covenant   (byte) [ChrStat+0x113]

        // ---- keep 'valid' last — memcmp compares everything above it ----
        bool valid = false;

        static_assert(std::is_standard_layout_v<int32_t>);

        bool operator==(const PlayerStats& o) const {
            return std::memcmp(this, &o, offsetof(PlayerStats, valid)) == 0;
        }
        bool operator!=(const PlayerStats& o) const { return !(*this == o); }
    };

    static_assert(std::is_standard_layout_v<PlayerStats>,
                  "PlayerStats must be standard layout for memcmp comparison");

    MemoryReader();

    bool        Initialize();
    PlayerStats ReadPlayerStats();

private:
    HMODULE   gameModule;
    uintptr_t baseBPtr; // address of the global pointer variable holding the BaseB struct

    uintptr_t FindBaseBPtr();
};
