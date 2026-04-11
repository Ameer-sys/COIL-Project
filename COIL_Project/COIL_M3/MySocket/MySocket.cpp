// MySocket.cpp
// CSCN72050 - COIL Project
// Cross-platform socket implementation - Windows (Winsock2) + Linux (POSIX)

#include "MySocket.h"
#include <iostream>
#include <cstring>

// Platform-specific: start / stop Winsock 
static void SocketStartup()
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        std::cerr << "MySocket: WSAStartup failed." << std::endl;
#endif
}

static void SocketCleanup()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

static int LastError()
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

//  Constructor 
MySocket::MySocket(SocketType socketType, std::string ipAddress,
                   unsigned int port, ConnectionType connType,
                   unsigned int bufferSize)
{
    mySocket         = socketType;
    IPAddr           = ipAddress;
    Port             = static_cast<int>(port);
    connectionType   = connType;
    bTCPConnect      = false;
    WelcomeSocket    = INVALID_SOCKET;
    ConnectionSocket = INVALID_SOCKET;

    MaxSize = (bufferSize > 0) ? static_cast<int>(bufferSize) : DEFAULT_SIZE;
    Buffer  = new char[MaxSize];
    memset(Buffer, 0, MaxSize);

    SocketStartup();

    memset(&SvrAddr, 0, sizeof(SvrAddr));
    SvrAddr.sin_family = AF_INET;
    SvrAddr.sin_port   = htons(static_cast<unsigned short>(Port));
    inet_pton(AF_INET, IPAddr.c_str(), &SvrAddr.sin_addr);

    int proto = (connectionType == TCP) ? SOCK_STREAM : SOCK_DGRAM;

    if (mySocket == SERVER) {
        WelcomeSocket = socket(AF_INET, proto, 0);
        if (WelcomeSocket == INVALID_SOCKET) {
            std::cerr << "MySocket: Failed to create server socket." << std::endl;
            return;
        }

        int opt = 1;
        setsockopt(WelcomeSocket, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&opt), sizeof(opt));

        struct sockaddr_in bindAddr;
        memset(&bindAddr, 0, sizeof(bindAddr));
        bindAddr.sin_family      = AF_INET;
        bindAddr.sin_addr.s_addr = INADDR_ANY;
        bindAddr.sin_port        = htons(static_cast<unsigned short>(Port));

        if (bind(WelcomeSocket,
                 reinterpret_cast<struct sockaddr*>(&bindAddr),
                 sizeof(bindAddr)) == SOCKET_ERROR) {
            std::cerr << "MySocket: Bind failed. Error: " << LastError() << std::endl;
            return;
        }

        if (connectionType == TCP) {
            if (listen(WelcomeSocket, SOMAXCONN) == SOCKET_ERROR)
                std::cerr << "MySocket: Listen failed." << std::endl;
        } else {
            // UDP server — use WelcomeSocket directly for communication
            ConnectionSocket = WelcomeSocket;

            // Avoid waiting forever if server is not responding
            // May be adjusted later depending on hardware and software capabilities
#ifdef _WIN32
            int timeout = 5000;
            setsockopt(ConnectionSocket, SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
            struct timeval tv;
            tv.tv_sec  = 5;
            tv.tv_usec = 0;
            setsockopt(ConnectionSocket, SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif
        }
    } else {
        // CLIENT
        ConnectionSocket = socket(AF_INET, proto, 0);
        if (ConnectionSocket == INVALID_SOCKET) {
            std::cerr << "MySocket: Failed to create client socket." << std::endl;
            return;
        }

        // Avoid waiting forever if server is not responding
        // May be adjusted later depending on hardware and software capabilities
#ifdef _WIN32
        int timeout = 5000;
        setsockopt(ConnectionSocket, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
        struct timeval tv;
        tv.tv_sec  = 5;
        tv.tv_usec = 0;
        setsockopt(ConnectionSocket, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif
    }
}

// Destructor 
MySocket::~MySocket()
{
    if (bTCPConnect) DisconnectTCP();

    if (ConnectionSocket != INVALID_SOCKET &&
        ConnectionSocket != WelcomeSocket) {
        closesocket(ConnectionSocket);
        ConnectionSocket = INVALID_SOCKET;
    }
    if (WelcomeSocket != INVALID_SOCKET) {
        closesocket(WelcomeSocket);
        WelcomeSocket = INVALID_SOCKET;
    }

    delete[] Buffer;
    Buffer = nullptr;

    SocketCleanup();
}

// ConnectTCP
void MySocket::ConnectTCP()
{
    if (connectionType == UDP) {
        std::cerr << "ConnectTCP: Not applicable for UDP." << std::endl;
        return;
    }
    if (bTCPConnect) {
        std::cerr << "ConnectTCP: Already connected." << std::endl;
        return;
    }

    if (mySocket == CLIENT) {
        if (connect(ConnectionSocket,
                    reinterpret_cast<struct sockaddr*>(&SvrAddr),
                    sizeof(SvrAddr)) == SOCKET_ERROR) {
            std::cerr << "ConnectTCP: connect() failed. Error: "
                      << LastError() << std::endl;
            return;
        }
        bTCPConnect = true;
    } else {
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        ConnectionSocket = accept(WelcomeSocket,
                                  reinterpret_cast<struct sockaddr*>(&clientAddr),
                                  &addrLen);
        if (ConnectionSocket == INVALID_SOCKET) {
            std::cerr << "ConnectTCP: accept() failed. Error: "
                      << LastError() << std::endl;
            return;
        }
        bTCPConnect = true;
    }
}

// DisconnectTCP
void MySocket::DisconnectTCP()
{
    if (connectionType == UDP) {
        std::cerr << "DisconnectTCP: Not applicable for UDP." << std::endl;
        return;
    }
    if (!bTCPConnect) {
        std::cerr << "DisconnectTCP: No active connection." << std::endl;
        return;
    }

    shutdown(ConnectionSocket, SD_BOTH);

    if (mySocket == SERVER) {
        closesocket(ConnectionSocket);
        ConnectionSocket = INVALID_SOCKET;
    }
    bTCPConnect = false;
}

// SendData
void MySocket::SendData(const char* data, int size)
{
    if (data == nullptr || size <= 0) {
        std::cerr << "SendData: Invalid data or size." << std::endl;
        return;
    }

    if (connectionType == TCP) {
        if (!bTCPConnect) {
            std::cerr << "SendData: No TCP connection." << std::endl;
            return;
        }
        if (send(ConnectionSocket, data, size, 0) == SOCKET_ERROR)
            std::cerr << "SendData: send() failed. Error: " << LastError() << std::endl;
    } else {
        if (sendto(ConnectionSocket, data, size, 0,
                   reinterpret_cast<const struct sockaddr*>(&SvrAddr),
                   sizeof(SvrAddr)) == SOCKET_ERROR)
            std::cerr << "SendData: sendto() failed. Error: " << LastError() << std::endl;
    }
}

// GetData 
int MySocket::GetData(char* dest)
{
    if (dest == nullptr) {
        std::cerr << "GetData: Null destination." << std::endl;
        return 0;
    }

    memset(Buffer, 0, MaxSize);
    int bytesReceived = 0;

    if (connectionType == TCP) {
        if (!bTCPConnect) {
            std::cerr << "GetData: No TCP connection." << std::endl;
            return 0;
        }
        bytesReceived = recv(ConnectionSocket, Buffer, MaxSize, 0);
    } else {
        struct sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);
        bytesReceived = recvfrom(ConnectionSocket, Buffer, MaxSize, 0,
                                 reinterpret_cast<struct sockaddr*>(&fromAddr),
                                 &fromLen);
    }

    if (bytesReceived == SOCKET_ERROR) {
        int err = LastError();
#ifdef _WIN32
        if (err == WSAETIMEDOUT)
#else
        if (err == EAGAIN || err == EWOULDBLOCK)
#endif
            std::cerr << "GetData: Timed out — no response from robot." << std::endl;
        else
            std::cerr << "GetData: Failed. Error: " << err << std::endl;
        return 0;
    }

    memcpy(dest, Buffer, bytesReceived);
    return bytesReceived;
}

// Getters / Setters 
std::string MySocket::GetIPAddr() { return IPAddr; }

void MySocket::SetIPAddr(std::string newIP)
{
    if (bTCPConnect) {
        std::cerr << "SetIPAddr: Cannot change — TCP connection active." << std::endl;
        return;
    }
    IPAddr = newIP;
    inet_pton(AF_INET, IPAddr.c_str(), &SvrAddr.sin_addr);
}

void MySocket::SetPort(int newPort)
{
    if (bTCPConnect) {
        std::cerr << "SetPort: Cannot change — TCP connection active." << std::endl;
        return;
    }
    Port = newPort;
    SvrAddr.sin_port = htons(static_cast<unsigned short>(Port));
}

int        MySocket::GetPort()  { return Port; }
SocketType MySocket::GetType()  { return mySocket; }

void MySocket::SetType(SocketType newType)
{
    if (bTCPConnect) {
        std::cerr << "SetType: Cannot change — TCP connection active." << std::endl;
        return;
    }
    if (WelcomeSocket != INVALID_SOCKET) {
        std::cerr << "SetType: Cannot change — Welcome socket is open." << std::endl;
        return;
    }
    mySocket = newType;
}