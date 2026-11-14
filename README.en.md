# Pangozip

[Tiếng Việt](README.md) | **English**

Pangozip (`pgz`) is a command-line tool written in C++17 for **creating, listing, and extracting ZIP files**, including the **ZIP64** extension (files of 4 GiB or more, or archives with more than 65 535 entries). The ZIP format itself (headers, Central Directory, CRC-32) is handled by the project's own code, while DEFLATE compression and decompression use the zlib library.

- **Author:** Le Khanh Duy Nguyen - Ho Chi Minh City University of Teachnology and Engineering (as printed by `pgz --version`)
- **Version:** 0.0.2
- **Repository:** `dragoncodervn/Pangozip`

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Build](#build)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Architecture and Technical Details](#architecture-and-technical-details)
- [Known Limitations](#known-limitations)
- [Notes for Developers](#notes-for-developers)

## Features

| Command | Description |
|---------|-------------|
| `--create` / `-c` | Create a ZIP file from one or more files or directories (recursive). |
| `--list` / `-l` | List archive contents: original size, compressed size, compression ratio, method, and name. |
| `--extract` / `-e` | Extract a ZIP file into a target directory (defaults to the current directory). |
| `--version` / `-v` | Print the version and author. |
| `--help` / `-h` | Print usage information. |

Details:

- **Smart compression:** each file is compressed with DEFLATE (zlib, default level). If the compressed output is not smaller than the original, the file is stored as-is (method `store`). Empty files and directories are always stored.
- **Integrity check:** when extracting, the CRC-32 of the decompressed data is recomputed and compared with the CRC in the archive. On a mismatch, extraction stops and reports `CRC mismatch`.
- **Zip Slip protection:** entry names containing absolute paths, drive letters (`:`), backslashes (`\`), or `..` components are rejected.
- **Format-aware reading:** the End of Central Directory (EOCD) record is located by scanning backward from the end of the file, the Central Directory is read to get the entry list, and each Local File Header is read to locate the entry data.
- **ZIP64 support:** when an archive exceeds 32-bit limits (an entry size or offset of 4 GiB or more, or 65 535 or more entries), `--create` writes ZIP64 records, and only when needed. `--list` and `--extract` read ZIP64 archives. Files of 4 GiB or more are stored (method `store`) without compression.

## Requirements

- A C++17 compiler (the build example uses `clang++`)
- [CMake](https://cmake.org/) 3.16 or newer
- [Ninja](https://ninja-build.org/) (optional, if you use `-G Ninja`)
- The **zlib** library (found by CMake through `find_package(ZLIB REQUIRED)`)

## Build

From the project root:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build
```

The output is `build\pgz.exe` on Windows or `build/pgz` on Linux/macOS.

## Usage

```text
pgz --create <out.zip> <file|dir> [...]
pgz --list <file.zip>
pgz --extract <file.zip> [-d <dir>]
```

### Create an archive

```powershell
# Archive several files
pgz --create out.zip hello.txt hoho.txt

# Archive a whole directory (keeps its subdirectory structure)
pgz --create docs.zip docs
```

- When the input is a **file**, the entry name is only the file name (no parent path).
- When the input is a **directory**, all entries are placed under a root folder with the same name, for example `docs/`, `docs/a.txt`, `docs/sub/b.txt`. A trailing `/` marks a directory entry.
- If any input does not exist or cannot be read, the command reports an error and does not create the ZIP file.

### List an archive

```powershell
pgz --list out.zip
```

Example output (actual output from `test\combine.zip`):

```text
        Size   Compressed   Ratio  Method   Name
         216           74   65.7%  deflate  hehe.txt
         327          107   67.3%  deflate  hoho.txt
         543          181   66.7%           2 file(s)
```

The last line shows the totals for the whole archive. The compression ratio is calculated as `100 × (1 − compressed / original)`; empty files show `0.0%`.

### Extract an archive

```powershell
# Extract into the current directory
pgz --extract out.zip

# Extract into a specific directory (created if it does not exist)
pgz --extract out.zip -d output
```

- Each extracted file is printed as `extracted: <name>`.
- Existing destination files are **overwritten** without confirmation.
- Extraction stops at the first failing entry. Files already extracted before that point remain on disk.

### Test ZIP64

To exercise the ZIP64 code path without a 4 GiB file, set the `PGZ_FORCE_ZIP64` environment variable (any value) before creating the archive:

```powershell
$env:PGZ_FORCE_ZIP64 = "1"
pgz --create zip64.zip hello.txt
Remove-Item Env:PGZ_FORCE_ZIP64

pgz --list zip64.zip
pgz --extract zip64.zip -d out64
```

### Version and help

```powershell
pgz --version
pgz --help
```

### Exit codes

- `0`: success (`--help` also returns `0`).
- `1`: invalid arguments, file read/write error, compression or decompression error, or no arguments given.

## Project Structure

```text
Pangozip_Project/
├── CmakeLists.txt        # CMake build configuration
├── README.md             # Vietnamese documentation
├── README.en.md          # English documentation
├── header/
│   ├── bytes.hpp         # Little-endian integer read/write helpers (put16/32, get16/32)
│   ├── cmd_pgz.hpp       # CLI command declarations
│   ├── crc32.hpp         # crc32_update() declaration
│   ├── zip_reader.hpp    # struct ZipEntry, read_zip_entries(), extract_zip()
│   └── zip_writer.hpp    # create_zip()
├── src/
│   ├── main.cpp          # Entry point, command-line dispatch
│   ├── cmd_pgz.cpp       # Command implementations: create, list, extract, version, usage
│   ├── crc32.cpp         # CRC-32 using a 256-entry lookup table
│   ├── zip_writer.cpp    # Writes Local Headers, Central Directory, and EOCD
│   └── zip_reader.cpp    # Reads the Central Directory and extracts entries
└── test/                 # Sample files and ZIP archives for testing
```

The `build/` directory holds build output and should not be committed as source.

## Architecture and Technical Details

### Data flow

1. **`main.cpp`** reads `argv`, selects the command, and calls the matching function in **`cmd_pgz.cpp`**.
2. **Create** (`create_zip`): walks the inputs, computes the CRC-32 of each file, compresses it with DEFLATE when that helps, and writes the Local Header and data into memory (adding the ZIP64 extra field when needed). Then it writes the Central Directory and EOCD (adding a Zip64 EOCD when needed), and saves the result to disk.
3. **Read** (`read_zip_entries`): loads the whole file into memory, scans backward for the EOCD (up to 65 535 bytes, to skip over a comment). If the EOCD fields overflow, it reads the Zip64 EOCD through the locator. It then parses each Central Directory header into a `ZipEntry`, taking 64-bit values from the ZIP64 extra field when the 32-bit field is `0xFFFFFFFF`.
4. **Extract** (`extract_zip`): for each entry, checks that the name is safe, reads the Local Header to find the data, decompresses it (`store` or `deflate`), verifies the CRC, and then writes the file.

### ZIP format details

- **Local File Header** (signature `0x04034b50`): version 20 (or 45 when ZIP64 is used), flags 0, method, modification time 0, modification date `0x21` (1980-01-01), CRC, compressed and uncompressed sizes, and name length. With ZIP64, the 32-bit sizes are set to `0xFFFFFFFF` and the extra field holds the 64-bit sizes.
- **Central Directory Header** (signature `0x02014b50`): adds version made by, external attributes (`0x10` for directories), and the offset of the Local Header. When ZIP64 is needed, values that exceed 32 bits are set to `0xFFFFFFFF` and stored in extra field ID `0x0001`, in the order: uncompressed size, compressed size, offset.
- **End of Central Directory** (signature `0x06054b50`): entry count, size and offset of the Central Directory, and no comment. If there are 65 535 or more entries, or the size/offset is 4 GiB or more, the EOCD fields are set to `0xFFFF`/`0xFFFFFFFF` and a **Zip64 EOCD record** (signature `0x06064b50`) and **Zip64 EOCD locator** (signature `0x07064b50`) are written before it.
- DEFLATE data is **raw deflate** (`deflateInit2` with `-MAX_WBITS`), without a zlib header.
- All numbers are written in **little-endian** order using the helpers in [bytes.hpp](header/bytes.hpp).

### CRC-32

[crc32.cpp](src/crc32.cpp) uses the standard polynomial `0xEDB88320`, a 256-entry lookup table, and bit inversion before and after the computation. `crc32_update` can be called repeatedly to compute the CRC over data read in chunks.

## Known Limitations

- **Multi-disk (split) archives are not supported:** only single-disk archives are written and read.
- **Extraction supports only method 0 (store) and method 8 (deflate).** Other methods fail with `Unsupported method`.
- **Not supported:** encryption or passwords, ZIP comments, extra fields, real timestamps (the time is always 1980-01-01 00:00), file permissions, and symbolic links.
- **The whole file is loaded into memory** when creating, listing, or extracting. Large archives use a lot of RAM.
- **Entry names containing `:` or `\` are rejected** during extraction, even when they would be valid on Linux.
- **No options** for compression level, excluding files, or extracting selected files only.
- **File names with Vietnamese diacritics are not supported** when compressing and extracting (for example `tiếng.txt`, `ăn cơm.txt`). When creating a ZIP, the command reports `failed to create` or stops with `Illegal byte sequence`. The content of files containing Vietnamese text (UTF-8) is still compressed and extracted correctly. Rename such files to names without diacritics before archiving.

## Notes for Developers

- The `TODO` comments in [zip_reader.cpp](src/zip_reader.cpp) and [crc32.cpp](src/crc32.cpp) describe code that is already implemented. These comments can be removed.
- [zip_writer.cpp](src/zip_writer.cpp) still has many old code blocks commented out (`write_local`, `write_central`, `write_eocd`, and the old `create_zip`). These blocks can be removed.
- The CMake file is named `CmakeLists.txt` (lowercase `m`). Windows is case-insensitive, so the build still works there, but Linux and macOS require the exact name `CMakeLists.txt`.
- The project does not have a LICENSE file yet.
