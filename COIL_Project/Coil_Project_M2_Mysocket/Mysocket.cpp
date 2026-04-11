// MySocket.cpp
// CSCN72050 - COIL Project Milestone 2
// MySocket class implementation - TCP/UDP socket communication layer

#include "MySocket.h"
#include <iostream>

// Constructor
// Configures socket, allocates buffer, and prepares server/client state.
//
// TCP Server: creates socket -> bind -> listen (accept deferred to ConnectTCP)
// UDP Server: creates socket -> bind (ready to recvfrom immediately)
// TCP Client: creates socket (connect deferred to ConnectTCP)
// UDP Client: creates socket (ready to sendto/recvfrom immediately)

MySocket::MySocket(SocketType socketType, std::string ipAddress,
    unsigned int port, ConnectionType connType,
    unsigned int bufferSize)
{
    // Initialize member state
    mySocket = socketType;
    IPAddr = ipAddress;
    Port = static_cast<int>(port);
    connectionType = connType;
    bTCPConnect = false;
    WelcomeSocket = INVALID_SOCKET;
    ConnectionSocket = INVALID_SOCKET;

    // Allocate buffer — fall back to DEFAULT_SIZE if invalid
    MaxSize = (bufferSize > 0) ? static_cast<int>(bufferSize) : DEFAULT_SIZE;
    Buffer = new char[MaxSize];
    memset(Buffer, 0, MaxSize);

    // Start Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "MySocket: WSAStartup failed." << std::endl;
        return;
    }

    // Build server address structure
    memset(&SvrAddr, 0, sizeof(SvrAddr));
    SvrAddr.sin_family = AF_INET;
    SvrAddr.sin_port = htons(static_cast<u_short>(Port));
    inet_pton(AF_INET, IPAddr.c_str(), &SvrAddr.sin_addr);

    // Choose socket protocol
    int proto = (connectionType == TCP) ? SOCK_STREAM : SOCK_DGRAM;

    if (mySocket == SERVER) {
        // Create the welcome/binding socket
        WelcomeSocket = socket(AF_INET, proto, 0);
        if (WelcomeSocket == INVALID_SOCKET) {
            std::cerr << "MySocket: Failed to create server socket." << std::endl;
            return;
        }

        // Allow quick port reuse after restart
        int opt = 1;
        setsockopt(WelcomeSocket, SOL_SOCKET, SO_REUSEADDR,
            reinterpret_cast<const char*>(&opt), sizeof(opt));

        // Bind to port on all interfaces
        struct sockaddr_in bindAddr;
        memset(&bindAddr, 0, sizeof(bindAddr));
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_addr.s_addr = INADDR_ANY;
        bindAddr.sin_port = htons(static_cast<u_short>(Port));

        if (bind(WelcomeSocket, reinterpret_cast<sockaddr*>(&bindAddr),
            sizeof(bindAddr)) == SOCKET_ERROR) {
            std::cerr << "MySocket: Bind failed." << std::endl;
            return;
        }

        if (connectionType == TCP) {
            // TCP Server: listen for incoming connections
            if (listen(WelcomeSocket, SOMAXCONN) == SOCKET_ERROR) {
                std::cerr << "MySocket: Listen failed." << std::endl;
            }
        }
        else {
            // UDP Server: ConnectionSocket and WelcomeSocket are the same
            // (UDP is connectionless: we communicate directly on WelcomeSocket)
            ConnectionSocket = WelcomeSocket;

            // Set receive timeout so GetData never blocks forever in webserver - this is avoid waiting forever if server is not responding - might be changed later depending hardware and software capabilities
            int timeout = 5000; // 5 seconds
            setsockopt(ConnectionSocket, SOL_SOCKET, SO_RCVTIMEO,
                reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        }
    }
    else {
        // CLIENT: create connection socket (connect happens in ConnectTCP for TCP)
        ConnectionSocket = socket(AF_INET, proto, 0);
        if (ConnectionSocket == INVALID_SOCKET) {
            std::cerr << "MySocket: Failed to create client socket." << std::endl;
        }

		// Set receive timeout so GetData never blocks forever in webserver 
        int timeout = 5000; // 5 seconds
        setsockopt(ConnectionSocket, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    }
}

// Destructor
// Closes all open sockets and frees all heap memory.
MySocket::~MySocket()
{
    // Disconnect TCP if still connected
    if (bTCPConnect) {
        DisconnectTCP();
    }

    // Close sockets
    if (ConnectionSocket != INVALID_SOCKET &&
        ConnectionSocket != WelcomeSocket) {
        closesocket(ConnectionSocket);
        ConnectionSocket = INVALID_SOCKET;
    }
    if (WelcomeSocket != INVALID_SOCKET) {
        closesocket(WelcomeSocket);
        WelcomeSocket = INVALID_SOCKET;
    }

    // Free buffer
    delete[] Buffer;
    Buffer = nullptr;

    WSACleanup();
}

// ConnectTCP
// Client: initiates 3-way handshake with the configured server.
// Server: blocks and accepts one incoming client connection.
// Blocked entirely for UDP sockets.
void MySocket::ConnectTCP()
{
    if (connectionType == UDP) {
        std::cerr << "ConnectTCP: Cannot connect! socket is configured for UDP." << std::endl;
        return;
    }
    if (bTCPConnect) {
        std::cerr << "ConnectTCP: Already connected." << std::endl;
        return;
    }

    if (mySocket == CLIENT) {
        // Connect to server
        if (connect(ConnectionSocket,
            reinterpret_cast<sockaddr*>(&SvrAddr),
            sizeof(SvrAddr)) == SOCKET_ERROR) {
            std::cerr << "ConnectTCP: connect() failed. Error: "
                << WSAGetLastError() << std::endl;
            return;
        }
        bTCPConnect = true;
    }
    else {
        // Server accepts an incoming connection
        struct sockaddr_in clientAddr;
        int addrLen = sizeof(clientAddr);
        ConnectionSocket = accept(WelcomeSocket,
            reinterpret_cast<sockaddr*>(&clientAddr),
            &addrLen);
        if (ConnectionSocket == INVALID_SOCKET) {
            std::cerr << "ConnectTCP: accept() failed. Error: "
                << WSAGetLastError() << std::endl;
            return;
        }
        bTCPConnect = true;
    }
}

// DisconnectTCP
// Closes the active TCP connection (4-way handshake via shutdown + closesocket).
void MySocket::DisconnectTCP()
{
    if (connectionType == UDP) {
        std::cerr << "DisconnectTCP: Not applicable for UDP." << std::endl;
        return;
    }
    if (!bTCPConnect) {
        std::cerr << "DisconnectTCP: No active TCP connection." << std::endl;
        return;
    }

    shutdown(ConnectionSocket, SD_BOTH);

    // For server, close the accepted connection socket (keep WelcomeSocket open)
    if (mySocket == SERVER) {
        closesocket(ConnectionSocket);
        ConnectionSocket = INVALID_SOCKET;
    }

    bTCPConnect = false;
}

// SendData
// Transmits 'size' bytes starting at 'data' over the socket.
// TCP: uses send()
// UDP: uses sendto() with the configured SvrAddr
void MySocket::SendData(const char* data, int size)
{
    if (data == nullptr || size <= 0) {
        std::cerr << "SendData: Invalid data or size." << std::endl;
        return;
    }

    if (connectionType == TCP) {
        if (!bTCPConnect) {
            std::cerr << "SendData: No TCP connection established." << std::endl;
            return;
        }
        int bytesSent = send(ConnectionSocket, data, size, 0);
        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "SendData: send() failed. Error: "
                << WSAGetLastError() << std::endl;
        }
    }
    else {
        // UDP: send to the configured server address
        int bytesSent = sendto(ConnectionSocket, data, size, 0,
            reinterpret_cast<const sockaddr*>(&SvrAddr),
            sizeof(SvrAddr));
        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "SendData: sendto() failed. Error: "
                << WSAGetLastError() << std::endl;
        }
    }
}

