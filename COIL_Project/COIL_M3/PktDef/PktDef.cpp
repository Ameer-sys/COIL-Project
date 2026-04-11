// PktDef.cpp
// CSCN72050 - COIL Project Milestone 1
// Application Layer Protocol - PktDef Class Implementation

#include "PktDef.h"

// Private Helper: CountBits
// Counts the total number of '1' bits across numBytes bytes of a buffer.
// Used for CRC calculation (bit-count parity algorithm).
int PktDef::CountBits(char* buffer, int numBytes) const {
    int count = 0;
    for (int i = 0; i < numBytes; i++) {
        unsigned char byte = static_cast<unsigned char>(buffer[i]);
        while (byte) {
            count += (byte & 1);    // add LSB
            byte >>= 1;             // shift right
        }
    }
    return count;
}

// Default Constructor
// Places the PktDef object in a safe (all-zero) state:
//   - All header fields = 0
//   - Data pointer = nullptr
//   - CRC = 0
//   - RawBuffer = nullptr
PktDef::PktDef() {
    memset(&Packet.header, 0, sizeof(Header));
    Packet.Data = nullptr;
    Packet.CRC = 0;
    RawBuffer = nullptr;
}

// Overloaded Constructor
// Takes a RAW data buffer, parses it, and populates the Header, Body, and CRC.
//
// Buffer layout: [Header(4 bytes)][Body(n bytes)][CRC(1 byte)]
// Body size is derived from: Length - HEADERSIZE - 1
PktDef::PktDef(char* rawData) {
    RawBuffer = nullptr;
    Packet.Data = nullptr;

    // 1. Copy the 4-byte header directly from the buffer
    memcpy(&Packet.header, rawData, HEADERSIZE);

    // 2. Calculate body size from the Length field
    int bodySize = static_cast<int>(Packet.header.Length) - HEADERSIZE - 1;

    // 3. Copy body data (if any)
    if (bodySize > 0) {
        Packet.Data = new char[bodySize];
        memcpy(Packet.Data, rawData + HEADERSIZE, bodySize);
    }
    else {
        Packet.Data = nullptr;
    }

    // 4. Copy CRC � always the last byte of the packet
    Packet.CRC = rawData[Packet.header.Length - 1];
}

// Destructor
// Frees heap-allocated memory for Data and RawBuffer.
PktDef::~PktDef() {
    if (Packet.Data != nullptr) {
        delete[] Packet.Data;
        Packet.Data = nullptr;
    }
    if (RawBuffer != nullptr) {
        delete[] RawBuffer;
        RawBuffer = nullptr;
    }
}

// SetCmd
// Sets exactly one command flag based on CmdType; clears all other flags.
// Drive, Status, and Sleep should never be set simultaneously.
void PktDef::SetCmd(CmdType cmd) {
    // Clear all command flags before setting the new one
    Packet.header.Drive = 0;
    Packet.header.Status = 0;
    Packet.header.Sleep = 0;

    switch (cmd) {
    case DRIVE:    Packet.header.Drive = 1; break;
    case SLEEP:    Packet.header.Sleep = 1; break;
    case RESPONSE: Packet.header.Status = 1; break;
    }
}

// SetBodyData
// Allocates the packet's body, copies 'size' bytes from 'data' into it,
// and updates the Length header field:
//   Length = HEADERSIZE + body_size + 1 (for CRC byte)
void PktDef::SetBodyData(char* data, int size) {
    // Free any existing body allocation
    if (Packet.Data != nullptr) {
        delete[] Packet.Data;
        Packet.Data = nullptr;
    }

    if (data != nullptr && size > 0) {
        Packet.Data = new char[size];
        memcpy(Packet.Data, data, size);
    }

    // Update Length: header + body + 1 CRC byte
    Packet.header.Length = static_cast<unsigned char>(HEADERSIZE + size + 1);
}

// SetPktCount
// Sets the packet counter in the header.
void PktDef::SetPktCount(int count) {
    Packet.header.PktCount = static_cast<unsigned short>(count);
}

// GetCmd
// Returns the CmdType by checking which command flag is set.
// Defaults to RESPONSE if neither Drive nor Sleep is set.
CmdType PktDef::GetCmd() {
    if (Packet.header.Drive == 1) return DRIVE;
    if (Packet.header.Sleep == 1) return SLEEP;
    return RESPONSE;
}

