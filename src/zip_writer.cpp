#include "zip_writer.hpp"
#include "crc32.hpp"
#include "bytes.hpp"
#include <fstream>
#include <vector>
#include <filesystem>
#include <zlib.h>

namespace fs = std::filesystem;

struct Entry {
    std::string name;
    uint16_t method;      // 0 = store, 8 = deflate
    uint32_t crc;
    uint32_t size;        // kích thước gốc
    uint32_t csize;       // kích thước sau nén
    uint32_t local_offset;
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

static void write_local(std::vector<uint8_t>& out, Entry& e, const std::vector<uint8_t>& data) {
    e.local_offset = (uint32_t)out.size();
    put32(out, 0x04034b50); // signature
    put16(out, 20);         // version needed
    put16(out, 0);          // flags
    put16(out, e.method);   // compression method
    put16(out, 0);          // mod time
    put16(out, 0x21);       // mod date
    put32(out, e.crc);
    put32(out, e.csize);    // compressed size
    put32(out, e.size);                // uncompressed size
    put16(out, (uint16_t)e.name.size());
    put16(out, 0);                     // extra field length
    out.insert(out.end(), e.name.begin(), e.name.end());
    out.insert(out.end(), data.begin(), data.end());
}

static void write_central(std::vector<uint8_t>& out, const Entry& e) {
    put32(out, 0x02014b50); // signature
    put16(out, 20);         // version made by
    put16(out, 20);         // version needed
    put16(out, 0);          // flags
    put16(out, e.method);   // method
    put16(out, 0);          // mod time
    put16(out, 0x21);       // mod date
    put32(out, e.crc);
    put32(out, e.csize);    // compressed size
    put32(out, e.size);     // uncompressed size
    put16(out, (uint16_t)e.name.size());
    put16(out, 0);          // extra field length
    put16(out, 0);          // comment length
    put16(out, 0);          // disk number start
    put16(out, 0);          // internal attrs
    put32(out, e.is_dir ? 0x10 : 0); // external attrs (0x10 = thư mục)
    put32(out, e.local_offset);
    out.insert(out.end(), e.name.begin(), e.name.end());
}

static void write_eocd(std::vector<uint8_t>& out, uint16_t count, uint32_t cd_offset, uint32_t cd_size) {
    put32(out, 0x06054b50); // signature
    put16(out, 0);          // this disk
    put16(out, 0);          // disk with central dir
    put16(out, count);      // entries on this disk
    put16(out, count);      // total entries
    put32(out, cd_size);
    put32(out, cd_offset);
    put16(out, 0);          // comment length
}

static void add_entry(std::vector<uint8_t>& out, std::vector<Entry>& entries,
                      const std::string& name, const std::vector<uint8_t>& data, bool is_dir) {
    Entry e{name, 0, crc32_update(0, data.data(), data.size()), (uint32_t)data.size(), 0, 0, is_dir};

    std::vector<uint8_t> packed;
    bool use_deflate = !is_dir && !data.empty() && deflate_data(data, packed) && packed.size() < data.size();
    const std::vector<uint8_t>& body = use_deflate ? packed : data;
    e.method = use_deflate ? 8 : 0;
    e.csize = (uint32_t)body.size();

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

bool create_zip(const std::string& zip_path, const std::vector<std::string>& inputs) {
    std::vector<uint8_t> out;
    std::vector<Entry> entries;

    for (const auto& input : inputs)
        if (!add_path(out, entries, input)) return false;

    if (entries.size() > 0xFFFF) return false; // chưa hỗ trợ ZIP64

    uint32_t cd_offset = (uint32_t)out.size();
    for (const auto& e : entries) write_central(out, e);
    uint32_t cd_size = (uint32_t)out.size() - cd_offset;

    write_eocd(out, (uint16_t)entries.size(), cd_offset, cd_size);

    std::ofstream f(zip_path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(out.data()), out.size());
    return true;
}