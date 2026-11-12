#include "crc32.hpp"
#include <array>

static std::array<uint32_t, 256> make_table() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        // TODO 1: lặp 8 lần
        //   nếu (c & 1) thì c = 0xEDB88320 ^ (c >> 1)
        //   ngược lại   thì c = c >> 1
        for (int bit = 0; bit < 8; bit++)
        {
            if(c & 1) c = 0xEDB88320 ^ (c >> 1);
            else c = c >> 1;
        }
        table[i] = c;
    }
    return table;
}

uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    static const auto table = make_table();   // chỉ tạo một lần

    crc = ~crc;
    for (size_t i = 0; i < len; ++i) {
        // TODO 2: crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}