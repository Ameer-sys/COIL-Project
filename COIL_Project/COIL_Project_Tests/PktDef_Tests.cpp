// =============================================================================
// PktDefTest.cpp
// CSCN72050 - COIL Project Milestone 1
// MSTest Unit Tests for PktDef class
// =============================================================================

#include "pch.h"
#include "CppUnitTest.h"
#include "../COIL_Project/PktDef.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PktDefTests
{
    // 
    // Default Constructor Tests
    // 
    TEST_CLASS(DefaultConstructorTests)
    {
    public:
        TEST_METHOD(TC01_DefaultCtor_PktCountIsZero)
        {
            PktDef pkt;
            Assert::AreEqual(0, pkt.GetPktCount());
        }

        TEST_METHOD(TC02_DefaultCtor_LengthIsZero)
        {
            PktDef pkt;
            Assert::AreEqual(0, pkt.GetLength());
        }

        TEST_METHOD(TC03_DefaultCtor_BodyDataIsNull)
        {
            PktDef pkt;
            Assert::IsNull(pkt.GetBodyData());
        }

        TEST_METHOD(TC04_DefaultCtor_AckIsFalse)
        {
            PktDef pkt;
            Assert::IsFalse(pkt.GetAck());
        }
    };

    // SetCmd / GetCmd Tests
    // 
    TEST_CLASS(SetGetCmdTests)
    {
    public:
        TEST_METHOD(TC05_SetCmd_Drive)
        {
            PktDef pkt;
            pkt.SetCmd(DRIVE);
            Assert::AreEqual((int)DRIVE, (int)pkt.GetCmd());
        }

        TEST_METHOD(TC06_SetCmd_Sleep)
        {
            PktDef pkt;
            pkt.SetCmd(SLEEP);
            Assert::AreEqual((int)SLEEP, (int)pkt.GetCmd());
        }

        TEST_METHOD(TC07_SetCmd_Response)
        {
            PktDef pkt;
            pkt.SetCmd(RESPONSE);
            Assert::AreEqual((int)RESPONSE, (int)pkt.GetCmd());
        }

        // Calling SetCmd twice should clear the previous flag
        TEST_METHOD(TC08_SetCmd_OverwriteClearsPreviousFlag)
        {
            PktDef pkt;
            pkt.SetCmd(DRIVE);
            pkt.SetCmd(SLEEP);
            Assert::AreEqual((int)SLEEP, (int)pkt.GetCmd());
        }

        TEST_METHOD(TC09_SetGetPktCount)
        {
            PktDef pkt;
            pkt.SetPktCount(42);
            Assert::AreEqual(42, pkt.GetPktCount());
        }
    };

    // 
    // SetBodyData / GetBodyData / GetLength Tests
    // 
    TEST_CLASS(BodyDataTests)
    {
    public:
        // HEADERSIZE(4) + DriveBody(3) + CRC(1) = 8
        TEST_METHOD(TC10_SetBodyData_DriveBody_LengthCorrect)
        {
            PktDef pkt;
            pkt.SetCmd(DRIVE);

            DriveBody body;
            body.Direction = FORWARD;
            body.Duration = 5;
            body.Power = 80;
            pkt.SetBodyData(reinterpret_cast<char*>(&body), sizeof(DriveBody));

            Assert::AreEqual(HEADERSIZE + (int)sizeof(DriveBody) + 1, pkt.GetLength());
        }

        TEST_METHOD(TC11_SetBodyData_DriveBody_DataPreserved)
        {
            PktDef pkt;
            pkt.SetCmd(DRIVE);

            DriveBody body;
            body.Direction = FORWARD;
            body.Duration = 10;
            body.Power = 100;
            pkt.SetBodyData(reinterpret_cast<char*>(&body), sizeof(DriveBody));

            DriveBody* ret = reinterpret_cast<DriveBody*>(pkt.GetBodyData());
            Assert::IsNotNull(ret);
            Assert::AreEqual((int)FORWARD, (int)ret->Direction);
            Assert::AreEqual(10, (int)ret->Duration);
            Assert::AreEqual(100, (int)ret->Power);
        }

        // HEADERSIZE(4) + TurnBody(3) + CRC(1) = 8
        TEST_METHOD(TC12_SetBodyData_TurnBody_LengthCorrect)
        {
            PktDef pkt;
            pkt.SetCmd(DRIVE);

            TurnBody body;
            body.Direction = RIGHT;
            body.Duration = 2;
            pkt.SetBodyData(reinterpret_cast<char*>(&body), sizeof(TurnBody));

            Assert::AreEqual(HEADERSIZE + (int)sizeof(TurnBody) + 1, pkt.GetLength());
        }

        TEST_METHOD(TC13_SetBodyData_TurnBody_DataPreserved)
        {
            PktDef pkt;
            pkt.SetCmd(DRIVE);

            TurnBody body;
            body.Direction = LEFT;
            body.Duration = 4;
            pkt.SetBodyData(reinterpret_cast<char*>(&body), sizeof(TurnBody));

            TurnBody* ret = reinterpret_cast<TurnBody*>(pkt.GetBodyData());
            Assert::IsNotNull(ret);
            Assert::AreEqual((int)LEFT, (int)ret->Direction);
            Assert::AreEqual(4, (int)ret->Duration);
        }

        // Calling SetBodyData twice should overwrite the first body
        TEST_METHOD(TC14_SetBodyData_Overwrite_ReplacesData)
        {
            PktDef pkt;
            pkt.SetCmd(DRIVE);

            DriveBody first;
            first.Direction = FORWARD; first.Duration = 5; first.Power = 80;
            pkt.SetBodyData(reinterpret_cast<char*>(&first), sizeof(DriveBody));

            DriveBody second;
            second.Direction = BACKWARD; second.Duration = 2; second.Power = 90;
            pkt.SetBodyData(reinterpret_cast<char*>(&second), sizeof(DriveBody));

            DriveBody* ret = reinterpret_cast<DriveBody*>(pkt.GetBodyData());
            Assert::AreEqual((int)BACKWARD, (int)ret->Direction);
            Assert::AreEqual(90, (int)ret->Power);
        }
    };

    // 
    // Header / Struct Size Tests
    // 
    TEST_CLASS(HeaderSizeTests)
    {
    public:
        TEST_METHOD(TC15_HEADERSIZE_EqualsFour)
        {
            Assert::AreEqual(4, HEADERSIZE);
        }

        // Verifies #pragma pack is working - no compiler padding
        TEST_METHOD(TC16_SizeofHeader_EqualsFour)
        {
            Assert::AreEqual(HEADERSIZE, (int)sizeof(Header));
        }

        TEST_METHOD(TC17_SizeofDriveBody_EqualsThree)
        {
            Assert::AreEqual(3, (int)sizeof(DriveBody));
        }

        TEST_METHOD(TC18_SizeofTurnBody_EqualsThree)
        {
            Assert::AreEqual(3, (int)sizeof(TurnBody));
        }
    };

    // CRC Tests
    TEST_CLASS(CRCTests)
    {
    public:
        // CRC stored in last byte must equal manual bit count of all prior bytes
        TEST_METHOD(TC19_CalcCRC_MatchesManualBitCount)
        {
            PktDef pkt;
            pkt.SetPktCount(1);
            pkt.SetCmd(DRIVE);

            DriveBody body;
            body.Direction = FORWARD;
            body.Duration = 10;
            body.Power = 80;
            pkt.SetBodyData(reinterpret_cast<char*>(&body), sizeof(DriveBody));

            char* raw = pkt.GenPacket();
            int   len = pkt.GetLength();

            int manualCount = 0;
            for (int i = 0; i < len - 1; i++) {
                unsigned char byte = static_cast<unsigned char>(raw[i]);
                while (byte) { manualCount += (byte & 1); byte >>= 1; }
            }

            Assert::AreEqual((unsigned int)manualCount,
                (unsigned int)(unsigned char)raw[len - 1]);
        }

        TEST_METHOD(TC20_CheckCRC_ValidPacket_ReturnsTrue)
        {
            PktDef pkt;
            pkt.SetPktCount(5);
            pkt.SetCmd(DRIVE);

            DriveBody body;
            body.Direction = BACKWARD; body.Duration = 3; body.Power = 90;
            pkt.SetBodyData(reinterpret_cast<char*>(&body), sizeof(DriveBody));

            char* raw = pkt.GenPacket();
            Assert::IsTrue(pkt.CheckCRC(raw, pkt.GetLength()));
        }

        TEST_METHOD(TC21_CheckCRC_CorruptedPacket_ReturnsFalse)
        {
            PktDef pkt;
            pkt.SetPktCount(3);
            pkt.SetCmd(DRIVE);

            DriveBody body;
            body.Direction = FORWARD; body.Duration = 7; body.Power = 85;
            pkt.SetBodyData(reinterpret_cast<char*>(&body), sizeof(DriveBody));

            char* raw = pkt.GenPacket();
            raw[HEADERSIZE] ^= 0xFF;   // corrupt body

            Assert::IsFalse(pkt.CheckCRC(raw, pkt.GetLength()));
        }

        TEST_METHOD(TC22_CheckCRC_SleepPacket_Valid)
        {
            PktDef pkt;
            pkt.SetPktCount(2);
            pkt.SetCmd(SLEEP);

            char* raw = pkt.GenPacket();
            Assert::IsTrue(pkt.CheckCRC(raw, pkt.GetLength()));
        }
    };

    // GenPacket Tests
    TEST_CLASS(GenPacketTests)
    {
    public:
        // SLEEP packet: no body -> length = HEADERSIZE + CRC = 5
        TEST_METHOD(TC23_GenPacket_SleepPacket_LengthFive)
        {
            PktDef pkt;
            pkt.SetPktCount(1);
            pkt.SetCmd(SLEEP);
            pkt.GenPacket();

            Assert::AreEqual(HEADERSIZE + 1, pkt.GetLength());
        }

        // Drive flag must be bit 0 of the flags byte (offset 2)
        TEST_METHOD(TC24_GenPacket_DriveFlagSetInRawBuffer)
        {
            PktDef pkt;
            pkt.SetPktCount(1);
            pkt.SetCmd(DRIVE);

            DriveBody body;
            body.Direction = FORWARD; body.Duration = 5; body.Power = 80;
            pkt.SetBodyData(reinterpret_cast<char*>(&body), sizeof(DriveBody));

            char* raw = pkt.GenPacket();
            unsigned char flags = static_cast<unsigned char>(raw[2]);
            Assert::AreEqual(1u, (unsigned int)(flags & 0x01));
        }

        // Body must start immediately after the 4-byte header
        TEST_METHOD(TC25_GenPacket_BodyPlacedAfterHeader)
        {
            PktDef pkt;
            pkt.SetPktCount(1);
            pkt.SetCmd(DRIVE);

            DriveBody body;
            body.Direction = FORWARD; body.Duration = 10; body.Power = 80;
            pkt.SetBodyData(reinterpret_cast<char*>(&body), sizeof(DriveBody));

            char* raw = pkt.GenPacket();
            DriveBody* parsed = reinterpret_cast<DriveBody*>(raw + HEADERSIZE);

            Assert::AreEqual((int)FORWARD, (int)parsed->Direction);
            Assert::AreEqual(10, (int)parsed->Duration);
            Assert::AreEqual(80, (int)parsed->Power);
        }
    };

    // Overloaded Constructor (Raw Buffer Parsing) Tests
    TEST_CLASS(OverloadedConstructorTests)
    {
    public:
        TEST_METHOD(TC26_OverloadedCtor_ParsesPktCount)
        {
            PktDef sender;
            sender.SetPktCount(99);
            sender.SetCmd(SLEEP);

            PktDef receiver(sender.GenPacket());
            Assert::AreEqual(99, receiver.GetPktCount());
        }

        TEST_METHOD(TC27_OverloadedCtor_ParsesDriveCmd)
        {
            PktDef sender;
            sender.SetPktCount(10);
            sender.SetCmd(DRIVE);

            DriveBody body;
            body.Direction = FORWARD; body.Duration = 5; body.Power = 80;
            sender.SetBodyData(reinterpret_cast<char*>(&body), sizeof(DriveBody));

            PktDef receiver(sender.GenPacket());
            Assert::AreEqual((int)DRIVE, (int)receiver.GetCmd());
        }

        TEST_METHOD(TC28_OverloadedCtor_ParsesSleepCmd)
        {
            PktDef sender;
            sender.SetPktCount(20);
            sender.SetCmd(SLEEP);

            PktDef receiver(sender.GenPacket());
            Assert::AreEqual((int)SLEEP, (int)receiver.GetCmd());
        }

        // Full round-trip: build -> serialize -> parse -> verify body
        TEST_METHOD(TC29_OverloadedCtor_RoundTrip_DriveBodyData)
        {
            PktDef sender;
            sender.SetPktCount(5);
            sender.SetCmd(DRIVE);

            DriveBody body;
            body.Direction = FORWARD; body.Duration = 7; body.Power = 95;
            sender.SetBodyData(reinterpret_cast<char*>(&body), sizeof(DriveBody));

            PktDef receiver(sender.GenPacket());
            DriveBody* parsed = reinterpret_cast<DriveBody*>(receiver.GetBodyData());

            Assert::IsNotNull(parsed);
            Assert::AreEqual((int)FORWARD, (int)parsed->Direction);
            Assert::AreEqual(7, (int)parsed->Duration);
            Assert::AreEqual(95, (int)parsed->Power);
        }

        // SLEEP has no body - parsed object should have nullptr body
        TEST_METHOD(TC30_OverloadedCtor_SleepHasNoBody)
        {
            PktDef sender;
            sender.SetPktCount(1);
            sender.SetCmd(SLEEP);

            PktDef receiver(sender.GenPacket());
            Assert::IsNull(receiver.GetBodyData());
        }

        // GetAck returns true when Ack bit is set in the raw buffer
        TEST_METHOD(TC31_OverloadedCtor_AckBitParsed)
        {
            // Manually build a 5-byte ACK packet:
            // [PktCount=1 (2B)] [Flags: Drive=1,Ack=1 -> 0x09] [Length=5] [CRC]
            unsigned char raw[5] = { 0x01, 0x00, 0x09, 0x05, 0x00 };

            int crc = 0;
            for (int i = 0; i < 4; i++) {
                unsigned char b = raw[i];
                while (b) { crc += (b & 1); b >>= 1; }
            }
            raw[4] = static_cast<unsigned char>(crc);

            PktDef pkt(reinterpret_cast<char*>(raw));
            Assert::IsTrue(pkt.GetAck());
        }
    };
}