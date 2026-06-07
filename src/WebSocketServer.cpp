// WebSocketServer.cpp
// Minimal HTTP + WebSocket server that serves per-stat overlay pages and
// pushes JSON stat updates to connected browser clients in real time.

#include "../include/WebSocketServer.h"
#include "../include/StatRegistry.h"
#include <sstream>
#include <algorithm>
#include <cstring>
#include "../include/DebugConsole.h"

// ---------------------------------------------------------------------------
// Internal constants
// ---------------------------------------------------------------------------
static constexpr int   SELECT_TIMEOUT_SEC  = 1;   // select() timeout in ServerLoop
static constexpr DWORD SEND_TIMEOUT_MS     = 100;  // SO_SNDTIMEO — prevents slow-client blocking
static constexpr int   MAX_HTTP_REQUEST    = 8192; // reject requests larger than this

// Simple Base64 encoding
static const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const unsigned char* bytes_to_encode, unsigned int in_len) {
    std::string ret;
    int i = 0;
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
        for (int j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (int j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];

        while (i++ < 3)
            ret += '=';
    }

    return ret;
}

// SHA-1 via Windows Cryptography API
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")

static std::string sha1(const std::string& input) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[20];
    DWORD hashLen = 20;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
        return "";
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

// ---------------------------------------------------------------------------
// Construction / lifecycle
// ---------------------------------------------------------------------------

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
    serverAddr.sin_port = htons(static_cast<u_short>(port));

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
            // Send WebSocket Close frame (opcode 0x8) before closing
            char closeFrame[] = {static_cast<char>(0x88), 0x00};
            send(client, closeFrame, 2, 0);
            closesocket(client);
        }
        clients.clear();
    }

    // Wait for handler threads to finish (they'll exit because sockets are closed)
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (activeHandlers > 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (serverThread.joinable()) {
        serverThread.join();
    }

    WSACleanup();
}

