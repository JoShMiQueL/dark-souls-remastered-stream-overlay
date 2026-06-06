---
name: building
description: How to build the DarkSoulsTracker DLL using MSBuild on Windows.
---

# Building

## Requirements

- Windows 10/11 x64
- Visual Studio 2026 with C++ Desktop workload

## Build (command line)

```powershell
.\scripts\build.ps1
```

Or directly with MSBuild:

```powershell
msbuild DarkSoulsTracker.vcxproj `
  /p:Configuration=Release `
  /p:Platform=x64 `
  /p:OutDir=build\Release\ `
  /p:IntDir=build\Release\obj\ `
  /v:minimal /nologo
```

## Output

`build/Release/dinput8.dll` — copy this to the Dark Souls Remastered game folder.

## CI

GitHub Actions runs the same MSBuild command on `windows-2025-vs2026`.
See `.github/workflows/build.yml`.

## Debug build

Open `DarkSoulsTracker.vcxproj` in Visual Studio, select **Debug | x64**, and build.
To enable the debug console window, set `ENABLE_DEBUG_CONSOLE` to `1` in `include/DebugConsole.h`.
