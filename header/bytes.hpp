#pragma once
#include <cstdint>
#include <vector>

inline void put16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(v & 0xFF);
    out.push_back((v >> 8) & 0xFF);
}

inline void put32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(v & 0xFF);
    out.push_back((v >> 8) & 0xFF);
    out.push_back((v >> 16) & 0xFF);
    out.push_back((v >> 24) & 0xFF);
}

inline uint16_t get16(const std::vector<uint8_t>& b, size_t pos) {
    return (uint16_t)(b[pos] | (b[pos + 1] << 8));
}

inline uint32_t get32(const std::vector<uint8_t>& b, size_t pos) {
    return (uint32_t)b[pos]
         | ((uint32_t)b[pos + 1] << 8)
         | ((uint32_t)b[pos + 2] << 16)
         | ((uint32_t)b[pos + 3] << 24);
}