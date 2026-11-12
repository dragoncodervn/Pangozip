#include "crc32.hpp"
#include <filesystem>
#include <iostream>
#include <zlib.h>
#include "zip_reader.hpp"
#include "bytes.hpp"
#include <cstdint>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

bool read_zip_entries(const std::string& zip_path, std::vector<ZipEntry>& entries) {
    std::ifstream in(zip_path, std::ios::binary);
    if (!in) return false;
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());

    if (buf.size() < 22) return false;

    // TODO 1: tìm EOCD từ cuối file lùi lên
    // EOCD nằm cách cuối file tối đa 22 + 65535 byte (do comment dài tối đa 65535)
    size_t eocd = SIZE_MAX;
    size_t max_back = buf.size() - 22;
    if (max_back > 65535) max_back = 65535;
    for (size_t back = 0; back <= max_back; ++back) {
        size_t pos = buf.size() - 22 - back;
        if (get32(buf, pos) == 0x06054b50) { eocd = pos; break; }
    }
    if (eocd == SIZE_MAX) return false;

    uint16_t total   = get16(buf, eocd + 10);
    uint32_t cd_size = get32(buf, eocd + 12);
    uint32_t cd_off  = get32(buf, eocd + 16);
    if ((uint64_t)cd_off + cd_size > buf.size()) return false;

    size_t pos = cd_off;
    for (uint16_t i = 0; i < total; ++i) {
        if (pos + 46 > buf.size()) return false;
        if (get32(buf, pos) != 0x02014b50) return false;

        ZipEntry e;
        e.method       = get16(buf, pos + 10);
        e.crc          = get32(buf, pos + 16);
        e.comp_size    = get32(buf, pos + 20);
        e.uncomp_size  = get32(buf, pos + 24);
        uint16_t nlen  = get16(buf, pos + 28);
        uint16_t xlen  = get16(buf, pos + 30);
        uint16_t clen  = get16(buf, pos + 32);
        e.local_offset = get32(buf, pos + 42);

        // TODO 2: kiểm tra đủ dữ liệu rồi lấy tên
        if (pos + 46 + nlen > buf.size()) return false;
        e.name.assign(buf.begin() + pos + 46, buf.begin() + pos + 46 + nlen);

        entries.push_back(e);

        // TODO 3: nhảy tới entry kế tiếp
        pos += 46 + nlen + xlen + clen;
    }
    return true;
}

// Chống Zip Slip: từ chối đường dẫn tuyệt đối, ổ đĩa, '\' và thành phần ".."
static bool is_safe_name(const std::string& name) {
    if (name.empty()) return false;
    if (name[0] == '/' || name[0] == '\\') return false;
    if (name.find(':') != std::string::npos) return false;
    if (name.find('\\') != std::string::npos) return false;
    for (const auto& part : fs::path(name)) {
        if (part == "..") return false;
    }
    return true;
}

// Giải nén raw deflate (khớp với deflateInit2 dùng -MAX_WBITS ở writer)
static bool inflate_data(const uint8_t* src, size_t src_len,
                         std::vector<uint8_t>& out, size_t expected) {
    out.assign(expected, 0);
    if (expected == 0) return true;

    z_stream strm{};
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) return false;
    strm.next_in   = const_cast<Bytef*>(src);
    strm.avail_in  = (uInt)src_len;
    strm.next_out  = out.data();
    strm.avail_out = (uInt)out.size();
    int ret = inflate(&strm, Z_FINISH);
    bool ok = (ret == Z_STREAM_END) && (strm.total_out == expected);
    inflateEnd(&strm);
    return ok;
}

bool extract_zip(const std::string& zip_path, const std::string& dest_dir) {
    std::vector<ZipEntry> entries;
    if (!read_zip_entries(zip_path, entries)) {
        std::cerr << "Error: cannot read zip file " << zip_path << "\n";
        return false;
    }

    std::ifstream in(zip_path, std::ios::binary);
    if (!in) return false;
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());

    fs::path root = dest_dir;
    std::error_code ec;
    fs::create_directories(root, ec);

    for (const auto& e : entries) {
        if (!is_safe_name(e.name)) {
            std::cerr << "Skipped unsafe entry: " << e.name << "\n";
            return false;
        }
        fs::path target = root / e.name;

        // Thư mục: tên kết thúc bằng '/'
        if (e.name.back() == '/') {
            fs::create_directories(target, ec);
            continue;
        }

        // Đọc Local Header để tìm chỗ bắt đầu dữ liệu
        size_t lo = e.local_offset;
        if (lo + 30 > buf.size() || get32(buf, lo) != 0x04034b50) {
            std::cerr << "Bad local header: " << e.name << "\n";
            return false;
        }
        uint16_t nlen = get16(buf, lo + 26);
        uint16_t xlen = get16(buf, lo + 28);
        size_t data_start = lo + 30 + nlen + xlen;
        if (data_start + e.comp_size > buf.size()) {
            std::cerr << "Truncated data: " << e.name << "\n";
            return false;
        }
        const uint8_t* src = buf.data() + data_start;

        std::vector<uint8_t> data;
        if (e.method == 0) {
            data.assign(src, src + e.comp_size);
        } else if (e.method == 8) {
            if (!inflate_data(src, e.comp_size, data, e.uncomp_size)) {
                std::cerr << "Inflate failed: " << e.name << "\n";
                return false;
            }
        } else {
            std::cerr << "Unsupported method " << e.method << ": " << e.name << "\n";
            return false;
        }

        // TODO: kiểm tra CRC
        uint32_t crc = crc32_update(0, data.data(), data.size());
        //   nếu crc != e.crc thì in "CRC mismatch: <tên>" ra std::cerr và return false
        if (crc != e.crc)
        {
            std::cerr<<"CRC mismatch: "<< e.name;
            return false;
        }

        fs::create_directories(target.parent_path(), ec);
        std::ofstream f(target, std::ios::binary);
        if (!f) {
            std::cerr << "Cannot write: " << target.string() << "\n";
            return false;
        }
        f.write(reinterpret_cast<const char*>(data.data()), data.size());
        std::cout << "extracted: " << e.name << "\n";
    }
    return true;
}