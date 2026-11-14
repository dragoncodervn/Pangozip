#include <iostream>
#include <cstdio>
#include <string>
#include <vector>
#include "zip_writer.hpp"
#include "zip_reader.hpp"
#include "cmd_pgz.hpp"

void print_usage(const char *prog)
{
    std::cout << "Usage:\n"
              << "  " << prog << " --create <out.zip> <file|dir> [...]\n"
              << "  " << prog << " --list <file.zip>\n"
              << "  " << prog << " --extract <file.zip> [-d <dir>]\n";
}

int cmd_create(const std::vector<std::string> &args)
{
    // args: create <out.zip> <input>...
    if (args.size() < 3)
    {
        print_usage("pgz");
        return 1;
    }
    std::vector<std::string> inputs(args.begin() + 2, args.end());
    if (!create_zip(args[1], inputs))
    {
        std::cerr << "Error: failed to create " << args[1] << " (check input paths)\n";
        return 1;
    }
    std::cout << "Created " << args[1] << "\n";
    return 0;
}

static double ratio_percent(uint64_t size, uint64_t comp) {
    if (size == 0) return 0.0;               // thư mục hoặc file rỗng: tránh chia cho 0
    return 100.0 * (1.0 - (double)comp / (double)size);
}

int cmd_list(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        print_usage("pgz");
        return 1;
    }
    std::vector<ZipEntry> entries;
    if (!read_zip_entries(args[1], entries)) {
        std::cerr << "Error: cannot read zip file " << args[1] << "\n";
        return 1;
    }

    std::printf("%12s %12s %7s  %-8s %s\n", "Size", "Compressed", "Ratio", "Method", "Name");

    uint64_t total_size = 0, total_comp = 0;
    for (const auto& e : entries) {
        const char* method = e.method == 0 ? "store"
                           : e.method == 8 ? "deflate" : "?";
        std::printf("%12llu %12llu %6.1f%%  %-8s %s\n",
            (unsigned long long)e.uncomp_size, (unsigned long long)e.comp_size,
            ratio_percent(e.uncomp_size, e.comp_size), method, e.name.c_str());
        total_size += e.uncomp_size;
        total_comp += e.comp_size;
    }

    std::printf("%12llu %12llu %6.1f%%  %-8s %zu file(s)\n",
                (unsigned long long)total_size,
                (unsigned long long)total_comp,
                ratio_percent(total_size, total_comp),
                "", entries.size());
    return 0;
}

int cmd_extract(const std::vector<std::string>& args) {
    // args: extract <file.zip> [-d <thu_muc>]
    if (args.size() < 2) {
        print_usage("pgz");
        return 1;
    }
    std::string dest = ".";
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "-d" && i + 1 < args.size()) {
            dest = args[++i];
        } else {
            std::cerr << "Unknown option: " << args[i] << "\n";
            return 1;
        }
    }
    return extract_zip(args[1], dest) ? 0 : 1;
}

int cmd_version(const std::vector<std::string>& args) {
    if(args.size() < 1)
    {
        print_usage("pgz");
        return 1;
    }
    printf("PANGOZIP VERSION 0.0.2\n");
    printf("Author: Le Khanh Duy Nguyen - Ho Chi Minh City University of Teachnology and Engineering\n");
    return 0;
}