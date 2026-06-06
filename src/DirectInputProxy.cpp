// DirectInputProxy.cpp
// Exports DirectInput8Create and forwards every call to the real system
// dinput8.dll loaded by dllmain.cpp. This keeps controller input working
// normally while the mod runs alongside the game.

#include <windows.h>

// Function pointer to the original DirectInput8Create
typedef HRESULT(WINAPI* DirectInput8CreateFunc)(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, void* punkOuter);
extern DirectInput8CreateFunc OriginalDirectInput8Create;

// Exported function - DirectInput8Create
extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, void* punkOuter) {
    if (OriginalDirectInput8Create) {
        return OriginalDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
    }
    return 0x80040154; // DIERR_NOTINITIALIZED
}
