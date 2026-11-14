#include "zip_writer.hpp"
#include "crc32.hpp"
#include "bytes.hpp"
#include <fstream>
#include <vector>
#include <filesystem>
#include <zlib.h>
#include <cstdlib>   // getenv

namespace fs = std::filesystem;


static const uint64_t M32 = 0xFFFFFFFFull;
static bool g_force64 = false;   // dùng để test ZIP64 mà không cần file 4 GiB

struct Entry {
    std::string name;
    uint16_t method;
    uint32_t crc;
    uint64_t size;
    uint64_t csize;
    uint64_t local_offset;
    bool is_dir;
};

static bool read_file(const fs::path& p, std::vector<uint8_t>& data) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

static bool deflate_data(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
    z_stream strm{};
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return false;
    out.resize(deflateBound(&strm, in.size()));
    strm.next_in = const_cast<Bytef*>(in.data());
    strm.avail_in = (uInt)in.size();
    strm.next_out = out.data();
    strm.avail_out = (uInt)out.size();
    int ret = deflate(&strm, Z_FINISH);
    out.resize(strm.total_out);
    deflateEnd(&strm);
    return ret == Z_STREAM_END;
}

// static void write_local(std::vector<uint8_t>& out, Entry& e, const std::vector<uint8_t>& data) {
//     e.local_offset = (uint32_t)out.size();
//     put32(out, 0x04034b50); // signature
//     put16(out, 20);         // version needed
//     put16(out, 0);          // flags
//     put16(out, e.method);   // compression method
//     put16(out, 0);          // mod time
//     put16(out, 0x21);       // mod date
//     put32(out, e.crc);     
//     /*
//     CRC (Cyclic Redundancy Check) là mã kiểm tra lỗi. 
//     Nó tính ra một số nguyên ngắn (ở đây là 32 bit) từ dữ liệu, 
//     dùng để phát hiện dữ liệu bị hỏng do lỗi đọc/ghi hoặc truyền tải. 
//     Khi đọc lại, nếu CRC tính lại không khớp với CRC đã lưu thì dữ liệu đã bị thay đổi.
//     */
//     put32(out, e.csize);    // compressed size
//     put32(out, e.size);                // uncompressed size
//     put16(out, (uint16_t)e.name.size());
//     put16(out, 0);                     // extra field length
//     out.insert(out.end(), e.name.begin(), e.name.end());
//     out.insert(out.end(), data.begin(), data.end());
// } 

static void write_local(std::vector<uint8_t>& out, Entry& e, const std::vector<uint8_t>& data) {
    e.local_offset = out.size();
    bool z64 = g_force64 || e.size >= M32 || e.csize >= M32;

    put32(out, 0x04034b50);
    put16(out, z64 ? 45 : 20);      // version needed
    put16(out, 0);
    put16(out, e.method);
    put16(out, 0);
    put16(out, 0x21);
    put32(out, e.crc);
    put32(out, z64 ? 0xFFFFFFFF : (uint32_t)e.csize);
    put32(out, z64 ? 0xFFFFFFFF : (uint32_t)e.size);
    put16(out, (uint16_t)e.name.size());
    put16(out, z64 ? 20 : 0);       // extra length
    out.insert(out.end(), e.name.begin(), e.name.end());
    if (z64) {                      // local header phải có cả 2 size
        put16(out, 0x0001);
        put16(out, 16);
        put64(out, e.size);
        put64(out, e.csize);
    }
    out.insert(out.end(), data.begin(), data.end());
}

// static void write_central(std::vector<uint8_t>& out, const Entry& e) {
//     put32(out, 0x02014b50); // signature
//     put16(out, 20);         // version made by
//     put16(out, 20);         // version needed
//     put16(out, 0);          // flags
//     put16(out, e.method);   // method
//     put16(out, 0);          // mod time
//     put16(out, 0x21);       // mod date
//     put32(out, e.crc);
//     put32(out, e.csize);    // compressed size
//     put32(out, e.size);     // uncompressed size
//     put16(out, (uint16_t)e.name.size());
//     put16(out, 0);          // extra field length
//     put16(out, 0);          // comment length
//     put16(out, 0);          // disk number start
//     put16(out, 0);          // internal attrs
//     put32(out, e.is_dir ? 0x10 : 0); // external attrs (0x10 = thư mục)
//     put32(out, e.local_offset);
//     out.insert(out.end(), e.name.begin(), e.name.end());
// }

static void write_central(std::vector<uint8_t>& out, const Entry& e) {
    bool b_size = g_force64 || e.size >= M32;
    bool b_csz  = g_force64 || e.csize >= M32;
    bool b_off  = g_force64 || e.local_offset >= M32;
    uint16_t xlen = (b_size || b_csz || b_off)
                  ? (uint16_t)(4 + 8 * (b_size + b_csz + b_off)) : 0;

    put32(out, 0x02014b50);
    put16(out, xlen ? 45 : 20);     // version made by
    put16(out, xlen ? 45 : 20);     // version needed
    put16(out, 0);
    put16(out, e.method);
    put16(out, 0);
    put16(out, 0x21);
    put32(out, e.crc);
    put32(out, b_csz  ? 0xFFFFFFFF : (uint32_t)e.csize);
    put32(out, b_size ? 0xFFFFFFFF : (uint32_t)e.size);
    put16(out, (uint16_t)e.name.size());
    put16(out, xlen);
    put16(out, 0);                  // comment length
    put16(out, 0);
    put16(out, 0);
    put32(out, e.is_dir ? 0x10 : 0);
    put32(out, b_off ? 0xFFFFFFFF : (uint32_t)e.local_offset);
    out.insert(out.end(), e.name.begin(), e.name.end());
    if (xlen) {                     // thứ tự bắt buộc: size, csize, offset
        put16(out, 0x0001);
        put16(out, xlen - 4);
        if (b_size) put64(out, e.size);
        if (b_csz)  put64(out, e.csize);
        if (b_off)  put64(out, e.local_offset);
    }
}