// GetData
// Receives raw data into the internal Buffer, copies it to 'dest',
// and returns the number of bytes received.
// TCP: uses recv()
// UDP: uses recvfrom()
int MySocket::GetData(char* dest)
{
    if (dest == nullptr) {
        std::cerr << "GetData: Destination buffer is null." << std::endl;
        return 0;
    }

    memset(Buffer, 0, MaxSize);
    int bytesReceived = 0;

    if (connectionType == TCP) {
        if (!bTCPConnect) {
            std::cerr << "GetData: No TCP connection established." << std::endl;
            return 0;
        }
        bytesReceived = recv(ConnectionSocket, Buffer, MaxSize, 0);
    }
    else {
        // UDP: receive and capture sender address
        struct sockaddr_in fromAddr;
        int fromLen = sizeof(fromAddr);
        bytesReceived = recvfrom(ConnectionSocket, Buffer, MaxSize, 0,
            reinterpret_cast<sockaddr*>(&fromAddr),
            &fromLen);
    }

    if (bytesReceived == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAETIMEDOUT) {
            std::cerr << "GetData: Receive timed out (5s) — no response from robot." << std::endl;
        }
        else {
            std::cerr << "GetData: recv/recvfrom failed. Error: " << err << std::endl;
        }
        return 0;
    }
    // Copy from internal buffer to destination
    memcpy(dest, Buffer, bytesReceived);
    return bytesReceived;
}

// GetIPAddr
std::string MySocket::GetIPAddr()
{
    return IPAddr;
}

// SetIPAddr
// Blocked if a TCP connection is already active.
void MySocket::SetIPAddr(std::string newIP)
{
    if (bTCPConnect) {
        std::cerr << "SetIPAddr: Cannot change IP — TCP connection is active." << std::endl;
        return;
    }
    IPAddr = newIP;
    inet_pton(AF_INET, IPAddr.c_str(), &SvrAddr.sin_addr);
}

// SetPort
// Blocked if a TCP connection is already active.
void MySocket::SetPort(int newPort)
{
    if (bTCPConnect) {
        std::cerr << "SetPort: Cannot change port — TCP connection is active." << std::endl;
        return;
    }
    Port = newPort;
    SvrAddr.sin_port = htons(static_cast<u_short>(Port));
}

// GetPort
int MySocket::GetPort()
{
    return Port;
}

// GetType
SocketType MySocket::GetType()
{
    return mySocket;
}

// SetType
// Blocked if a TCP connection is active or the WelcomeSocket is open.
void MySocket::SetType(SocketType newType)
{
    if (bTCPConnect) {
        std::cerr << "SetType: Cannot change type — TCP connection is active." << std::endl;
        return;
    }
    if (WelcomeSocket != INVALID_SOCKET) {
        std::cerr << "SetType: Cannot change type — Welcome socket is open." << std::endl;
        return;
    }
    mySocket = newType;
}