# Bug Fix Examples

## Critical Fixes
- **Memory leak fix** — Fixed memory leak in WebSocket connection handling
- **Crash prevention** — Added null checks to prevent crashes on invalid data
- **Data corruption** — Fixed issue where stat values could become corrupted

## WebSocket & Template Processing
- **Escape sequence order** — Fixed processing order (backslash must be first to avoid partial matches)
- **Variable name regex** — Relaxed regex to allow underscores in stat names
- **URL decoding** — Fixed to handle both keys and values properly

## Build & Installation
- **Build script error handling** — Added error handling to PowerShell build script
- **Dependency resolution** — Fixed issue with missing dependencies during build
- **Installation path** — Fixed incorrect default installation path