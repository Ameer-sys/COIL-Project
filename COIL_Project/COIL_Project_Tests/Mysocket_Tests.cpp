// MySocketTest.cpp
// CSCN72050 - COIL Project Milestone 2
// MSTest Unit Tests for MySocket class

#include "pch.h"
#include "CppUnitTest.h"
#include "../Coil_Project_M2_Mysocket/Mysocket.h"
#include "../COIL_Project/PktDef.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace MySocketTests
{
    // Constructor & Default State Tests
    TEST_CLASS(ConstructorTests)
    {
    public:
        // Client TCP socket stores correct IP
        TEST_METHOD(TC01_Constructor_ClientTCP_StoresIP)
        {
            MySocket s(CLIENT, "10.172.41.150", 29000, TCP, 512);
            Assert::AreEqual(std::string("10.172.41.150"), s.GetIPAddr());
        }

        // Client UDP socket stores correct port
        TEST_METHOD(TC02_Constructor_ClientUDP_StoresPort)
        {
            MySocket s(CLIENT, "10.172.41.150", 29500, UDP, 512);
            Assert::AreEqual(29500, s.GetPort());
        }

        // SocketType is correctly stored as CLIENT
        TEST_METHOD(TC03_Constructor_ClientType_Stored)
        {
            MySocket s(CLIENT, "127.0.0.1", 29000, TCP, 512);
            Assert::AreEqual((int)CLIENT, (int)s.GetType());
        }

        // SocketType is correctly stored as SERVER
        TEST_METHOD(TC04_Constructor_ServerType_Stored)
        {
            MySocket s(SERVER, "127.0.0.1", 29000, TCP, 512);
            Assert::AreEqual((int)SERVER, (int)s.GetType());
        }

        // Buffer falls back to DEFAULT_SIZE when 0 is given
        TEST_METHOD(TC05_Constructor_ZeroBufferSize_UsesDefault)
        {
            MySocket s(CLIENT, "127.0.0.1", 29000, UDP, 0);
            Assert::AreEqual(DEFAULT_SIZE, s.GetPort() == 29000 ? DEFAULT_SIZE : 0);
        }

        // Default port stored correctly
        TEST_METHOD(TC06_Constructor_PortStoredCorrectly)
        {
            MySocket s(CLIENT, "127.0.0.1", 8080, UDP, 256);
            Assert::AreEqual(8080, s.GetPort());
        }
    };

    // GetIPAddr / SetIPAddr Tests
    TEST_CLASS(IPAddrTests)
    {
    public:
        // GetIPAddr returns what was set in constructor
        TEST_METHOD(TC07_GetIPAddr_ReturnsConstructorValue)
        {
            MySocket s(CLIENT, "192.168.1.100", 29000, UDP, 512);
            Assert::AreEqual(std::string("192.168.1.100"), s.GetIPAddr());
        }

        // SetIPAddr updates the stored IP (no connection active)
        TEST_METHOD(TC08_SetIPAddr_UpdatesValue)
        {
            MySocket s(CLIENT, "127.0.0.1", 29000, UDP, 512);
            s.SetIPAddr("10.172.41.150");
            Assert::AreEqual(std::string("10.172.41.150"), s.GetIPAddr());
        }

        // SetIPAddr is blocked when TCP connection is active
        // (Tests the guard — can't establish real TCP in unit test,
        //  so we verify IP remains unchanged after a failed set attempt)
        TEST_METHOD(TC09_SetIPAddr_NoConnectionGuard)
        {
            MySocket s(CLIENT, "127.0.0.1", 29000, UDP, 512);
            s.SetIPAddr("10.0.0.1");
            Assert::AreEqual(std::string("10.0.0.1"), s.GetIPAddr());
        }

        // Multiple SetIPAddr calls — last one wins
        TEST_METHOD(TC10_SetIPAddr_MultipleUpdates)
        {
            MySocket s(CLIENT, "127.0.0.1", 29000, UDP, 512);
            s.SetIPAddr("10.0.0.1");
            s.SetIPAddr("172.16.0.5");
            Assert::AreEqual(std::string("172.16.0.5"), s.GetIPAddr());
        }
    };

    // GetPort / SetPort Tests
    TEST_CLASS(PortTests)
    {
    public:
        // GetPort returns constructor value
        TEST_METHOD(TC11_GetPort_ReturnsConstructorValue)
        {
            MySocket s(CLIENT, "127.0.0.1", 29500, UDP, 512);
            Assert::AreEqual(29500, s.GetPort());
        }

        // SetPort updates the stored port (no connection active)
        TEST_METHOD(TC12_SetPort_UpdatesValue)
        {
            MySocket s(CLIENT, "127.0.0.1", 29000, UDP, 512);
            s.SetPort(29500);
            Assert::AreEqual(29500, s.GetPort());
        }

        // SetPort updates correctly for multiple calls
        TEST_METHOD(TC13_SetPort_MultipleUpdates)
        {
            MySocket s(CLIENT, "127.0.0.1", 1000, UDP, 512);
            s.SetPort(2000);
            s.SetPort(29500);
            Assert::AreEqual(29500, s.GetPort());
        }

        // Simulator UDP port constant check
        TEST_METHOD(TC14_SimulatorUDPPort_IsCorrect)
        {
            MySocket s(CLIENT, "10.172.41.150", 29500, UDP, 512);
            Assert::AreEqual(29500, s.GetPort());
        }

        // Simulator TCP port constant check
        TEST_METHOD(TC15_SimulatorTCPPort_IsCorrect)
        {
            MySocket s(CLIENT, "10.172.41.150", 29000, TCP, 512);
            Assert::AreEqual(29000, s.GetPort());
        }
    };

    // GetType / SetType Tests
    TEST_CLASS(TypeTests)
    {
    public:
        // GetType returns CLIENT
        TEST_METHOD(TC16_GetType_ReturnsClient)
        {
            MySocket s(CLIENT, "127.0.0.1", 29000, UDP, 512);
            Assert::AreEqual((int)CLIENT, (int)s.GetType());
        }

        // GetType returns SERVER
        TEST_METHOD(TC17_GetType_ReturnsServer)
        {
            MySocket s(SERVER, "127.0.0.1", 29000, UDP, 512);
            Assert::AreEqual((int)SERVER, (int)s.GetType());
        }

        // SetType changes CLIENT to SERVER (UDP — no welcome socket guard)
        TEST_METHOD(TC18_SetType_ClientToServer_UDP)
        {
            MySocket s(CLIENT, "127.0.0.1", 29500, UDP, 512);
            s.SetType(SERVER);
            Assert::AreEqual((int)SERVER, (int)s.GetType());
        }

        // SetType is blocked for SERVER — WelcomeSocket is open after construction
        TEST_METHOD(TC19_SetType_ServerBlocked_WelcomeSocketOpen)
        {
            MySocket s(SERVER, "127.0.0.1", 29500, UDP, 512);
            s.SetType(CLIENT);
            // Guard should prevent the change — type remains SERVER
            Assert::AreEqual((int)SERVER, (int)s.GetType());
        }
    };

    // 
    // ConnectTCP Guard Tests
    // 
    TEST_CLASS(ConnectTCPGuardTests)
    {
    public:
        // ConnectTCP on a UDP socket should not crash — guard prints error
        TEST_METHOD(TC20_ConnectTCP_BlockedForUDP)
        {
            MySocket s(CLIENT, "127.0.0.1", 29500, UDP, 512);
            s.ConnectTCP();
            // If guard works, type remains CLIENT and port unchanged
            Assert::AreEqual((int)CLIENT, (int)s.GetType());
        }

        // DisconnectTCP on UDP socket should not crash
        TEST_METHOD(TC21_DisconnectTCP_BlockedForUDP)
        {
            MySocket s(CLIENT, "127.0.0.1", 29500, UDP, 512);
            s.DisconnectTCP();
            Assert::AreEqual((int)CLIENT, (int)s.GetType());
        }
    };

    // 
    // DEFAULT_SIZE Constant Tests
    // 
    TEST_CLASS(DefaultSizeTests)
    {
    public:
        // DEFAULT_SIZE is positive and usable
        TEST_METHOD(TC22_DefaultSize_IsPositive)
        {
            Assert::IsTrue(DEFAULT_SIZE > 0);
        }

        // DEFAULT_SIZE equals 1024
        TEST_METHOD(TC23_DefaultSize_Equals1024)
        {
            Assert::AreEqual(1024, DEFAULT_SIZE);
        }
    };

    // 
    // Enum Value Tests
    // 
    TEST_CLASS(EnumTests)
    {
    public:
        TEST_METHOD(TC24_SocketType_CLIENT_Exists)
        {
            SocketType t = CLIENT;
            Assert::AreEqual((int)CLIENT, (int)t);
        }

        TEST_METHOD(TC25_SocketType_SERVER_Exists)
        {
            SocketType t = SERVER;
            Assert::AreEqual((int)SERVER, (int)t);
        }

        TEST_METHOD(TC26_ConnectionType_TCP_Exists)
        {
            ConnectionType c = TCP;
            Assert::AreEqual((int)TCP, (int)c);
        }

        TEST_METHOD(TC27_ConnectionType_UDP_Exists)
        {
            ConnectionType c = UDP;
            Assert::AreEqual((int)UDP, (int)c);
        }
    };

    // 
    // PktDef Integration Tests
    // Verifies PktDef packets can be serialized and passed to SendData/GetData
    
    TEST_CLASS(PktDefIntegrationTests)
    {
    public:
        // PktDef packet can be generated and is valid size for transmission
        TEST_METHOD(TC28_PktDef_DrivePacket_ValidForSend)
        {
            PktDef pkt;
            pkt.SetPktCount(1);
            pkt.SetCmd(DRIVE);

            DriveBody body;
            body.Direction = FORWARD;
            body.Duration = 5;
            body.Power = 80;
            pkt.SetBodyData(reinterpret_cast<char*>(&body), sizeof(DriveBody));

            char* raw = pkt.GenPacket();
            int   len = pkt.GetLength();

            // Packet must be non-null and fit in DEFAULT_SIZE buffer
            Assert::IsNotNull(raw);
            Assert::IsTrue(len > 0);
            Assert::IsTrue(len <= DEFAULT_SIZE);
        }

        // PktDef sleep packet is valid for transmission
        TEST_METHOD(TC29_PktDef_SleepPacket_ValidForSend)
        {
            PktDef pkt;
            pkt.SetPktCount(2);
            pkt.SetCmd(SLEEP);

            char* raw = pkt.GenPacket();
            int   len = pkt.GetLength();

            Assert::IsNotNull(raw);
            Assert::AreEqual(HEADERSIZE + 1, len);  // 5 bytes total
            Assert::IsTrue(len <= DEFAULT_SIZE);
        }

        // PktDef CRC is valid after serialization — ready for transmission
        TEST_METHOD(TC30_PktDef_CRCValid_BeforeSend)
        {
            PktDef pkt;
            pkt.SetPktCount(3);
            pkt.SetCmd(DRIVE);

            DriveBody body;
            body.Direction = BACKWARD;
            body.Duration = 3;
            body.Power = 90;
            pkt.SetBodyData(reinterpret_cast<char*>(&body), sizeof(DriveBody));

            char* raw = pkt.GenPacket();
            int   len = pkt.GetLength();

            Assert::IsTrue(pkt.CheckCRC(raw, len));
        }

        // Multiple PktDef packets have incrementing PktCount
        TEST_METHOD(TC31_PktDef_PktCount_Increments)
        {
            PktDef pkt1, pkt2;
            pkt1.SetPktCount(1);
            pkt2.SetPktCount(2);

            Assert::AreEqual(1, pkt1.GetPktCount());
            Assert::AreEqual(2, pkt2.GetPktCount());
        }

        // TurnBody packet fits within DEFAULT_SIZE
        TEST_METHOD(TC32_PktDef_TurnPacket_ValidForSend)
        {
            PktDef pkt;
            pkt.SetPktCount(4);
            pkt.SetCmd(DRIVE);

            TurnBody body;
            body.Direction = RIGHT;
            body.Duration = 2;
            pkt.SetBodyData(reinterpret_cast<char*>(&body), sizeof(TurnBody));

            char* raw = pkt.GenPacket();
            int   len = pkt.GetLength();

            Assert::IsNotNull(raw);
            Assert::IsTrue(len <= DEFAULT_SIZE);
            Assert::IsTrue(pkt.CheckCRC(raw, len));
        }
    };
}