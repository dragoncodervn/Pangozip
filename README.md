"# Pangozip

**Tiếng Việt** | [English](README.en.md)

Pangozip (`pgz`) là một công cụ dòng lệnh viết bằng C++17 để **tạo, liệt kê và giải nén file ZIP**. Mã tự xử lý định dạng ZIP (header, Central Directory, CRC-32), còn việc nén/giải nén DEFLATE dùng thư viện zlib.

- **Tác giả:** Le Khanh Duy Nguyen - Ho Chi Minh City University of Teachnology and Engineering (theo thông tin trong `pgz --version`)
- **Phiên bản:** 0.0.1
- **Repository:** `dragoncodervn/Pangozip`

## Mục lục

- [Tính năng](#tính-năng)
- [Yêu cầu](#yêu-cầu)
- [Build](#build)
- [Cách sử dụng](#cách-sử-dụng)
- [Cấu trúc thư mục](#cấu-trúc-thư-mục)
- [Kiến trúc và chi tiết kỹ thuật](#kiến-trúc-và-chi-tiết-kỹ-thuật)
- [Hạn chế đã biết](#hạn-chế-đã-biết)
- [Ghi chú cho người phát triển](#ghi-chú-cho-người-phát-triển)

## Tính năng

| Lệnh | Mô tả |
|------|-------|
| `--create` / `-c` | Tạo file ZIP từ nhiều file hoặc thư mục (đệ quy). |
| `--list` / `-l` | Liệt kê nội dung ZIP: kích thước gốc, kích thước nén, tỉ lệ nén, phương thức, tên. |
| `--extract` / `-e` | Giải nén ZIP vào thư mục đích (mặc định là thư mục hiện tại). |
| `--version` / `-v` | In phiên bản và tác giả. |
| `--help` / `-h` | In hướng dẫn sử dụng. |

Chi tiết:

- **Nén thông minh:** mỗi file được nén bằng DEFLATE (zlib, level mặc định). Nếu bản nén không nhỏ hơn bản gốc, file được lưu nguyên (method `store`). File rỗng và thư mục luôn ở dạng `store`.
- **Kiểm tra toàn vẹn:** khi giải nén, CRC-32 của dữ liệu được tính lại và so với CRC trong ZIP. Nếu lệch, quá trình dừng và báo `CRC mismatch`.
- **Chống Zip Slip:** tên entry chứa đường dẫn tuyệt đối, ký tự ổ đĩa (`:`), dấu `\`, hoặc thành phần `..` sẽ bị từ chối.
- **Đọc đúng cấu trúc ZIP:** tìm End of Central Directory (EOCD) từ cuối file, rồi đọc Central Directory để lấy danh sách entry, sau đó đọc Local File Header để tìm dữ liệu.

## Yêu cầu

- Trình biên dịch C++17 (ví dụ build dùng `clang++`)
- [CMake](https://cmake.org/) từ 3.16 trở lên
- [Ninja](https://ninja-build.org/) (tùy chọn, nếu dùng `-G Ninja`)
- Thư viện **zlib** (CMake tìm bằng `find_package(ZLIB REQUIRED)`)

## Build

Từ thư mục gốc của dự án:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build
```

Kết quả là file `build\pgz.exe` (trên Windows) hoặc `build/pgz` (trên Linux/macOS).

## Cách sử dụng

```text
pgz --create <out.zip> <file|dir> [...]
pgz --list <file.zip>
pgz --extract <file.zip> [-d <dir>]
```

### Tạo ZIP

```powershell
# Nén nhiều file
pgz --create out.zip hello.txt hoho.txt

# Nén cả thư mục (giữ nguyên cấu trúc con)
pgz --create docs.zip docs
```

- Khi input là **file**, entry trong ZIP chỉ mang tên file (không có đường dẫn cha).
- Khi input là **thư mục**, mọi entry được đặt dưới một thư mục gốc cùng tên, ví dụ `docs/`, `docs/a.txt`, `docs/sub/b.txt`. Dấu `/` cuối tên đánh dấu thư mục.
- Nếu một input không tồn tại hoặc không đọc được, lệnh báo lỗi và không tạo file ZIP.

### Liệt kê ZIP

```powershell
pgz --list out.zip
```

Ví dụ output (thực tế từ `test\combine.zip`):

```text
        Size   Compressed   Ratio  Method   Name
         216           74   65.7%  deflate  hehe.txt
         327          107   67.3%  deflate  hoho.txt
         543          181   66.7%           2 file(s)
```

Dòng cuối là tổng kích thước và tỉ lệ nén toàn bộ archive. Tỉ lệ nén được tính theo công thức `100 × (1 − nén / gốc)`; file rỗng hiển thị `0.0%`.

### Giải nén

```powershell
# Giải nén vào thư mục hiện tại
pgz --extract out.zip

# Giải nén vào thư mục chỉ định (tự tạo nếu chưa có)
pgz --extract out.zip -d output
```

- Mỗi file đã giải nén được in ra dạng `extracted: <tên>`.
- File đích bị **ghi đè** nếu đã tồn tại, không có hỏi xác nhận.
- Giải nén dừng ngay tại entry lỗi đầu tiên. Các file trước đó đã được ghi ra đĩa và không bị xóa.

### Phiên bản và trợ giúp

```powershell
pgz --version
pgz --help
```

### Mã thoát

- `0`: thành công (`--help` cũng trả về `0`).
- `1`: lỗi tham số, lỗi đọc/ghi file, lỗi nén/giải nén, hoặc không có tham số nào.

## Cấu trúc thư mục

```text
Pangozip_Project/
├── CmakeLists.txt        # Cấu hình build CMake
├── README.md             # Tài liệu tiếng Việt
├── README.en.md          # Tài liệu tiếng Anh├── header/
│   ├── bytes.hpp         # Hàm đọc/ghi số nguyên little-endian (put16/32, get16/32)
│   ├── cmd_pgz.hpp       # Khai báo các lệnh CLI
│   ├── crc32.hpp         # Khai báo crc32_update()
│   ├── zip_reader.hpp    # struct ZipEntry, read_zip_entries(), extract_zip()
│   └── zip_writer.hpp    # create_zip()
├── src/
│   ├── main.cpp          # Điểm vào, phân tích tham số dòng lệnh
│   ├── cmd_pgz.cpp       # Triển khai các lệnh: create, list, extract, version, usage
│   ├── crc32.cpp         # CRC-32 bằng bảng tra 256 phần tử
│   ├── zip_writer.cpp    # Ghi Local Header, Central Directory, EOCD
│   └── zip_reader.cpp    # Đọc Central Directory và giải nén
└── test/                 # File mẫu và ZIP mẫu để thử nghiệm
```

Thư mục `build/` là kết quả build, không nên đưa vào mã nguồn.

## Kiến trúc và chi tiết kỹ thuật

### Luồng dữ liệu

1. **`main.cpp`** đọc `argv`, chọn lệnh và chuyển cho hàm tương ứng trong **`cmd_pgz.cpp`**.
2. **Tạo ZIP** (`create_zip`): duyệt các input, với mỗi file tính CRC-32, nén DEFLATE nếu có lợi, ghi Local Header + dữ liệu vào bộ nhớ. Sau đó ghi Central Directory và EOCD, rồi xuất ra file.
3. **Đọc ZIP** (`read_zip_entries`): đọc toàn bộ file vào bộ nhớ, tìm EOCD (quét lùi tối đa 65 535 byte để bỏ qua comment), đọc các Central Directory Header để lấy `ZipEntry`.
4. **Giải nén** (`extract_zip`): với mỗi entry, kiểm tra tên an toàn, đọc Local Header để xác định vị trí dữ liệu, giải nén (`store` hoặc `deflate`), kiểm tra CRC, rồi ghi file.

### Định dạng ZIP được dùng

- **Local File Header** (signature `0x04034b50`): version 20, flags 0, method, thời gian sửa đổi 0, ngày sửa đổi `0x21` (1980-01-01), CRC, kích thước nén và gốc, độ dài tên, extra field rỗng.
- **Central Directory Header** (signature `0x02014b50`): thêm version made by, external attributes (`0x10` cho thư mục), và offset tới Local Header.
- **End of Central Directory** (signature `0x06054b50`): số entry, kích thước và offset của Central Directory, không có comment.
- Dữ liệu DEFLATE là **raw deflate** (`deflateInit2` với `-MAX_WBITS`), không có header zlib.
- Các số được ghi theo **little-endian** qua các hàm trong [bytes.hpp](header/bytes.hpp).

### CRC-32

[crc32.cpp](src/crc32.cpp) dùng đa thức chuẩn `0xEDB88320`, bảng tra 256 phần tử, và đảo bit ở đầu và cuối. Hàm `crc32_update` có thể gọi nhiều lần để tính CRC trên dữ liệu đọc theo khối.

## Hạn chế đã biết

- **Không hỗ trợ ZIP64:** tối đa 65 535 entry và kích thước dữ liệu trong 32 bit (`create_zip` từ chối khi vượt số entry).
- **Chỉ hỗ trợ method 0 (store) và 8 (deflate)** khi giải nén. Các method khác bị báo lỗi `Unsupported method`.
- **Không hỗ trợ:** mã hóa/mật khẩu, comment của ZIP, extra field, timestamp thật (thời gian luôn là 1980-01-01 00:00), quyền truy cập file, symbolic link.
- **Đọc toàn bộ file vào RAM** khi tạo, liệt kê và giải nén. File ZIP lớn sẽ tốn nhiều bộ nhớ.
- **Tên entry có `:` hoặc `\`** bị từ chối khi giải nén, kể cả khi đó là tên hợp lệ trên Linux.
- **Không có tùy chọn** chọn mức nén, loại trừ file, hay chọn file cụ thể để giải nén.

## Ghi chú cho người phát triển

- Trong [zip_reader.cpp](src/zip_reader.cpp) vẫn còn các comment `TODO 1/2/3`, nhưng các phần đó đã được cài đặt. Có thể xóa các comment này.
- [zip_writer.cpp](src/zip_writer.cpp) có một khối comment giải thích CRC đặt bên trong `write_local`. Nội dung này nên chuyển thành comment ngắn hoặc xóa.
- [cmd_pgz.hpp](header/cmd_pgz.hpp) khai báo `static double ratio_percent`, nhưng hàm được định nghĩa trong `cmd_pgz.cpp`. Khai báo `static` trong header không có tác dụng với file khác, nên nên xóa khỏi header.
- File CMake đặt tên `CmakeLists.txt` (chữ `m` viết thường). Windows không phân biệt hoa thường nên build vẫn chạy, nhưng trên Linux/macOS CMake yêu cầu đúng tên `CMakeLists.txt`.
- Dự án chưa có file LICENSE.
" 
