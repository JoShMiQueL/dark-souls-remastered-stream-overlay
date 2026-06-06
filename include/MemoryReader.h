#pragma once

#include <windows.h>
#include <cstdint>

// ---------------------------------------------------------------------------
// PlayerStats
// All fields sourced from Phokz's Cheat Table (All_DarkSoulsRemastered_CheatTables.CT).
// Pointer chains are relative to BaseB (see POINTER_MAP.md for the full reference).
//
// To add a new stat:
//   1. Find the offset chain in POINTER_MAP.md
//   2. Add the field here
//   3. Read it in MemoryReader::ReadPlayerStats()
//   4. Serialize it in WebSocketServer::StatsToJson()
//   5. Add an HTML block in WebSocketServer::StatBlock()
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

        bool valid = false;

        bool operator==(const PlayerStats& o) const {
            return hp == o.hp && maxHp == o.maxHp &&
                   fp == o.fp && maxFp == o.maxFp &&
                   stamina == o.stamina && maxStamina == o.maxStamina &&
                   souls == o.souls && soulsTotal == o.soulsTotal &&
                   soulLevel == o.soulLevel &&
                   vit == o.vit && atn == o.atn && end == o.end &&
                   str == o.str && dex == o.dex && res == o.res &&
                   intl == o.intl && fth == o.fth &&
                   poisonResist == o.poisonResist && bleedResist == o.bleedResist &&
                   diseaseResist == o.diseaseResist && curseResist == o.curseResist &&
                   deaths == o.deaths && trueDeaths == o.trueDeaths &&
                   playTime == o.playTime;
        }
        bool operator!=(const PlayerStats& o) const { return !(*this == o); }
    };

    MemoryReader();

    bool        Initialize();
    PlayerStats ReadPlayerStats();

private:
    HMODULE   gameModule;
    uintptr_t baseBPtr; // address of the global pointer variable holding the BaseB struct

    uintptr_t FindBaseBPtr();
};
