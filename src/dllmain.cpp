// dllmain.cpp
// DLL entry point and main game loop.
//
// Loaded as a dinput8.dll proxy: the real system dinput8.dll is loaded and its
// DirectInput8Create forwarded transparently. A background thread initializes
// the MemoryReader and WebSocketServer once the game has finished loading.

#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_

#include <winsock2.h>
#include <windows.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include "../include/MemoryReader.h"
#include "../include/WebSocketServer.h"
#include "../include/DebugConsole.h"

// ---------------------------------------------------------------------------
// Configuration constants
// ---------------------------------------------------------------------------
static constexpr int  GAME_LOAD_DELAY_SEC = 5;   // wait for game to finish loading
static constexpr int  STATS_POLL_MS       = 200; // how often to poll memory (ms)
static constexpr int  WEBSOCKET_PORT      = 3000;

// ---------------------------------------------------------------------------
// Module-level state
// ---------------------------------------------------------------------------
static HMODULE                       hOriginalDLL = nullptr;
static std::unique_ptr<MemoryReader>    memReader;
static std::unique_ptr<WebSocketServer> wsServer;
static std::thread                   gameThread;
static std::atomic<bool>             gameRunning(false);

typedef HRESULT(WINAPI* DI8CreateFn)(HINSTANCE, DWORD, REFIID, LPVOID*, void*);
DI8CreateFn OriginalDirectInput8Create = nullptr;

// ---------------------------------------------------------------------------
// GameLoop — runs on a dedicated thread.
// Waits for the game to load, then polls memory every STATS_POLL_MS and
// broadcasts stats via WebSocket only when they change.
// ---------------------------------------------------------------------------
static void GameLoop() {
    char msg[128];
    sprintf_s(msg, "[GameLoop] Waiting %ds for game to load...", GAME_LOAD_DELAY_SEC);
    DebugConsole::Log(msg);
    std::this_thread::sleep_for(std::chrono::seconds(GAME_LOAD_DELAY_SEC));

    memReader = std::make_unique<MemoryReader>();
    if (!memReader->Initialize()) {
        DebugConsole::Log("[GameLoop] ERROR: MemoryReader init failed");
        return;
    }

    wsServer = std::make_unique<WebSocketServer>(WEBSOCKET_PORT);
    if (!wsServer->Start()) {
        DebugConsole::Log("[GameLoop] ERROR: WebSocket server failed to start");
        return;
    }

    DebugConsole::Log("[GameLoop] Running...");
    MemoryReader::PlayerStats lastStats{};
    while (gameRunning) {
        auto stats = memReader->ReadPlayerStats();
        if (stats.valid && stats != lastStats) {
            wsServer->BroadcastStats(stats);
            sprintf_s(msg, "[GameLoop] HP:%d/%d Deaths:%d Souls:%d",
                      stats.hp, stats.maxHp, stats.deaths, stats.souls);
            DebugConsole::Log(msg);
            lastStats = stats;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(STATS_POLL_MS));
    }

    DebugConsole::Log("[GameLoop] Stopped");
}

// ---------------------------------------------------------------------------
// DllMain
// ---------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        DebugConsole::Initialize();
        DebugConsole::Log("[DLL] Loading...");

        {
            // Load the real system dinput8.dll so DirectInput keeps working
            char sysPath[MAX_PATH];
            GetSystemDirectoryA(sysPath, MAX_PATH);
            strcat_s(sysPath, "\\dinput8.dll");
            hOriginalDLL = LoadLibraryA(sysPath);
            if (!hOriginalDLL) {
                DebugConsole::Log("[DLL] ERROR: Failed to load original dinput8.dll");
                return FALSE;
            }
            OriginalDirectInput8Create = (DI8CreateFn)GetProcAddress(hOriginalDLL, "DirectInput8Create");
            if (!OriginalDirectInput8Create) {
                DebugConsole::Log("[DLL] ERROR: DirectInput8Create not found");
                return FALSE;
            }
        }

        gameRunning = true;
        gameThread  = std::thread(GameLoop);
        DebugConsole::Log("[DLL] Loaded OK");
        break;

    case DLL_PROCESS_DETACH:
        DebugConsole::Log("[DLL] Unloading...");
        gameRunning = false;

        if (gameThread.joinable())
            gameThread.join();

        // unique_ptr destructors handle cleanup; explicit stop needed for wsServer
        if (wsServer)  wsServer->Stop();
        wsServer.reset();
        memReader.reset();

        if (hOriginalDLL) { FreeLibrary(hOriginalDLL); hOriginalDLL = nullptr; }

        DebugConsole::Cleanup();
        break;
    }
    return TRUE;
}
