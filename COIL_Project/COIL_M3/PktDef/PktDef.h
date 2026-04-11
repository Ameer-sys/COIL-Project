// PktDef.h
// CSCN72050 - COIL Project Milestone 1
// Application Layer Protocol Packet Definition

#pragma once
#include <cstring>
#include <cstdint>

// Command Type Enumeration
enum CmdType { DRIVE, SLEEP, RESPONSE };

// Direction Constants
const int FORWARD = 1;
const int BACKWARD = 2;
const int RIGHT = 3;
const int LEFT = 4;

// Header Size Constant (calculated by hand)
//   PktCount  = 2 bytes (unsigned short)
//   Flags     = 1 byte  (Drive:1, Status:1, Sleep:1, Ack:1, Padding:4)
//   Length    = 1 byte  (unsigned char)
//   HEADERSIZE = 4 bytes
const int HEADERSIZE = 4;

// Packed Structures (no compiler padding)
#pragma pack(push, 1)

// Header structure: 4 bytes total
struct Header {
    unsigned short  PktCount;       // 2 bytes: incrementing packet counter
    struct {
        unsigned char Drive : 1;  // bit 0 - Drive command flag
        unsigned char Status : 1;  // bit 1 - Status/telemetry request flag
        unsigned char Sleep : 1;  // bit 2 - Sleep command flag
        unsigned char Ack : 1;  // bit 3 - Acknowledgement flag
        unsigned char Padding : 4;  // bits 4-7 - unused
    };
    unsigned char Length;           // 1 byte: total packet size in bytes
};

// Drive Body for FORWARD / BACKWARD commands (3 bytes)
struct DriveBody {
    unsigned char Direction;        // 1 byte: FORWARD=1 or BACKWARD=2
    unsigned char Duration;         // 1 byte: seconds to execute command
    unsigned char Power;            // 1 byte: motor speed 80-100%
};

// Turn Body for LEFT / RIGHT commands (3 bytes)
struct TurnBody {
    unsigned char  Direction;       // 1 byte:  RIGHT=3 or LEFT=4
    unsigned short Duration;        // 2 bytes: seconds to execute turn
};

#pragma pack(pop)

// PktDef Class
// Handles creation, parsing, validation, and serialization of robot packets.
class PktDef {
private:
    // Internal packet representation
    struct CmdPacket {
        Header  header;             // Packet header (4 bytes)
        char* Data;               // Pointer to body payload (heap-allocated)
        char    CRC;                // 1-byte CRC checksum (bit-count parity)
    };

    CmdPacket Packet;               // The packet being built/parsed
    char* RawBuffer;            // Serialized packet for transmission

    // Helper: count all '1' bits in a raw buffer of given byte length
    int CountBits(char* buffer, int numBytes) const;

public:
    // Constructors & Destructor

    // Default constructor: places object in a safe (all-zero) state
    PktDef();

    // Overloaded constructor: parses a raw buffer and populates Header/Body/CRC
    PktDef(char* rawData);

    // Destructor: frees any heap-allocated memory
    ~PktDef();

    // Mutators (Set Functions)

    // Sets the command flag (DRIVE, SLEEP, or RESPONSE) � clears all other flags
    void SetCmd(CmdType cmd);

    // Copies size bytes from data into the packet body; updates Length field
    void SetBodyData(char* data, int size);

    // Sets the PktCount header field
    void SetPktCount(int count);

    // Accessors (Get / Query Functions)

    // Returns the CmdType based on which command flag is set
    CmdType GetCmd();

    // Returns true if the Ack flag is set in the header
    bool GetAck();

    // Returns the Length field value (total packet size in bytes)
    int GetLength();

    // Returns a pointer to the body data (or nullptr if no body)
    char* GetBodyData();

    // Returns the PktCount header value
    int GetPktCount();

    // CRC Functions
    // Validates the CRC of a raw buffer (last byte = CRC to verify against)
    // Returns true if CRC matches, false otherwise
    bool CheckCRC(char* buffer, int size);

    // Calculates the CRC of the current packet and stores it in Packet.CRC
    void CalcCRC();

    // Serialization
    // Assembles Header + Body + CRC into RawBuffer; returns pointer to buffer
    char* GenPacket();
};