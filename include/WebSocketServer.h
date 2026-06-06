#pragma once

// WebSocketServer.h
// Minimal HTTP + WebSocket server (configurable port, default 3000).
//
// HTTP endpoints:
//   GET /          -> overlay page (stat selection handled client-side via ?stat=)
//
// WebSocket:
//   ws://<host>:<port>/ws  -> receives JSON on every stat change
//
// The server pushes the current state to each new client immediately on connect.
// All broadcasting is guarded by a single mutex (clientsMutex).

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOGDI
#define NOGDI
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include "MemoryReader.h"

#pragma comment(lib, "ws2_32.lib")

class WebSocketServer {
public:
    static constexpr int MAX_CLIENTS = 20;

    explicit WebSocketServer(int port = 3000);
    ~WebSocketServer();

    bool Start();
    void Stop();
    void BroadcastStats(const MemoryReader::PlayerStats& stats);

private:
    SOCKET serverSocket;
    int    port;
    std::thread          serverThread;
    std::atomic<bool>    running;
    std::mutex           clientsMutex;
    std::vector<SOCKET>  clients;
    std::atomic<int>     activeHandlers{0};
    MemoryReader::PlayerStats lastStats{}; // last known valid state, guarded by clientsMutex

    void ServerLoop();
    void HandleClient(SOCKET clientSocket);
    std::string GenerateHandshakeResponse(const std::string& clientKey);
    std::string CreateWebSocketFrame(const std::string& data);
    std::string StatsToJson(const MemoryReader::PlayerStats& stats);
    std::string GetHTMLPage();
    std::string HttpResponse(const std::string& contentType, const std::string& body);

    static std::string ParsePath(const std::string& request);
};
