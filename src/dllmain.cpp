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
static constexpr int  GAME_LOAD_DELAY_SEC  = 5;    // wait for game to finish loading
static constexpr int  STATS_POLL_MS        = 200;   // how often to poll memory (ms)
static constexpr int  DEFAULT_PORT         = 3000;
static constexpr int  MAX_RESCAN_FAILURES  = 50;    // consecutive failures before re-scanning BaseB

// ---------------------------------------------------------------------------
// Module-level state
// ---------------------------------------------------------------------------
static HMODULE                          hModule     = nullptr;
static HMODULE                          hOriginalDLL = nullptr;
static std::unique_ptr<MemoryReader>    memReader;
static std::unique_ptr<WebSocketServer> wsServer;
static std::thread                      gameThread;
static std::atomic<bool>                gameRunning(false);
static HANDLE                           shutdownEvent = NULL;

typedef HRESULT(WINAPI* DI8CreateFn)(HINSTANCE, DWORD, REFIID, LPVOID*, void*);
DI8CreateFn OriginalDirectInput8Create = nullptr;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Reads [Server] Port from dstracker.ini next to the DLL.
// Returns DEFAULT_PORT if the file or key is absent.
static int LoadPort() {
    char dllPath[MAX_PATH];
    if (!GetModuleFileNameA(hModule, dllPath, MAX_PATH)) return DEFAULT_PORT;

    char* lastSlash = strrchr(dllPath, '\\');
    if (lastSlash)
        strcpy_s(lastSlash + 1, MAX_PATH - static_cast<size_t>(lastSlash - dllPath + 1), "dstracker.ini");
    else
        return DEFAULT_PORT;

    return static_cast<int>(GetPrivateProfileIntA("Server", "Port", DEFAULT_PORT, dllPath));
}

// ---------------------------------------------------------------------------
// GameLoop — runs on a dedicated thread.
// Uses shutdownEvent for interruptible waits so DLL_PROCESS_DETACH is fast.
// ---------------------------------------------------------------------------
static void GameLoop() {
    char msg[128];
    sprintf_s(msg, "[GameLoop] Waiting %ds for game to load...", GAME_LOAD_DELAY_SEC);
    DebugConsole::Log(msg);

    // Interruptible sleep — returns immediately if shutdownEvent is signaled
    WaitForSingleObject(shutdownEvent, GAME_LOAD_DELAY_SEC * 1000);
    if (!gameRunning) return;

    memReader = std::make_unique<MemoryReader>();
    if (!memReader->Initialize()) {
        DebugConsole::Log("[GameLoop] ERROR: MemoryReader init failed");
        return;
    }

    int port = LoadPort();
    sprintf_s(msg, "[GameLoop] Using port %d", port);
    DebugConsole::Log(msg);

    wsServer = std::make_unique<WebSocketServer>(port);
    if (!wsServer->Start()) {
        DebugConsole::Log("[GameLoop] ERROR: WebSocket server failed to start");
        return;
    }

    DebugConsole::Log("[GameLoop] Running...");
    MemoryReader::PlayerStats lastStats{};
    int consecutiveFailures = 0;

    while (gameRunning) {
        auto stats = memReader->ReadPlayerStats();

        if (stats.valid) {
            consecutiveFailures = 0;
            if (stats != lastStats) {
                wsServer->BroadcastStats(stats);
                lastStats = stats;
            }
        } else {
            consecutiveFailures++;
            if (consecutiveFailures >= MAX_RESCAN_FAILURES) {
                DebugConsole::Log("[GameLoop] Too many read failures, re-scanning BaseB...");
                if (memReader->Initialize()) {
                    DebugConsole::Log("[GameLoop] Re-scan succeeded");
                    consecutiveFailures = 0;
                } else {
                    DebugConsole::Log("[GameLoop] Re-scan failed, will retry later");
                    consecutiveFailures = 0; // reset to avoid spamming
                }
            }
        }

        // Interruptible poll delay
        WaitForSingleObject(shutdownEvent, STATS_POLL_MS);
    }

    DebugConsole::Log("[GameLoop] Stopped");
}

// ---------------------------------------------------------------------------
// DllMain
// ---------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hMod);
        hModule = hMod;
        DebugConsole::Initialize();
        DebugConsole::Log("[DLL] Loading...");

        shutdownEvent = CreateEventA(NULL, TRUE, FALSE, NULL); // manual-reset

        {
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

        // Wake up GameLoop from any WaitForSingleObject immediately
        if (shutdownEvent) SetEvent(shutdownEvent);

        if (gameThread.joinable())
            gameThread.join();

        if (wsServer)  wsServer->Stop();
        wsServer.reset();
        memReader.reset();

        if (hOriginalDLL) { FreeLibrary(hOriginalDLL); hOriginalDLL = nullptr; }
        if (shutdownEvent) { CloseHandle(shutdownEvent); shutdownEvent = NULL; }

        DebugConsole::Cleanup();
        break;
    }
    return TRUE;
}
