# AGENTS.md

## Project Overview

Windows DLL mod for Dark Souls Remastered. Reads game memory (read-only) and exposes player stats over WebSocket (port 3000) for OBS stream overlays.

## Build

- **Toolchain:** Visual Studio 2026 — MSBuild, C++17, x64 only
- **Build command:** `msbuild DarkSoulsTracker.vcxproj /p:Configuration=Release /p:Platform=x64 /p:OutDir=build\Release\ /p:IntDir=build\Release\obj\`
- **CI:** GitHub Actions on `windows-2025-vs2026` (see `.github/workflows/build.yml`)
- **Output:** `build/Release/dinput8.dll`

## Code Style

- C++17 (`stdcpp17` in vcxproj)
- Warning level 3 (`/W3`), SDL checks enabled
- `WIN32_LEAN_AND_MEAN` always defined
- Include winsock2.h before windows.h to avoid redefinition warnings
- Use `sprintf_s` and other secure CRT functions (MSVC)
- `#pragma comment(lib, ...)` for link dependencies

## Project Structure

- `src/` — implementation (.cpp)
- `include/` — headers (.h)
- `scripts/` — build automation (PowerShell)
- `POINTER_MAP.md` — memory offset reference from Cheat Engine tables
- `DarkSoulsTracker.vcxproj` — the single VS project file

## Rules

- **Never write to game memory.** This mod is strictly read-only.
- Do not add external dependencies — the project uses only Win32 APIs and the C++ standard library.
- Do not modify `.vcxproj` without good reason; it is the canonical build definition.
- Keep the WebSocket server minimal — no frameworks, no npm, just raw WinSock.
- All new stats must follow the 5-step workflow documented in README.md § Adding new stats.

## Testing

No automated tests. The DLL requires Dark Souls Remastered running on Windows to test.

## No Linter

Compiler warnings (`/W3 + /WX-`) are the quality gate. No external linter is configured.
