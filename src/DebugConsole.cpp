// DebugConsole.cpp
// Implements the logging system: optional console window + dstracker.log.
// Each log line is prefixed with a local timestamp [HH:MM:SS.mmm].

#include "../include/DebugConsole.h"
#include <windows.h>
#include <stdio.h>
#include <fstream>
#include <iostream>

bool DebugConsole::initialized = false;
FILE* DebugConsole::stdoutFile = nullptr;
FILE* DebugConsole::stderrFile = nullptr;
FILE* DebugConsole::stdinFile = nullptr;
static std::ofstream logFile;

// Returns "[HH:MM:SS.mmm] " using the system local time
static std::string Timestamp() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    char buf[20];
    sprintf_s(buf, "[%02d:%02d:%02d.%03d] ",
              t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    return buf;
}

void DebugConsole::Initialize() {
    if (initialized) return;

#if ENABLE_DEBUG_CONSOLE
    if (!AllocConsole()) {
        // Console might already exist
    }

    freopen_s(&stdoutFile, "CONOUT$", "w", stdout);
    freopen_s(&stderrFile, "CONOUT$", "w", stderr);
    freopen_s(&stdinFile,  "CONIN$",  "r", stdin);

    SetConsoleTitleA("Dark Souls Tracker - Debug Console");

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
#endif

    logFile.open("dstracker.log", std::ios::out | std::ios::app);
    if (logFile.is_open()) {
        std::string ts = Timestamp();
        logFile << "========================================\n"
                << ts << "Session Started\n"
                << "========================================\n";
        logFile.flush();
    }

    initialized = true;
}

void DebugConsole::Cleanup() {
    if (!initialized) return;

    if (logFile.is_open()) {
        std::string ts = Timestamp();
        logFile << "========================================\n"
                << ts << "Session Ended\n"
                << "========================================\n";
        logFile.close();
    }

#if ENABLE_DEBUG_CONSOLE
    if (stdoutFile) fclose(stdoutFile);
    if (stderrFile) fclose(stderrFile);
    if (stdinFile)  fclose(stdinFile);
    FreeConsole();
#endif

    initialized = false;
}

void DebugConsole::Log(const char* message) {
    std::string line = Timestamp() + message;

#if ENABLE_DEBUG_CONSOLE
    std::cout << line << std::endl;
#endif

    if (logFile.is_open()) {
        logFile << line << '\n';
        logFile.flush();
    }
}

void DebugConsole::Log(const std::string& message) {
    Log(message.c_str());
}
