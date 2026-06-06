// WebSocketServer.cpp
// Minimal HTTP + WebSocket server that serves per-stat overlay pages and
// pushes JSON stat updates to connected browser clients in real time.

#include "../include/WebSocketServer.h"
#include <sstream>
#include <algorithm>
#include <cstring>
#include "../include/DebugConsole.h"

// ---------------------------------------------------------------------------
// Internal constants
// ---------------------------------------------------------------------------
static constexpr DWORD RECV_TIMEOUT_MS   = 1000; // SO_RCVTIMEO on client sockets
static constexpr int   SELECT_TIMEOUT_SEC = 1;    // select() timeout in ServerLoop

// Simple Base64 encoding
static const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const unsigned char* bytes_to_encode, unsigned int in_len) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];

        while (i++ < 3)
            ret += '=';
    }

    return ret;
}

// Simple SHA-1 implementation (using Windows Cryptography API)
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")

std::string sha1(const std::string& input) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[20];
    DWORD hashLen = 20;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return "";
    }

    if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }

    if (!CryptHashData(hHash, (const BYTE*)input.c_str(), static_cast<DWORD>(input.length()), 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    return std::string((char*)hash, hashLen);
}

WebSocketServer::WebSocketServer(int port) : serverSocket(INVALID_SOCKET), port(port), running(false) {}

WebSocketServer::~WebSocketServer() {
    Stop();
}

bool WebSocketServer::Start() {
    {
        char msg[64];
        sprintf_s(msg, "[WebSocket] Starting server on port %d...", port);
        DebugConsole::Log(msg);
    }
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        DebugConsole::Log("[WebSocket] ERROR: WSAStartup failed");
        return false;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        DebugConsole::Log("[WebSocket] ERROR: Failed to create socket");
        WSACleanup();
        return false;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        DebugConsole::Log("[WebSocket] ERROR: Failed to bind socket");
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        DebugConsole::Log("[WebSocket] ERROR: Failed to listen on socket");
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }

    running = true;
    serverThread = std::thread(&WebSocketServer::ServerLoop, this);

    DebugConsole::Log("[WebSocket] Server started successfully");
    return true;
}

void WebSocketServer::Stop() {
    running = false;

    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (SOCKET client : clients) {
            closesocket(client);
        }
        clients.clear();
    }

    if (serverThread.joinable()) {
        serverThread.join();
    }

    WSACleanup();
}

void WebSocketServer::ServerLoop() {
    while (running) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(serverSocket, &readSet);

        timeval timeout;
        timeout.tv_sec  = SELECT_TIMEOUT_SEC;
        timeout.tv_usec = 0;

        int selectResult = select(0, &readSet, nullptr, nullptr, &timeout);
        if (selectResult == SOCKET_ERROR || !running) {
            break;
        }

        if (FD_ISSET(serverSocket, &readSet)) {
            sockaddr_in clientAddr;
            int clientAddrLen = sizeof(clientAddr);
            SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrLen);

            if (clientSocket != INVALID_SOCKET) {
                std::thread(&WebSocketServer::HandleClient, this, clientSocket).detach();
            }
        }
    }
}

