// MySocket.h
// CSCN72050 - COIL Project Milestone 2
// MySocket class declaration - TCP/UDP socket communication layer

#pragma once
#pragma comment(lib, "ws2_32.lib")

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

// Global enumerations
enum SocketType { CLIENT, SERVER };
enum ConnectionType { TCP, UDP };

// Default buffer size constant
const int DEFAULT_SIZE = 1024;

class MySocket
{
private:
    char* Buffer;           // Raw receive/send buffer (heap allocated)
    SOCKET          WelcomeSocket;    // TCP server accept socket
    SOCKET          ConnectionSocket; // Active communication socket (TCP & UDP)
    struct sockaddr_in SvrAddr;       // Server address structure
    SocketType      mySocket;         // CLIENT or SERVER
    std::string     IPAddr;           // IPv4 address string
    int             Port;             // Port number
    ConnectionType  connectionType;   // TCP or UDP
    bool            bTCPConnect;      // True when TCP connection is established
    int             MaxSize;          // Allocated buffer size

public:
    // Constructor: configures socket type, IP, port, protocol, buffer size
    // If bufferSize is 0 or invalid, DEFAULT_SIZE is used
    MySocket(SocketType socketType, std::string ipAddress,
        unsigned int port, ConnectionType connType,
        unsigned int bufferSize);

    // Destructor: closes sockets, frees buffer, cleans up Winsock
    ~MySocket();

    // Establishes TCP 3-way handshake (blocked for UDP sockets)
    void ConnectTCP();

    // Tears down TCP connection with 4-way handshake
    void DisconnectTCP();

    // Transmits raw data over TCP or UDP
    void SendData(const char* data, int size);

    // Receives raw data into Buffer, copies to dest, returns bytes received
    int GetData(char* dest);

    // Returns the configured IP address string
    std::string GetIPAddr();

    // Sets a new IP address (error if TCP connection already established)
    void SetIPAddr(std::string newIP);

    // Sets a new port number (error if TCP connection already established)
    void SetPort(int newPort);

    // Returns the configured port number
    int GetPort();

    // Returns the SocketType (CLIENT or SERVER)
    SocketType GetType();

    // Sets the SocketType (blocked if connection or welcome socket is open)
    void SetType(SocketType newType);
};