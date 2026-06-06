#pragma once

// StatRegistry.h
// Central registry for all player stats. Drives JSON serialization, HTML
// overlay generation, and the operator== comparison — adding a new stat
// is just one entry here plus the read in MemoryReader::ReadPlayerStats().

#include "MemoryReader.h"
#include <cstddef>

// Maps a JSON key to the int32_t field inside PlayerStats via offsetof.
struct JsonField {
    const char* key;
    size_t      offset;
};

// Display metadata for the overlay page.
struct DisplayStat {
    const char* key;       // URL param / element id
    const char* label;     // Human-readable label
    const char* pairedKey; // Second value key (e.g. "maxHp"), or nullptr
};

using PS = MemoryReader::PlayerStats;

// Every int32_t field serialized into the WebSocket JSON.
static const JsonField JSON_FIELDS[] = {
    // Vitals
    {"hp",         offsetof(PS, hp)},
    {"maxHp",      offsetof(PS, maxHp)},
    {"fp",         offsetof(PS, fp)},
    {"maxFp",      offsetof(PS, maxFp)},
    {"stamina",    offsetof(PS, stamina)},
    {"maxStamina", offsetof(PS, maxStamina)},
    // Resources
    {"souls",      offsetof(PS, souls)},
    {"soulsTotal", offsetof(PS, soulsTotal)},
    {"soulLevel",  offsetof(PS, soulLevel)},
    // Attributes
    {"vit",        offsetof(PS, vit)},
    {"atn",        offsetof(PS, atn)},
    {"end",        offsetof(PS, end)},
    {"str",        offsetof(PS, str)},
    {"dex",        offsetof(PS, dex)},
    {"res",        offsetof(PS, res)},
    {"int",        offsetof(PS, intl)},
    {"fth",        offsetof(PS, fth)},
    // Resistances
    {"poisonResist",  offsetof(PS, poisonResist)},
    {"bleedResist",   offsetof(PS, bleedResist)},
    {"diseaseResist", offsetof(PS, diseaseResist)},
    {"curseResist",   offsetof(PS, curseResist)},
    // Counters
    {"deaths",     offsetof(PS, deaths)},
    {"trueDeaths", offsetof(PS, trueDeaths)},
    {"playTime",   offsetof(PS, playTime)},
    // Game data
    {"ngPlus",     offsetof(PS, ngPlus)},
    {"archetype",  offsetof(PS, archetype)},
    {"covenant",   offsetof(PS, covenant)},
};
static const size_t JSON_FIELDS_COUNT = sizeof(JSON_FIELDS) / sizeof(JSON_FIELDS[0]);

// How each stat token renders in the overlay.
// Paired stats show as "Label: X / Y", simple stats as "Label: X".
static const DisplayStat DISPLAY_STATS[] = {
    {"hp",             "HP",           "maxHp"},
    {"fp",             "FP",           "maxFp"},
    {"stamina",        "Stamina",      "maxStamina"},
    {"souls",          "Souls",        nullptr},
    {"soulsTotal",     "Total Souls",  nullptr},
    {"soulLevel",      "Soul Level",   nullptr},
    {"vit",            "Vitality",     nullptr},
    {"atn",            "Attunement",   nullptr},
    {"end",            "Endurance",    nullptr},
    {"str",            "Strength",     nullptr},
    {"dex",            "Dexterity",    nullptr},
    {"res",            "Resistance",   nullptr},
    {"int",            "Intelligence", nullptr},
    {"fth",            "Faith",        nullptr},
    {"poisonResist",   "Poison",       nullptr},
    {"bleedResist",    "Bleed",        nullptr},
    {"diseaseResist",  "Disease",      nullptr},
    {"curseResist",    "Curse",        nullptr},
    {"deaths",         "Deaths",       nullptr},
    {"trueDeaths",     "True Deaths",  nullptr},
    {"playTime",       "Play Time",    nullptr},
    {"ngPlus",         "NG+",          nullptr},
    {"archetype",      "Class",        nullptr},
    {"covenant",       "Covenant",     nullptr},
};
static const size_t DISPLAY_STATS_COUNT = sizeof(DISPLAY_STATS) / sizeof(DISPLAY_STATS[0]);

// Shown when no ?stat= params are specified.
static const char* DEFAULT_DISPLAY[] = {
    "hp", "fp", "stamina", "souls", "soulLevel", "deaths"
};
static const size_t DEFAULT_DISPLAY_COUNT = sizeof(DEFAULT_DISPLAY) / sizeof(DEFAULT_DISPLAY[0]);
