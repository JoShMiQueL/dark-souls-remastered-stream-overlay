#pragma once

// WebSocketServer.h
// Minimal single-threaded HTTP + WebSocket server (port 3000 by default).
//
// HTTP endpoints:
//   GET /          -> overlay page with all stats
//   GET /?stat=X   -> overlay page with one or more stats (repeatable param,
//                     order is preserved: ?stat=deaths&stat=hp)
//
// WebSocket:
//   ws://localhost:3000/ws  -> receives JSON on every stat change
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
    explicit WebSocketServer(int port = 3000);
    ~WebSocketServer();

    // Starts the server loop on a background thread. Returns false on error.
    bool Start();

    // Signals the server to stop and waits for the thread to exit.
    void Stop();

    // Serializes stats to JSON and broadcasts to all connected WebSocket clients.
    // Also caches stats so new clients receive the current state on connect.
    // No-op if stats.valid is false.
    void BroadcastStats(const MemoryReader::PlayerStats& stats);

private:
    SOCKET serverSocket;
    int    port;
    std::thread          serverThread;
    std::atomic<bool>    running;
    std::mutex           clientsMutex;
    std::vector<SOCKET>  clients;
    MemoryReader::PlayerStats lastStats{}; // last known valid state, guarded by clientsMutex

    // Accepts connections in a loop until running == false.
    void ServerLoop();

    // Handles a single client connection: performs HTTP or WebSocket handshake,
    // then keeps the WebSocket alive until the client disconnects or running == false.
    void HandleClient(SOCKET clientSocket);

    // Generates the HTTP 101 Switching Protocols response for a WebSocket upgrade.
    std::string GenerateHandshakeResponse(const std::string& clientKey);

    // Wraps data in a WebSocket text frame (RFC 6455).
    std::string CreateWebSocketFrame(const std::string& data);

    // Serializes PlayerStats to a flat JSON object.
    std::string StatsToJson(const MemoryReader::PlayerStats& stats);

    // Builds a full HTML page showing the requested stats in order.
    // Empty vector = all stats in default order.
    std::string GetHTMLPage(const std::vector<std::string>& stats);

    // Returns "HTTP/1.1 200 OK\r\n..." with correct Content-Length.
    std::string HttpResponse(const std::string& contentType, const std::string& body);

    // Extracts the path+query string from a raw HTTP request line.
    // e.g. "GET /foo?bar HTTP/1.1\r\n..." -> "/foo?bar"
    static std::string ParsePath(const std::string& request);

    // Returns all values for a repeated query param, preserving order, deduplicating.
    // e.g. "/?stat=deaths&stat=hp" with key "stat" -> {"deaths", "hp"}
    static std::vector<std::string> ParseQueryParams(const std::string& path,
                                                     const std::string& key);
};
