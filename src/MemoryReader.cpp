#include "../include/MemoryReader.h"
#include "../include/DebugConsole.h"
#include <psapi.h>

MemoryReader::MemoryReader() : gameModule(nullptr), baseBPtr(0) {}

bool MemoryReader::Initialize() {
    DebugConsole::Log("[MemoryReader] Initializing...");

    gameModule = GetModuleHandleA("DarkSoulsRemastered.exe");
    if (!gameModule) {
        DebugConsole::Log("[MemoryReader] ERROR: DarkSoulsRemastered.exe not found");
        return false;
    }

    baseBPtr = FindBaseBPtr();
    if (baseBPtr == 0) {
        DebugConsole::Log("[MemoryReader] ERROR: Failed to find BaseB pointer");
        return false;
    }

    char msg[128];
    sprintf_s(msg, "[MemoryReader] BaseBPtr: 0x%llX", (unsigned long long)baseBPtr);
    DebugConsole::Log(msg);
    return true;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Scans [base, base+size) for a byte pattern.
// mask: 'x' = match exact byte, '?' = wildcard (any byte).
// Returns the address of the first match, or 0 if not found.
static uintptr_t PatternScan(uintptr_t base, size_t size, const uint8_t* pattern, const char* mask) {
    size_t patLen = strlen(mask);
    for (uintptr_t i = 0; i < size - patLen; i++) {
        bool match = true;
        for (size_t j = 0; j < patLen; j++) {
            if (mask[j] == 'x' && reinterpret_cast<uint8_t*>(base + i)[j] != pattern[j]) {
                match = false;
                break;
            }
        }
        if (match) return base + i;
    }
    return 0;
}

// Resolves a RIP-relative MOV instruction to the absolute address it references.
// Layout: [opcode 3B] [int32 disp] → target = addr + disp + 7 (sizeof the instruction)
static uintptr_t ResolveRIP(uintptr_t addr) {
    if (!addr) return 0;
    return addr + *reinterpret_cast<int32_t*>(addr + 3) + 7;
}

// Returns true if ptr points to committed, accessible memory (not PAGE_NOACCESS / PAGE_GUARD).
static bool IsReadable(uintptr_t ptr) {
    if (!ptr) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(reinterpret_cast<LPCVOID>(ptr), &mbi, sizeof(mbi))) return false;
    return mbi.State == MEM_COMMIT &&
           mbi.Protect != PAGE_NOACCESS &&
           mbi.Protect != PAGE_GUARD;
}

// Safe read helpers — return 0 on invalid address
static int32_t ReadInt(uintptr_t ptr) {
    return IsReadable(ptr) ? *reinterpret_cast<int32_t*>(ptr) : 0;
}

static int32_t ReadByte(uintptr_t ptr) {
    return IsReadable(ptr) ? static_cast<int32_t>(*reinterpret_cast<uint8_t*>(ptr)) : 0;
}

// ---------------------------------------------------------------------------
// FindBaseBPtr
// Pattern: GetB = 48 8B 05 xx xx xx xx 45 33 ED 48 8B F1 48 85 C0
// Source: Phokz's Cheat Table (see POINTER_MAP.md)
// ---------------------------------------------------------------------------
uintptr_t MemoryReader::FindBaseBPtr() {
    MODULEINFO modInfo;
    if (!GetModuleInformation(GetCurrentProcess(), gameModule, &modInfo, sizeof(modInfo))) {
        DebugConsole::Log("[MemoryReader] ERROR: GetModuleInformation failed");
        return 0;
    }

    uintptr_t base = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
    size_t    size = modInfo.SizeOfImage;

    {
        char msg[128];
        sprintf_s(msg, "[MemoryReader] Module: 0x%llX  size: 0x%X",
                  (unsigned long long)base, (unsigned)size);
        DebugConsole::Log(msg);
    }

    // Primary pattern (16 bytes)
    static const uint8_t pat[] = {
        0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00,
        0x45, 0x33, 0xED, 0x48, 0x8B, 0xF1, 0x48, 0x85, 0xC0
    };
    static const char mask[] = "xxx????xxxxxxxxx";

    uintptr_t hit = PatternScan(base, size, pat, mask);

    // Fallback: shorter variant (GetR in CT, same RIP calc)
    if (!hit) {
        static const uint8_t fallback[] = {
            0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00,
            0x45, 0x33, 0xED, 0x48, 0x85, 0xC0
        };
        static const char fallbackMask[] = "xxx????xxxxxx";
        hit = PatternScan(base, size, fallback, fallbackMask);
    }

    if (!hit) {
        DebugConsole::Log("[MemoryReader] ERROR: GetB pattern not found");
        return 0;
    }

    uintptr_t ptr = ResolveRIP(hit);
    if (!IsReadable(ptr)) {
        DebugConsole::Log("[MemoryReader] ERROR: resolved BaseBPtr is not readable");
        return 0;
    }

    char msg[128];
    sprintf_s(msg, "[MemoryReader] GetB hit: 0x%llX  BaseBPtr: 0x%llX",
              (unsigned long long)hit, (unsigned long long)ptr);
    DebugConsole::Log(msg);
    return ptr;
}

