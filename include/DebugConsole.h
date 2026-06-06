#pragma once

// DebugConsole.h
// Optional debug console window and persistent log file (dstracker.log).
// The log file is always written regardless of ENABLE_DEBUG_CONSOLE.
// Set ENABLE_DEBUG_CONSOLE to 1 to also open a console window — useful
// during development to see log output in real time.

#include <string>

#define ENABLE_DEBUG_CONSOLE 0 // set to 1 for development builds

class DebugConsole {
public:
    // Opens the log file and (if enabled) allocates a console window.
    static void Initialize();

    // Flushes and closes the log file, frees the console window if open.
    static void Cleanup();

    // Writes a timestamped line to the log file and (if enabled) the console.
    static void Log(const char* message);
    static void Log(const std::string& message);

private:
    static bool  initialized;
    static FILE* stdoutFile;
    static FILE* stderrFile;
    static FILE* stdinFile;
};
