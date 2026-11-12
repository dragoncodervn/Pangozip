#pragma once
#include <cstddef>
#include <cstdint>

// Dùng: uint32_t crc = 0; crc = crc32_update(crc, buf, n);
// Có thể gọi nhiều lần để tính CRC cho dữ liệu đọc theo từng khối.
uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len);