#include "codec.h"

#include <iostream>
#include <cstdint>
#include <stdexcept>

static uint16_t EncodeByte(uint8_t byte) {
    bool b[13] = {false}; 

    b[3]  = (byte >> 0) & 0x1;
    b[5]  = (byte >> 1) & 0x1;
    b[6]  = (byte >> 2) & 0x1;
    b[7]  = (byte >> 3) & 0x1;
    b[9]  = (byte >> 4) & 0x1;
    b[10] = (byte >> 5) & 0x1;
    b[11] = (byte >> 6) & 0x1;
    b[12] = (byte >> 7) & 0x1;

    b[1] = b[3] ^ b[5] ^ b[7] ^ b[9] ^ b[11];
    b[2] = b[3] ^ b[6] ^ b[7] ^ b[10] ^ b[11];
    b[4] = b[5] ^ b[6] ^ b[7] ^ b[12];
    b[8] = b[9] ^ b[10] ^ b[11] ^ b[12];

    uint16_t cw{};
    for (int i = 1; i <= 12; ++i) {
        if (b[i]) {
            cw |= (1u << (i - 1));
        }
    }
    return cw;
}

static uint8_t DecodeCodeword(uint16_t cw, bool& error_hemming_code) {
    bool b[13];
    for (int i = 1; i <= 12; ++i) {
        b[i] = (cw >> (i - 1)) & 0x1;
    }

    bool c1 = b[1] ^ b[3] ^ b[5] ^ b[7] ^ b[9] ^ b[11];
    bool c2 = b[2] ^ b[3] ^ b[6] ^ b[7] ^ b[10] ^ b[11];
    bool c4 = b[4] ^ b[5] ^ b[6] ^ b[7] ^ b[12];
    bool c8 = b[8] ^ b[9] ^ b[10] ^ b[11] ^ b[12];

    int syndrome = (c1 ? 1 : 0) | (c2 ? 2 : 0) | (c4 ? 4 : 0) | (c8 ? 8 : 0);

    if (syndrome != 0) {
        if (syndrome >= 1 && syndrome <= 12) {
            b[syndrome] = !b[syndrome];
        } else {
            error_hemming_code = true;
        }
    }

    uint8_t byte = 0;
    byte |= (b[3]  ? 1u : 0u) << 0;
    byte |= (b[5]  ? 1u : 0u) << 1;
    byte |= (b[6]  ? 1u : 0u) << 2;
    byte |= (b[7]  ? 1u : 0u) << 3;
    byte |= (b[9]  ? 1u : 0u) << 4;
    byte |= (b[10] ? 1u : 0u) << 5;
    byte |= (b[11] ? 1u : 0u) << 6;
    byte |= (b[12] ? 1u : 0u) << 7;

    return byte;
}

void HammingEncodeStream(std::istream& in, std::ostream& out, uint64_t originalSize) {
    for (uint64_t i = 0; i < originalSize; ++i) {
        char ch = 0;
        if (!in.get(ch)) {
            throw std::runtime_error("Unexpected end of input while encoding");
        }
        uint8_t b = static_cast<uint8_t>(ch);
        uint16_t cw = EncodeByte(b);

        uint8_t lo = static_cast<uint8_t>(cw & 0xFF);
        uint8_t hi = static_cast<uint8_t>((cw >> 8) & 0xFF);

        out.put(static_cast<char>(lo));
        out.put(static_cast<char>(hi));
        if (!out) {
            throw std::runtime_error("Failed to write encoded data");
        }
    }
}

bool HammingDecodeStream(std::istream& in, std::ostream& out, uint64_t originalSize) {
    bool anyuncorrectable = false;

    for (uint64_t i = 0; i < originalSize; ++i) {
        unsigned char lo = 0, hi = 0;
        if (!in.read(reinterpret_cast<char*>(&lo), 1)) {
            throw std::runtime_error("Unexpected end of encoded data (lo)");
        }
        if (!in.read(reinterpret_cast<char*>(&hi), 1)) {
            throw std::runtime_error("Unexpected end of encoded data (hi)");
        }

        uint16_t cw = static_cast<uint16_t>(lo | (static_cast<uint16_t>(hi) << 8));
        bool error_hemming_code = false;
        uint8_t decoded = DecodeCodeword(cw, error_hemming_code);
        if (error_hemming_code) {
            anyuncorrectable = true;
        }

        out.put(static_cast<char>(decoded));
        if (!out) {
            throw std::runtime_error("Failed to write decoded data");
        }
    }

    return anyuncorrectable;
}