void WebSocketServer::HandleClient(SOCKET clientSocket) {
    DebugConsole::Log("[WebSocket] New client connected");
    
    char buffer[4096];
    std::string request;

    // Read the HTTP request
    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            DebugConsole::Log("[WebSocket] Client disconnected during request read");
            closesocket(clientSocket);
            return;
        }

        buffer[bytesReceived] = '\0';
        request += buffer;

        if (request.find("\r\n\r\n") != std::string::npos) {
            break;
        }
    }

    DebugConsole::Log("[WebSocket] Request received");
    
    // Check if it's a WebSocket upgrade request
    if (request.find("Upgrade: websocket") != std::string::npos) {
        DebugConsole::Log("[WebSocket] WebSocket upgrade request detected");
        
        // Extract Sec-WebSocket-Key
        static const std::string WS_KEY_HEADER = "Sec-WebSocket-Key:";
        size_t keyPos = request.find(WS_KEY_HEADER);
        if (keyPos != std::string::npos) {
            keyPos += WS_KEY_HEADER.length(); // skip past the header name
            size_t keyEnd = request.find("\r\n", keyPos);
            std::string clientKey = request.substr(keyPos, keyEnd - keyPos);

            // Trim whitespace
            clientKey.erase(0, clientKey.find_first_not_of(" \t\r\n"));
            clientKey.erase(clientKey.find_last_not_of(" \t\r\n") + 1);

            DebugConsole::Log("[WebSocket] Client key extracted, generating handshake response...");

            std::string response = GenerateHandshakeResponse(clientKey);
            int sendResult = send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
            
            if (sendResult == SOCKET_ERROR) {
                DebugConsole::Log("[WebSocket] ERROR: Failed to send handshake response");
                closesocket(clientSocket);
                return;
            }

            DebugConsole::Log("[WebSocket] Handshake sent successfully");

            // Set recv timeout so the loop can check running flag periodically
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO,
                       (const char*)&RECV_TIMEOUT_MS, sizeof(RECV_TIMEOUT_MS));

            // Add to clients list and send current state immediately
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clients.push_back(clientSocket);
                if (lastStats.valid) {
                    std::string initFrame = CreateWebSocketFrame(StatsToJson(lastStats));
                    send(clientSocket, initFrame.c_str(), (int)initFrame.length(), 0);
                }
                DebugConsole::Log("[WebSocket] Client added, initial state sent");
            }

            // Keep connection alive and handle ping/pong
            while (running) {
                int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
                if (bytesReceived <= 0) {
                    int err = WSAGetLastError();
                    if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) continue; // timeout, check running
                    DebugConsole::Log("[WebSocket] Client disconnected");
                    break;
                }

                // Handle ping frames (opcode 0x9)
                if (bytesReceived >= 2 && (buffer[0] & 0x0F) == 0x9) {
                    // Respond with pong (opcode 0xA)
                    buffer[0] = (buffer[0] & 0xF0) | 0x0A;
                    send(clientSocket, buffer, bytesReceived, 0);
                }
            }

            // Remove from clients list
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
                DebugConsole::Log("[WebSocket] Client removed from list");
            }
        } else {
            DebugConsole::Log("[WebSocket] ERROR: No Sec-WebSocket-Key found");
        }
    } else {
        // HTTP request — parse path
        std::string path = ParsePath(request);
        DebugConsole::Log(("[WebSocket] HTTP GET " + path).c_str());

        // /                                    -> all stats
        // /?stat=hp                            -> just hp/maxhp
        // /?stat=deaths&stat=souls             -> deaths then souls (order preserved)
        // /?stat=hp&stat=deaths&stat=souls     -> all, custom order
        auto stats = ParseQueryParams(path, "stat");
        std::string response = HttpResponse("text/html", GetHTMLPage(stats));
        send(clientSocket, response.c_str(), (int)response.length(), 0);
    }

    closesocket(clientSocket);
    DebugConsole::Log("[WebSocket] Client handler finished");
}

std::string WebSocketServer::GenerateHandshakeResponse(const std::string& clientKey) {
    // WebSocket magic string
    const std::string magicString = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = clientKey + magicString;

    // Calculate SHA-1 hash
    std::string hash = sha1(combined);
    if (hash.empty()) {
        // Fallback to a static key if SHA-1 fails
        std::string acceptKey = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
        std::string response = "HTTP/1.1 101 Switching Protocols\r\n"
                               "Upgrade: websocket\r\n"
                               "Connection: Upgrade\r\n"
                               "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
        return response;
    }

    // Base64 encode the SHA-1 hash
    std::string acceptKey = base64_encode((const unsigned char*)hash.c_str(), static_cast<unsigned int>(hash.length()));

    std::string response = "HTTP/1.1 101 Switching Protocols\r\n"
                           "Upgrade: websocket\r\n"
                           "Connection: Upgrade\r\n"
                           "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";

    return response;
}

std::string WebSocketServer::CreateWebSocketFrame(const std::string& data) {
    std::string frame;

    // FIN bit set, text frame (opcode 0x1)
    frame.push_back(static_cast<char>(0x81));

    // Payload length
    size_t len = data.length();
    if (len <= 125) {
        frame.push_back(static_cast<char>(len));
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<char>((len >> 8) & 0xFF));
        frame.push_back(static_cast<char>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back(static_cast<char>((len >> (i * 8)) & 0xFF));
        }
    }

    frame += data;

    return frame;
}