// ---------------------------------------------------------------------------
// ServerLoop — accepts connections, enforces rate limit
// ---------------------------------------------------------------------------
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
                if (activeHandlers >= MAX_CLIENTS) {
                    closesocket(clientSocket);
                    DebugConsole::Log("[WebSocket] Max clients reached, rejecting");
                } else {
                    std::thread([this, clientSocket]() {
                        activeHandlers++;
                        HandleClient(clientSocket);
                        activeHandlers--;
                    }).detach();
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// HandleClient — HTTP routing + WebSocket lifecycle
// ---------------------------------------------------------------------------
void WebSocketServer::HandleClient(SOCKET clientSocket) {
    DebugConsole::Log("[WebSocket] New client connected");

    // Set send timeout to prevent slow clients from blocking broadcasts
    setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO,
               (const char*)&SEND_TIMEOUT_MS, sizeof(SEND_TIMEOUT_MS));

    char buffer[4096];
    std::string request;

    // ---- Read HTTP request (with size limit) ----
    DWORD recvTimeout = 5000; // 5 s to read the initial request
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO,
               (const char*)&recvTimeout, sizeof(recvTimeout));

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            closesocket(clientSocket);
            return;
        }
        buffer[bytesReceived] = '\0';
        request += buffer;

        if (request.size() > MAX_HTTP_REQUEST) {
            DebugConsole::Log("[WebSocket] Request too large, dropping client");
            closesocket(clientSocket);
            return;
        }
        if (request.find("\r\n\r\n") != std::string::npos) {
            break;
        }
    }

    // ---- WebSocket upgrade ----
    if (request.find("Upgrade: websocket") != std::string::npos) {
        DebugConsole::Log("[WebSocket] WebSocket upgrade request detected");

        static const std::string WS_KEY_HEADER = "Sec-WebSocket-Key:";
        size_t keyPos = request.find(WS_KEY_HEADER);
        if (keyPos == std::string::npos) {
            DebugConsole::Log("[WebSocket] ERROR: No Sec-WebSocket-Key found");
            closesocket(clientSocket);
            return;
        }

        keyPos += WS_KEY_HEADER.length();
        size_t keyEnd = request.find("\r\n", keyPos);
        std::string clientKey = request.substr(keyPos, keyEnd - keyPos);
        clientKey.erase(0, clientKey.find_first_not_of(" \t\r\n"));
        clientKey.erase(clientKey.find_last_not_of(" \t\r\n") + 1);

        std::string response = GenerateHandshakeResponse(clientKey);
        if (send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0) == SOCKET_ERROR) {
            DebugConsole::Log("[WebSocket] ERROR: Failed to send handshake response");
            closesocket(clientSocket);
            return;
        }

        // Add to clients list and push current state
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.push_back(clientSocket);
            if (lastStats.valid) {
                std::string initFrame = CreateWebSocketFrame(StatsToJson(lastStats));
                send(clientSocket, initFrame.c_str(), (int)initFrame.length(), 0);
            }
        }

        // ---- WebSocket message loop (proper frame parsing) ----
        while (running) {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(clientSocket, &readSet);
            timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            int sel = select(0, &readSet, nullptr, nullptr, &tv);
            if (sel == 0) continue;   // timeout — check running flag
            if (sel < 0 || !running) break;

            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesReceived <= 0) break;
            if (bytesReceived < 2)  continue;

            uint8_t b0 = static_cast<uint8_t>(buffer[0]);
            uint8_t b1 = static_cast<uint8_t>(buffer[1]);
            uint8_t opcode = b0 & 0x0F;
            bool isMasked = (b1 & 0x80) != 0;
            uint64_t payloadLen = b1 & 0x7F;

            int headerLen = 2;
            if (payloadLen == 126) {
                if (bytesReceived < 4) continue;
                payloadLen = (static_cast<uint8_t>(buffer[2]) << 8) |
                              static_cast<uint8_t>(buffer[3]);
                headerLen = 4;
            } else if (payloadLen == 127) {
                if (bytesReceived < 10) continue;
                payloadLen = 0;
                for (int i = 0; i < 8; i++)
                    payloadLen = (payloadLen << 8) | static_cast<uint8_t>(buffer[2 + i]);
                headerLen = 10;
            }

            if (isMasked) {
                if (bytesReceived < headerLen + 4) continue;
                headerLen += 4;
            }

            // Unmask payload in-place
            if (isMasked && payloadLen > 0) {
                uint8_t maskKey[4];
                memcpy(maskKey, buffer + headerLen - 4, 4);
                char* payload = buffer + headerLen;
                uint64_t available = static_cast<uint64_t>(bytesReceived - headerLen);
                uint64_t len = (payloadLen < available) ? payloadLen : available;
                for (uint64_t i = 0; i < len; i++)
                    payload[i] ^= maskKey[i % 4];
            }

            if (opcode == 0x8) {
                // Close frame — send close response and disconnect
                char closeResp[] = {static_cast<char>(0x88), 0x00};
                send(clientSocket, closeResp, 2, 0);
                break;
            } else if (opcode == 0x9) {
                // Ping — respond with pong (unmasked, same payload)
                char* payload = buffer + headerLen;
                uint64_t available = static_cast<uint64_t>(bytesReceived - headerLen);
                uint64_t len = (payloadLen < available) ? payloadLen : available;
                std::string pong;
                pong.push_back(static_cast<char>(0x8A));
                if (len <= 125)
                    pong.push_back(static_cast<char>(len));
                else {
                    pong.push_back(126);
                    pong.push_back(static_cast<char>((len >> 8) & 0xFF));
                    pong.push_back(static_cast<char>(len & 0xFF));
                }
                pong.append(payload, static_cast<size_t>(len));
                send(clientSocket, pong.c_str(), static_cast<int>(pong.length()), 0);
            }
            // other opcodes (text, binary, pong, continuation) are ignored
        }

        // Remove from clients list
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
        }
    } else {
        // ---- Plain HTTP request ----
        std::string path = ParsePath(request);
        DebugConsole::Log(("[WebSocket] HTTP GET " + path).c_str());
        std::string resp = HttpResponse("text/html", GetHTMLPage());
        send(clientSocket, resp.c_str(), (int)resp.length(), 0);
    }

    closesocket(clientSocket);
}

// ---------------------------------------------------------------------------
// WebSocket handshake
// ---------------------------------------------------------------------------
std::string WebSocketServer::GenerateHandshakeResponse(const std::string& clientKey) {
    const std::string magicString = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = clientKey + magicString;

    std::string hash = sha1(combined);
    if (hash.empty()) {
        std::string acceptKey = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
        return "HTTP/1.1 101 Switching Protocols\r\n"
               "Upgrade: websocket\r\n"
               "Connection: Upgrade\r\n"
               "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
    }

    std::string acceptKey = base64_encode((const unsigned char*)hash.c_str(),
                                          static_cast<unsigned int>(hash.length()));

    return "HTTP/1.1 101 Switching Protocols\r\n"
           "Upgrade: websocket\r\n"
           "Connection: Upgrade\r\n"
           "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
}