// ---------------------------------------------------------------------------
// ReadPlayerStats
//
// Pointer chains (CT notation, offsets in hex):
//   BaseB     = *baseBPtr
//   ChrStat   = *(BaseB + 0x10)
//
// Vitals:      ChrStat + 0x14 / 0x1C / 0x20 / 0x24 / 0x30 / 0x34
// Resources:   ChrStat + 0x94 / 0x98 / 0x90
// Attributes:  ChrStat + 0x40 / 0x48 / 0x50 / 0x58 / 0x60 / 0x88 / 0x68 / 0x70
// Resistances: ChrStat + 0x100 / 0x104 / 0x108 / 0x10C
// Counters:    BaseB   + 0x98 / 0x94 / 0xA4
// Game data:   BaseB   + 0x78  |  ChrStat + 0xCE / 0x113
// ---------------------------------------------------------------------------
MemoryReader::PlayerStats MemoryReader::ReadPlayerStats() {
    PlayerStats s{};

    if (!baseBPtr) return s;

    if (!IsReadable(baseBPtr)) return s;
    uintptr_t baseB = *reinterpret_cast<uintptr_t*>(baseBPtr);
    if (!baseB || !IsReadable(baseB)) return s;

    if (!IsReadable(baseB + 0x10)) return s;
    uintptr_t chrStat = *reinterpret_cast<uintptr_t*>(baseB + 0x10);
    if (!chrStat || !IsReadable(chrStat)) return s;

    // Vitals
    s.hp         = ReadInt(chrStat + 0x14);
    s.maxHp      = ReadInt(chrStat + 0x1C);
    s.fp         = ReadInt(chrStat + 0x20);
    s.maxFp      = ReadInt(chrStat + 0x24);
    s.stamina    = ReadInt(chrStat + 0x30);
    s.maxStamina = ReadInt(chrStat + 0x34);

    // Resources
    s.souls      = ReadInt(chrStat + 0x94);
    s.soulsTotal = ReadInt(chrStat + 0x98);
    s.soulLevel  = ReadInt(chrStat + 0x90);

    // Attributes
    s.vit  = ReadInt(chrStat + 0x40);
    s.atn  = ReadInt(chrStat + 0x48);
    s.end  = ReadInt(chrStat + 0x50);
    s.str  = ReadInt(chrStat + 0x58);
    s.dex  = ReadInt(chrStat + 0x60);
    s.res  = ReadInt(chrStat + 0x88);
    s.intl = ReadInt(chrStat + 0x68);
    s.fth  = ReadInt(chrStat + 0x70);

    // Resistances
    s.poisonResist  = ReadInt(chrStat + 0x100);
    s.bleedResist   = ReadInt(chrStat + 0x104);
    s.diseaseResist = ReadInt(chrStat + 0x108);
    s.curseResist   = ReadInt(chrStat + 0x10C);

    // Game counters (direct from BaseB)
    s.deaths     = ReadInt(baseB + 0x98);
    s.trueDeaths = ReadInt(baseB + 0x94);
    s.playTime   = ReadInt(baseB + 0xA4);

    // Game data
    s.ngPlus    = ReadByte(baseB + 0x78);
    s.archetype = ReadByte(chrStat + 0xCE);
    s.covenant  = ReadByte(chrStat + 0x113);

    s.valid = (s.maxHp > 0 && s.maxHp <= 99999 &&
               s.hp >= 0   && s.hp    <= s.maxHp);
    return s;
}