// GetAck
// Returns true if the Ack flag is set in the header.
bool PktDef::GetAck() {
    return Packet.header.Ack == 1;
}

// GetLength
// Returns the total packet length from the Length header field.
int PktDef::GetLength() {
    return static_cast<int>(Packet.header.Length);
}

// GetBodyData
// Returns a pointer to the packet's body data, or nullptr if no body.
char* PktDef::GetBodyData() {
    return Packet.Data;
}

// GetPktCount
// Returns the packet counter value from the header.
int PktDef::GetPktCount() {
    return static_cast<int>(Packet.header.PktCount);
}

// CheckCRC
// Validates the CRC of a raw buffer.
// Algorithm: count all '1' bits in every byte EXCEPT the last byte (the CRC),
// then compare the count to the value stored in the final byte.
//
// Parameters:
//   buffer - pointer to the raw packet buffer
//   size   - total size of the buffer in bytes (includes CRC byte)
// Returns:
//   true  if calculated CRC matches stored CRC
//   false otherwise
bool PktDef::CheckCRC(char* buffer, int size) {
    if (buffer == nullptr || size <= 0) return false;

    // Count bits in all bytes except the last (CRC) byte
    int calculatedCRC = CountBits(buffer, size - 1);

    // Compare to the stored CRC (last byte of buffer)
    return (static_cast<unsigned char>(calculatedCRC)
        == static_cast<unsigned char>(buffer[size - 1]));
}

// CalcCRC
// Calculates the CRC for the current packet (Header + Body) and stores the
// result in Packet.CRC.
//
// Algorithm (from spec): count the total number of '1' bits in the packet,
// excluding the CRC byte itself.
void PktDef::CalcCRC() {
    int bodySize = 0;
    if (Packet.header.Length > 0) {
        bodySize = static_cast<int>(Packet.header.Length) - HEADERSIZE - 1;
        if (bodySize < 0) bodySize = 0;
    }

    int dataSize = HEADERSIZE + bodySize;   // bytes to count (no CRC yet)
    char* temp = new char[dataSize];

    // Copy header into temp buffer
    memcpy(temp, &Packet.header, sizeof(Header));

    // Copy body into temp buffer (if any)
    if (bodySize > 0 && Packet.Data != nullptr) {
        memcpy(temp + HEADERSIZE, Packet.Data, bodySize);
    }

    // Count all '1' bits
    Packet.CRC = static_cast<char>(CountBits(temp, dataSize));

    delete[] temp;
}

// GenPacket
// Assembles the complete raw packet into RawBuffer:
//   [Header(4 bytes)][Body(n bytes)][CRC(1 byte)]
//
// Steps:
//   1. If Length is not yet set, compute it (no body: HEADERSIZE + 1)
//   2. Allocate (or reallocate) RawBuffer
//   3. Copy Header bytes
//   4. Copy Body bytes (if any)
//   5. CalcCRC() and append the CRC byte
//
// Returns: pointer to the allocated RawBuffer
char* PktDef::GenPacket() {
    // Free previous buffer if it exists
    if (RawBuffer != nullptr) {
        delete[] RawBuffer;
        RawBuffer = nullptr;
    }

    // If Length was never set (e.g., SLEEP with no body), set it now
    if (Packet.header.Length == 0) {
        Packet.header.Length = static_cast<unsigned char>(HEADERSIZE + 1);
    }

    int totalSize = static_cast<int>(Packet.header.Length);
    int bodySize = totalSize - HEADERSIZE - 1;
    if (bodySize < 0) bodySize = 0;

    // Allocate the raw buffer
    RawBuffer = new char[totalSize];
    memset(RawBuffer, 0, totalSize);

    // Copy header into buffer
#pragma warning(suppress: 6386)
    memcpy(RawBuffer, &Packet.header, sizeof(Header));

    // Copy body data into buffer
    if (bodySize > 0 && Packet.Data != nullptr) {
        memcpy(RawBuffer + HEADERSIZE, Packet.Data, bodySize);
    }

    // Calculate CRC over header + body, then append
    CalcCRC();
    RawBuffer[totalSize - 1] = Packet.CRC;

    return RawBuffer;
}