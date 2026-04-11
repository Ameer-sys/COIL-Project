// MySocket.h
// CSCN72050 - COIL Project
// Cross-platform socket class - compiles on Windows (Winsock2) and Linux (POSIX)

#pragma once

// ── Platform detection ────────────────────────────────────────────────────────
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #pragma comment(lib, "ws2_32.lib")
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <cerrno>
    #include <cstring>
    // Map Linux types/constants to match Winsock names used in .cpp
    typedef int SOCKET;
    #define INVALID_SOCKET  (-1)
    #define SOCKET_ERROR    (-1)
    #define closesocket(s)  close(s)
    #define SD_BOTH         SHUT_RDWR
#endif

#include <string>

// ── Global enumerations ───────────────────────────────────────────────────────
enum SocketType     { CLIENT, SERVER };
enum ConnectionType { TCP, UDP };

// ── Default buffer size ───────────────────────────────────────────────────────
const int DEFAULT_SIZE = 1024;

// ── MySocket class ────────────────────────────────────────────────────────────
class MySocket
{
private:
    char*              Buffer;
    SOCKET             WelcomeSocket;
    SOCKET             ConnectionSocket;
    struct sockaddr_in SvrAddr;
    SocketType         mySocket;
    std::string        IPAddr;
    int                Port;
    ConnectionType     connectionType;
    bool               bTCPConnect;
    int                MaxSize;

public:
    MySocket(SocketType socketType, std::string ipAddress,
             unsigned int port, ConnectionType connType,
             unsigned int bufferSize);
    ~MySocket();

    void        ConnectTCP();
    void        DisconnectTCP();
    void        SendData(const char* data, int size);
    int         GetData(char* dest);

    std::string GetIPAddr();
    void        SetIPAddr(std::string newIP);
    void        SetPort(int newPort);
    int         GetPort();
    SocketType  GetType();
    void        SetType(SocketType newType);
};