#pragma once
#include <cstdint>
#include <string>
#include <vector>


struct ZipEntry {
    std::string name;
    uint16_t method;
    uint32_t crc;
    uint64_t comp_size;
    uint64_t uncomp_size;
    uint64_t local_offset;
};

bool read_zip_entries(const std::string& zip_path, std::vector<ZipEntry>& entries);
// Giải nén toàn bộ zip vào dest_dir. Trả về false nếu có lỗi.
bool extract_zip(const std::string& zip_path, const std::string& dest_dir);