// static void write_eocd(std::vector<uint8_t>& out, uint16_t count, uint32_t cd_offset, uint32_t cd_size) {
//     put32(out, 0x06054b50); // signature
//     put16(out, 0);          // this disk
//     put16(out, 0);          // disk with central dir
//     put16(out, count);      // entries on this disk
//     put16(out, count);      // total entries
//     put32(out, cd_size);
//     put32(out, cd_offset);
//     put16(out, 0);          // comment length
// }

static void write_eocd(std::vector<uint8_t>& out, uint64_t count,
                       uint64_t cd_offset, uint64_t cd_size) {
    bool z64 = g_force64 || count >= 0xFFFF || cd_offset >= M32 || cd_size >= M32;
    if (z64) {
        uint64_t z64_off = out.size();
        // Zip64 EOCD record
        put32(out, 0x06064b50);
        put64(out, 44);             // kích thước phần còn lại của record
        put16(out, 45);             // version made by
        put16(out, 45);             // version needed
        put32(out, 0);              // this disk
        put32(out, 0);              // disk with central dir
        put64(out, count);
        put64(out, count);
        put64(out, cd_size);
        put64(out, cd_offset);
        // Zip64 EOCD locator
        put32(out, 0x07064b50);
        put32(out, 0);              // disk chứa Zip64 EOCD
        put64(out, z64_off);
        put32(out, 1);              // tổng số disk
    }
    put32(out, 0x06054b50);
    put16(out, 0);
    put16(out, 0);
    put16(out, z64 ? 0xFFFF : (uint16_t)count);
    put16(out, z64 ? 0xFFFF : (uint16_t)count);
    put32(out, z64 ? 0xFFFFFFFF : (uint32_t)cd_size);
    put32(out, z64 ? 0xFFFFFFFF : (uint32_t)cd_offset);
    put16(out, 0);
}

static void add_entry(std::vector<uint8_t>& out, std::vector<Entry>& entries,
                      const std::string& name, const std::vector<uint8_t>& data, bool is_dir) {
    Entry e{name, 0, crc32_update(0, data.data(), data.size()),
            (uint64_t)data.size(), 0, 0, is_dir};

    std::vector<uint8_t> packed;
    bool use_deflate = !is_dir && !data.empty()
                       && data.size() <= 0xFFFFFFFFull          // vừa với uInt của zlib
                       && deflate_data(data, packed)
                       && packed.size() < data.size();
    const std::vector<uint8_t>& body = use_deflate ? packed : data;
    e.method = use_deflate ? 8 : 0;
    e.csize  = body.size();

    write_local(out, e, body);
    entries.push_back(e);
}

static bool add_path(std::vector<uint8_t>& out, std::vector<Entry>& entries, const std::string& input_str) {
    fs::path input = input_str;
    if (fs::is_regular_file(input)) {
        std::vector<uint8_t> data;
        if (!read_file(input, data)) return false;
        add_entry(out, entries, input.filename().generic_string(), data, false);
        return true;
    }
    if (!fs::is_directory(input)) return false;

    fs::path root = input.lexically_normal();
    if (root.filename().empty()) root = root.parent_path(); // xử lý "docs/"
    std::string root_name = root.filename().generic_string();

    add_entry(out, entries, root_name + "/", {}, true);
    for (const auto& it : fs::recursive_directory_iterator(input)) {
        const fs::path& p = it.path();
        std::string name = (fs::path(root_name) / p.lexically_relative(input)).generic_string();
        if (it.is_directory()) {
            add_entry(out, entries, name + "/", {}, true);
        } else if (it.is_regular_file()) {
            std::vector<uint8_t> data;
            if (!read_file(p, data)) return false;
            add_entry(out, entries, name, data, false);
        }
    }
    return true;
}

// bool create_zip(const std::string& zip_path, const std::vector<std::string>& inputs) {
//     std::vector<uint8_t> out;
//     std::vector<Entry> entries;

//     for (const auto& input : inputs)
//         if (!add_path(out, entries, input)) return false;

//     if (entries.size() > 0xFFFF) return false; // chưa hỗ trợ ZIP64

//     uint32_t cd_offset = (uint32_t)out.size();
//     for (const auto& e : entries) write_central(out, e);
//     uint32_t cd_size = (uint32_t)out.size() - cd_offset;

//     write_eocd(out, (uint16_t)entries.size(), cd_offset, cd_size);

//     std::ofstream f(zip_path, std::ios::binary);
//     if (!f) return false;
//     f.write(reinterpret_cast<const char*>(out.data()), out.size());
//     return true;
//}

// Remove the limited of 32bit zip
bool create_zip(const std::string& zip_path, const std::vector<std::string>& inputs) {
    g_force64 = std::getenv("PGZ_FORCE_ZIP64") != nullptr;
    std::vector<uint8_t> out;
    std::vector<Entry> entries;

    for (const auto& input : inputs)
        if (!add_path(out, entries, input)) return false;

    uint64_t cd_offset = out.size();
    for (const auto& e : entries) write_central(out, e);
    uint64_t cd_size = out.size() - cd_offset;

    write_eocd(out, entries.size(), cd_offset, cd_size);
    
    std::ofstream f(zip_path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(out.data()), out.size());
    return true;
}