// ---------------------------------------------------------------------------
// WebSocket frame creation (server → client, unmasked)
// ---------------------------------------------------------------------------
std::string WebSocketServer::CreateWebSocketFrame(const std::string& data) {
    std::string frame;
    frame.push_back(static_cast<char>(0x81)); // FIN + text opcode

    size_t len = data.length();
    if (len <= 125) {
        frame.push_back(static_cast<char>(len));
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<char>((len >> 8) & 0xFF));
        frame.push_back(static_cast<char>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--)
            frame.push_back(static_cast<char>((len >> (i * 8)) & 0xFF));
    }

    frame += data;
    return frame;
}

// ---------------------------------------------------------------------------
// BroadcastStats — pushes to all connected WebSocket clients
// ---------------------------------------------------------------------------
void WebSocketServer::BroadcastStats(const MemoryReader::PlayerStats& stats) {
    if (!stats.valid) return;

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

// ---------------------------------------------------------------------------
// StatsToJson — data-driven from StatRegistry
// ---------------------------------------------------------------------------
std::string WebSocketServer::StatsToJson(const MemoryReader::PlayerStats& stats) {
    std::ostringstream j;
    j << '{';
    for (size_t i = 0; i < JSON_FIELDS_COUNT; i++) {
        if (i > 0) j << ',';
        int32_t val = *reinterpret_cast<const int32_t*>(
            reinterpret_cast<const char*>(&stats) + JSON_FIELDS[i].offset
        );
        j << '"' << JSON_FIELDS[i].key << "\":" << val;
    }
    j << '}';
    return j.str();
}

// ---------------------------------------------------------------------------
// HTTP helpers
// ---------------------------------------------------------------------------
std::string WebSocketServer::HttpResponse(const std::string& contentType, const std::string& body) {
    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: " + contentType + "; charset=utf-8\r\n"
           "Content-Length: " + std::to_string(body.length()) + "\r\n"
           "Connection: close\r\n\r\n" + body;
}

std::string WebSocketServer::ParsePath(const std::string& request) {
    size_t start = request.find("GET ");
    if (start == std::string::npos) return "/";
    start += 4;
    size_t end = request.find(' ', start);
    if (end == std::string::npos) return "/";
    return request.substr(start, end - start);
}

// ---------------------------------------------------------------------------
// GetHTMLPage — overlay page with client-side stat rendering
//
// The JS reads URL params (?hp&deaths='Label'), builds DOM elements,
// and updates values on WebSocket data. Supports custom labels, templates, and raw mode.
// ---------------------------------------------------------------------------
std::string WebSocketServer::GetHTMLPage() {
    // Generate JS stat definitions from the C++ registry
    std::string defs = "var S={";
    for (size_t i = 0; i < DISPLAY_STATS_COUNT; i++) {
        if (i > 0) defs += ",";
        defs += std::string("\"") + DISPLAY_STATS[i].key + "\":{l:\"" + DISPLAY_STATS[i].label + "\"";
        if (DISPLAY_STATS[i].pairedKey)
            defs += std::string(",p:\"") + DISPLAY_STATS[i].pairedKey + "\"";
        defs += "}";
    }
    defs += "};\n";

    std::string defaults = "var D=[";
    for (size_t i = 0; i < DEFAULT_DISPLAY_COUNT; i++) {
        if (i > 0) defaults += ",";
        defaults += std::string("\"") + DEFAULT_DISPLAY[i] + "\"";
    }
    defaults += "];\n";

    std::string js =
        "<script>\n"
        + defs + defaults +
        // Parse URL params - simplified template syntax only
        "function gp(){"
          "var s=location.search.substring(1);"
          "if(!s)return[];"
          "var p=s.split('&');"
          "var lines=[];"
          "for(var i=0;i<p.length;i++){"
            "var kv=p[i].split('=');"
            "var k=decodeURIComponent(kv[0]);"
            "var v=kv[1]?decodeURIComponent(kv[1]):'';"
            // If there's a value, use it as template; otherwise use the key
            "var tmpl=v?v:k;"
            "lines.push({template:tmpl});"
          "}"
          "return lines;"
        "}\n"
        // Process escapes - convert escaped chars to placeholders
        "function processEscapes(str){"
          "return str"
            ".replace(/\\\\n/g,'__NEWLINE__')"
            ".replace(/\\\\t/g,'__TAB__')"
            ".replace(/\\\\\\|/g,'__PIPE__')"
            ".replace(/\\\\\\\\/g,'__BACKSLASH__');"
        "}\n"
        "function unprocessEscapes(str){"
          "return str"
            ".replace(/__NEWLINE__/g,'\\n')"
            ".replace(/__TAB__/g,'\\t')"
            ".replace(/__PIPE__/g,'|')"
            ".replace(/__BACKSLASH__/g,'\\\\');"
        "}\n"
        // Apply template with variable substitution - _variable_ syntax
        "function applyTemplate(tmpl,data){"
          "var result=tmpl;"
          "result=result.replace(/_([^_:]+)(?::([^_]+))?_/g,function(match,key,fmt){"
            "var val=data[key];"
            "if(val===undefined)return match;"
            "if(!fmt&&key==='playTime')fmt='hms';"
            "if(fmt==='hms'){"
              "var s=Math.floor(val/1000);"
              "var h=Math.floor(s/3600),m=Math.floor(s%3600/60),ss=s%60;"
              "return h+':'+(m<10?'0':'')+m+':'+(ss<10?'0':'')+ss;"
            "}"
            "if(fmt==='s')return Math.floor(val/1000);"
            "if(fmt==='m')return Math.floor(val/60000);"
            "if(fmt==='h')return Math.floor(val/3600000);"
            "if(fmt==='ms')return val;"
            "return val;"
          "});"
          "return result;"
        "}\n"
        // Format time as H:MM:SS
        "function ft(s){"
          "var h=Math.floor(s/3600),m=Math.floor(s%3600/60),ss=s%60;"
          "return h+':'+(m<10?'0':'')+m+':'+(ss<10?'0':'')+ss;"
        "}\n"
        // Build DOM elements for each requested stat
        "function init(){"
          "var lines=gp();"
          "if(lines.length===0){"
            // Use defaults if no params
            "for(var i=0;i<D.length;i++){"
              "var k=D[i],d=S[k];"
              "if(!d)continue;"
              "var div=document.createElement('div');"
              "div.className='stat';div.id='stat-'+k;"
              "var lbl=document.createElement('span');"
              "lbl.className='label';lbl.textContent=d.l+': ';"
              "div.appendChild(lbl);"
              "var v=document.createElement('span');"
              "v.className='value';v.id=k;v.textContent='-';"
              "div.appendChild(v);"
              "if(d.p){"
                "var sep=document.createElement('span');"
                "sep.className='sep';sep.textContent=' / ';"
                "div.appendChild(sep);"
                "var v2=document.createElement('span');"
                "v2.className='value';v2.id=d.p;v2.textContent='-';"
                "div.appendChild(v2);"
              "}"
              "document.body.appendChild(div);"
            "}"
          "}else{"
            "for(var i=0;i<lines.length;i++){"
              "var line=lines[i];"
              "var processed=processEscapes(line.template);"
              "var parts=processed.split('|');"
              "for(var j=0;j<parts.length;j++){"
                "var unprocessed=unprocessEscapes(parts[j]);"
                "var div=document.createElement('div');"
                "div.id='line-'+i+'-'+j;"
                "div.textContent='-';"
                "div.dataset.template=unprocessed;"
                "document.body.appendChild(div);"
              "}"
            "}"
          "}"
        "}\n"
        // Update helpers
        "function set(id,v){var e=document.getElementById(id);if(e)e.textContent=v;}\n"
        "function upd(d){"
          "var lines=gp();"
          "if(lines.length===0){"
            // Update defaults
            "for(var i=0;i<D.length;i++){"
              "var k=D[i];"
              "if(k==='playTime')set(k,ft(d[k]));else set(k,d[k]);"
            "}"
          "}else{"
            "for(var i=0;i<lines.length;i++){"
              "var line=lines[i];"
              "var processed=processEscapes(line.template);"
              "var parts=processed.split('|');"
              "for(var j=0;j<parts.length;j++){"
                "var unprocessed=unprocessEscapes(parts[j]);"
                "var e=document.getElementById('line-'+i+'-'+j);"
                "if(e)e.textContent=applyTemplate(unprocessed,d);"
              "}"
            "}"
          "}"
        "}\n"
        // WebSocket with auto-reconnect
        "function connect(){"
          "var ws=new WebSocket('ws://'+location.host+'/ws');"
          "ws.onmessage=function(e){upd(JSON.parse(e.data));};"
          "ws.onclose=function(){setTimeout(connect,3000);};"
        "}\n"
        "init();connect();\n"
        "</script>\n";

    return "<!DOCTYPE html>\n"
           "<html><head><meta charset=\"UTF-8\">\n"
           "<style>body{margin:0;background:transparent;}</style>\n"
           "</head>\n"
           "<body>\n" + js +
           "</body></html>\n";
}
