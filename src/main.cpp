/* Build
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build
*/
#include <iostream>
#include <cstdio>
#include <string>
#include <vector>
#include "zip_writer.hpp"
#include "zip_reader.hpp"
#include "cmd_pgz.hpp"

int main(int argc, char **argv)
{
    std::vector<std::string> args(argv + 1, argv + argc); // bỏ argv[0]

    if (args.empty() || args[0] == "--help" || args[0] == "-h")
    {
        print_usage(argv[0]);
        return args.empty() ? 1 : 0;
    }

    const std::string &cmd = args[0];
    if(cmd == "--version" || cmd == "-v")
        return cmd_version(args);
    if (cmd == "--create" || cmd == "-c")
        return cmd_create(args);
    if (cmd == "--list" || cmd == "-l")
        return cmd_list(args);
    if (cmd == "--extract" || cmd == "-e") 
        return cmd_extract(args);

    std::cerr << "Unknown command: " << cmd << "\n";
    print_usage(argv[0]);
    return 1;
}