void WebSocketServer::BroadcastStats(const MemoryReader::PlayerStats& stats) {
    if (!stats.valid) {
        return;
    }

    std::string jsonData = StatsToJson(stats);
    std::string frame = CreateWebSocketFrame(jsonData);

    std::lock_guard<std::mutex> lock(clientsMutex);
    lastStats = stats;

    auto it = clients.begin();
    while (it != clients.end()) {
        SOCKET client = *it;
        int result = send(client, frame.c_str(), static_cast<int>(frame.length()), 0);

        if (result == SOCKET_ERROR) {
            closesocket(client);
            it = clients.erase(it);
        } else {
            ++it;
        }
    }
}

std::string WebSocketServer::StatsToJson(const MemoryReader::PlayerStats& stats) {
    std::ostringstream j;
    // Vitals
    j << "{\"hp\":"         << stats.hp
      << ",\"maxHp\":"      << stats.maxHp
      << ",\"fp\":"         << stats.fp
      << ",\"maxFp\":"      << stats.maxFp
      << ",\"stamina\":"    << stats.stamina
      << ",\"maxStamina\":" << stats.maxStamina
      // Resources
      << ",\"souls\":"      << stats.souls
      << ",\"soulsTotal\":" << stats.soulsTotal
      << ",\"soulLevel\":"  << stats.soulLevel
      // Attributes
      << ",\"vit\":"        << stats.vit
      << ",\"atn\":"        << stats.atn
      << ",\"end\":"        << stats.end
      << ",\"str\":"        << stats.str
      << ",\"dex\":"        << stats.dex
      << ",\"res\":"        << stats.res
      << ",\"int\":"        << stats.intl
      << ",\"fth\":"        << stats.fth
      // Resistances
      << ",\"poisonResist\":"  << stats.poisonResist
      << ",\"bleedResist\":"   << stats.bleedResist
      << ",\"diseaseResist\":" << stats.diseaseResist
      << ",\"curseResist\":"   << stats.curseResist
      // Counters
      << ",\"deaths\":"     << stats.deaths
      << ",\"trueDeaths\":" << stats.trueDeaths
      << ",\"playTime\":"   << stats.playTime
      << "}";
    return j.str();
}

// Helper: build a minimal HTTP response
std::string WebSocketServer::HttpResponse(const std::string& contentType, const std::string& body) {
    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: " + contentType + "; charset=utf-8\r\n"
           "Content-Length: " + std::to_string(body.length()) + "\r\n"
           "Connection: close\r\n\r\n" + body;
}

// Parse the path portion from a raw HTTP request line (e.g. "GET /foo?bar HTTP/1.1")
std::string WebSocketServer::ParsePath(const std::string& request) {
    size_t start = request.find("GET ");
    if (start == std::string::npos) return "/";
    start += 4;
    size_t end = request.find(' ', start);
    if (end == std::string::npos) return "/";
    return request.substr(start, end - start);
}

// Extract all values for a repeated query param, preserving order, deduplicating.
// e.g. "/?stat=deaths&stat=hp" -> {"deaths", "hp"}
std::vector<std::string> WebSocketServer::ParseQueryParams(const std::string& path, const std::string& key) {
    std::vector<std::string> result;
    std::string search = key + "=";
    size_t pos = 0;
    while ((pos = path.find(search, pos)) != std::string::npos) {
        pos += search.length();
        size_t end = path.find_first_of("&#", pos);
        std::string val = path.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        if (!val.empty() && std::find(result.begin(), result.end(), val) == result.end())
            result.push_back(val);
        if (end == std::string::npos) break;
        pos = end;
    }
    return result;
}

// Build an HTML element for one stat token, preserving insertion order.
// Each block gets a unique id="stat-X" for CSS targeting from OBS Custom CSS.
// To add a new stat: add a case here, update StatsToJson(), and document in POINTER_MAP.md.
static std::string StatBlock(const std::string& stat) {
    // Helper lambda to build a simple label+value block
    auto simple = [](const char* id, const char* label, const char* field) -> std::string {
        return std::string("<div class=\"stat\" id=\"stat-") + id + "\">"
               "<span class=\"label\">" + label + ": </span>"
               "<span class=\"value\" id=\"" + field + "\">-</span>"
               "</div>\n";
    };
    // Helper for paired value (X / Y)
    auto paired = [](const char* id, const char* label,
                     const char* fieldA, const char* fieldB) -> std::string {
        return std::string("<div class=\"stat\" id=\"stat-") + id + "\">"
               "<span class=\"label\">" + label + ": </span>"
               "<span class=\"value\" id=\"" + fieldA + "\">-</span>"
               "<span class=\"sep\"> / </span>"
               "<span class=\"value\" id=\"" + fieldB + "\">-</span>"
               "</div>\n";
    };

    // --- Vitals ---
    if (stat == "hp")      return paired("hp",      "HP",      "hp",      "maxHp");
    if (stat == "fp")      return paired("fp",      "FP",      "fp",      "maxFp");
    if (stat == "stamina") return paired("stamina",  "Stamina", "stamina", "maxStamina");

    // --- Resources ---
    if (stat == "souls")      return simple("souls",      "Souls",       "souls");
    if (stat == "soulsTotal") return simple("soulsTotal", "Total Souls", "soulsTotal");
    if (stat == "soulLevel")  return simple("soulLevel",  "Soul Level",  "soulLevel");

    // --- Attributes ---
    if (stat == "vit") return simple("vit", "Vitality",     "vit");
    if (stat == "atn") return simple("atn", "Attunement",   "atn");
    if (stat == "end") return simple("end", "Endurance",    "end");
    if (stat == "str") return simple("str", "Strength",     "str");
    if (stat == "dex") return simple("dex", "Dexterity",    "dex");
    if (stat == "res") return simple("res", "Resistance",   "res");
    if (stat == "int") return simple("int", "Intelligence", "int");
    if (stat == "fth") return simple("fth", "Faith",        "fth");

    // --- Resistances ---
    if (stat == "poisonResist")  return simple("poisonResist",  "Poison",  "poisonResist");
    if (stat == "bleedResist")   return simple("bleedResist",   "Bleed",   "bleedResist");
    if (stat == "diseaseResist") return simple("diseaseResist", "Disease", "diseaseResist");
    if (stat == "curseResist")   return simple("curseResist",   "Curse",   "curseResist");

    // --- Counters ---
    if (stat == "deaths")     return simple("deaths",     "Deaths",      "deaths");
    if (stat == "trueDeaths") return simple("trueDeaths", "True Deaths", "trueDeaths");
    if (stat == "playTime")   return simple("playTime",   "Play Time",   "playTime");

    return ""; // unknown stat token — silently ignored
}

// Build the HTML page. Empty vector = all stats in default order.
std::string WebSocketServer::GetHTMLPage(const std::vector<std::string>& stats) {
    static const std::vector<std::string> defaultOrder = {
        "hp", "fp", "stamina", "souls", "soulLevel", "deaths"
    };
    const std::vector<std::string>& order = stats.empty() ? defaultOrder : stats;

    std::string body;
    for (const auto& s : order)
        body += StatBlock(s);

    // JS: iterate all keys in the JSON and update any matching element by id.
    // This means adding new stats to the JSON automatically works here with no JS changes.
    std::string js =
        "<script>\n"
        "function set(id,val){var e=document.getElementById(id);if(e)e.textContent=val;}\n"
        "function update(d){for(var k in d)set(k,d[k]);}\n"
        "function connect(){\n"
        "  var ws=new WebSocket('ws://localhost:3000/ws');\n"
        "  ws.onmessage=function(e){update(JSON.parse(e.data));};\n"
        "  ws.onclose=function(){setTimeout(connect,3000);};\n"
        "}\n"
        "connect();\n"
        "</script>\n";

    // No <style> block — OBS Custom CSS handles all styling.
    // body has background:transparent so OBS chroma key works out of the box.
    return "<!DOCTYPE html>\n"
           "<html><head><meta charset=\"UTF-8\">\n"
           "<style>body{margin:0;background:transparent;}</style>\n"
           "</head>\n"
           "<body>\n" + body + js +
           "</body></html>\n";
}
