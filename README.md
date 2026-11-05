# Firmware QR check-in — mạch tím ESP32-S3

Firmware **0.1.10** đã nạp trên board: QR màu đỏ phủ gần hết màn 240×240, logo lớn ở giữa, ECDSA P-256/SHA-256, NTP và chu kỳ mặc định 30 giây. Bộ key và tối đa 5 mạng Wi-Fi lưu trong NVS; bật nguồn tự thử các mạng đã lưu, ưu tiên mạng lưu gần nhất. Xem [hướng dẫn Wi-Fi](docs/WIFI_SETUP_USB.md). Đã có thử kết nối HTTPS ping/pong có chữ ký tới Cloudflare Durable Object mỗi 30 giây; xem [cloudflare/README.md](cloudflare/README.md). `SETUP_REPORT.md` và `FIRMWARE_REVIEW.md` là báo cáo các giai đoạn trước.

Đã kiểm tra 108 lượt giải mã ảnh từ chính bộ vẽ firmware, 23 test giao thức/web và thử Worker/DO. Board thật đã nhận pong; trạng thái trên Cloudflare báo online. Lệnh quản lý từ xa và tích hợp check-in thật trên website sự kiện chưa hoàn thành.

Trạng thái kết nối thử: [DO device status](https://vnfarm-qrcode-device-link.dong141220047.workers.dev/api/devices/7ce8b1b17e60/status). Đây là endpoint server quản lý, riêng với URL khách quét QR.

Repo chỉ chứa mã nguồn, công cụ test và tài liệu. Không chứa private key thiết bị, mật khẩu Wi-Fi đã nhập, cơ sở dữ liệu chạy thử hay bản sao flash.

## Đích QR hiện tại

- Người dùng yêu cầu QR mở `https://vnfam.999.vn/#/`.
- Board hiện nạp environment `s3tim`, URL mặc định là địa chỉ sự kiện trên. Bản này tự chuyển URL LAN test cũ đã lưu sang địa chỉ mới, giữ Wi-Fi và cặp khóa.
- QR vẫn có thời gian, TTL và chữ ký. Query nằm trước fragment: `https://vnfam.999.vn/?v=1&dev=...&kid=...&iat=...&ttl=...&sig=...#/`.
- Log trực tiếp đã xác nhận đích URL mới và chu kỳ 30 giây. Việc xác minh/check-in trên web sự kiện cần tích hợp ở frontend/backend; đổi URL không tự thêm tính năng đó cho website.

## Web test cục bộ — cập nhật theo phạm vi hiện tại

Đã có web test **key và kết nối**, không cần ảnh vật phẩm. Xem `web/README.md` để chạy và test với board thật.

- Máy tính: `http://127.0.0.1:8765/` để đăng ký public key.
- URL QR trên mạng Wi-Fi hiện tại: `http://192.168.1.221:8765/checkin`.
- Firmware test LAN dùng environment `s3tim_local`; bản thông thường `s3tim` yêu cầu HTTPS và hiện đã nạp lên board với URL sự kiện ở trên.
- Bản test LAN được đóng gói ở `dist/s3tim_local/`. Khi nạp để thử với web này, dùng `python -m platformio run -e s3tim_local -t upload --upload-port COM5`.
- Web kiểm tra public key, chữ ký, hạn QR và liệt kê cổng USB; không theo dõi trực tiếp heartbeat/Wi-Fi của thiết bị.
- Backend thật và lệnh admin đổi khóa/xóa Wi-Fi vẫn cần thông tin tích hợp; web này là công cụ thử cục bộ.

## Phần cứng đã xác minh

- Bootloader COM5: ESP32-S3 QFN56 revision v0.2; flash 16 MB quad, PSRAM 8 MB; USB Serial/JTAG.
- Mạch trong sơ đồ người dùng: XH-S3E-AI_V1.0, màn hình TFT 1.54 inch ST7789.
- Đấu dây theo `docs/s3tim154st7789.jpg`:

| Màn hình | Mạch tím |
| --- | --- |
| GND | GND |
| VCC | 3V3 |
| SCL | IO42 |
| SDA | IO41 |
| RST | RXD / GPIO44 |
| DC | TXD / GPIO43 |
| CS | GND |
| BL | 3V3 |

SCL/SDA trên module này là tín hiệu SPI clock/data, không dùng I2C. UART0 không được khởi tạo trên GPIO43/44 vì hai chân đó đang điều khiển màn hình. Log qua USB CDC.

## Flow bản nền

1. Chưa có cấu hình: Wi-Fi thiết lập `VNFARM-xxxxxx` hiện trong danh sách mạng trên điện thoại. Mật khẩu cố định theo người dùng: `12345678` (bản 0.1.4). Màn hình thiết bị hiện tên mạng, mật khẩu và QR mở trang cấu hình.
2. Chọn mạng đó trong cài đặt Wi-Fi và nhập mật khẩu trên màn hình; sau khi kết nối, quét QR để mở `http://192.168.4.1/`, lấy public key và chỉnh thiết lập. QR chỉ chứa URL trang cấu hình, không chứa tên/mật khẩu mạng. Captive portal vẫn hỗ trợ mở trang tự động nếu điện thoại cho phép.
3. Copy hoặc tải public key, đăng ký với phía server. Private key tự sinh trên thiết bị và lưu lại, không đưa ra trang web.
4. Nhập Wi-Fi mới hoặc chọn mạng đã lưu, URL check-in thật, chu kỳ đổi QR. Kiểm tra kết nối Wi-Fi thành công rồi mới lưu và khởi động lại. Tối đa 5 mạng; nhập lại cùng SSID sẽ cập nhật mật khẩu.
5. Lần bật sau: tự nối Wi-Fi, lấy giờ chuẩn, tạo chữ ký và hiển thị QR động. Trang thiết lập và Wi-Fi AP tắt trong chế độ chạy thường.
6. Chưa có giờ chuẩn, chưa có URL hoặc mất mạng: không phát QR check-in.

Để trống URL khi thiết lập sẽ dùng URL mặc định của environment (bản `s3tim` dùng website sự kiện). Có thể đổi mặc định ở `include/AppConfig.h` rồi build lại; giá trị lưu sẵn trong NVS được ưu tiên. Gửi `SETUP_WIFI` kèm newline qua USB để mở lại thiết lập mạng, xem `docs/WIFI_SETUP_USB.md`. Chưa có lệnh admin từ xa.

## Đã chọn để triển khai thử, chưa phải quyết định của lead

- PlatformIO + Arduino ESP32; thuật toán ECDSA P-256/SHA-256; khóa tự sinh; giao thức QR ở `docs/QR_PROTOCOL.md`.
- Chu kỳ mặc định 30 giây, dải nhập 10–60; NTP dùng `time.google.com`, `pool.ntp.org`.
- Màn hình 240×240, rotation 0, inversion bật, SPI mode 3. Người dùng đã xác nhận bản 0.1.1 hiện chữ/Wi-Fi sau khi đổi từ SPI mode 0 sang mode 3; QR/hướng/màu cần thử tiếp.
- Chính sách bản nền: dừng QR khi mất Wi-Fi, không tự mở lại trang thiết lập trong chế độ chạy thường.
- Khóa lưu trong NVS thường, chưa cấu hình flash encryption/secure boot. Đây không phải bản firmware đã gia cố bảo vệ trước truy cập vật lý.

## Phần còn chờ để hoàn thành yêu cầu lead

- URL check-in và xác nhận phía server chấp nhận định dạng chữ ký/QR hiện tại, hoặc cung cấp định dạng cần thay thế.
- Endpoint/giao thức/xác thực nhận lệnh admin. Chưa triển khai nhận lệnh đổi khóa và xóa Wi-Fi; không giả lập kết nối thành công.
- Quy trình đăng ký public key mới khi đổi khóa.
- Xác nhận phương án tự sinh khóa là phương án cuối cùng; hội thoại có đổi ý về nhập private key.
- Thử trên màn hình thật, thử quét QR, rút/cắm nguồn và tích hợp server.

Không thêm camera, nhận diện ảnh, thưởng, chatbot hoặc cập nhật OTA firmware. Tính năng server kiểm tra/check-in và chặn quét tem thuộc web/backend.

## Build

Trong PowerShell:

```powershell
Set-Location 'F:\vnfarm\qrcode'
python -m platformio run -e s3tim
python -m unittest discover -s tests -v
```

Test verifier cần Python package `cryptography` (máy hiện tại đã có qua esptool). Dependencies firmware được pin trong `platformio.ini`.

## Nạp firmware

Firmware mới thay ứng dụng Xiaozhi đang có. Bản sao toàn flash cũ ở `backups/original-flash-16MB.bin`, gồm cấu hình cũ; không chia sẻ công khai. Đã bỏ thư mục backup khỏi Git.

Đưa board vào download mode bằng IO0 + RST như đã thử, rồi:

```powershell
Set-Location 'F:\vnfarm\qrcode'
python -m platformio run -e s3tim -t upload --upload-port COM5
python -m platformio device monitor --port COM5 --baud 115200
```

`board_upload.after_reset = watchdog_reset` đã được cấu hình vì board này có thể vẫn ở download mode khi dùng hard reset mặc định qua USB Serial/JTAG. Nếu cần thoát download mode thủ công bằng công cụ mà không nạp lại, chạy `python -m esptool --chip esp32s3 --port COM5 --before no_reset --after watchdog_reset chip_id`. Không giữ IO0 khi khởi động bình thường.

`dist/firmware.bin` chỉ là app tại offset `0x10000`, không phải file full-flash để ghi từ offset 0. Nên dùng lệnh PlatformIO ở trên để nạp đúng bootloader, partition table, boot_app0 và app. Bản này dùng factory app, không có app slot OTA và không có chức năng cập nhật firmware từ xa. Vùng `otadata` chỉ được dành riêng để tương thích ảnh `boot_app0` mà toolchain Arduino nạp ở `0xe000`, tránh ghi đè vùng NVS chứa khóa.

## Kiểm tra sau nạp

1. Màn hình hiện tên/mật khẩu Wi-Fi và QR trang cấu hình. Điện thoại thấy mạng trong danh sách Wi-Fi; sau khi kết nối, quét QR mở đúng trang thiết lập.
2. Tải được public key; private key không có route xuất.
3. Nhập đúng Wi-Fi, lưu và khởi động lại, thiết bị không mở lại AP setup.
4. Có URL thật và NTP thì QR đổi theo chu kỳ; điện thoại đọc được đủ URL.
5. Gửi QR cho verifier/server: mã đúng hợp lệ; sửa thời gian/chữ ký hoặc dùng mã quá hạn bị từ chối.
6. Rút/cắm điện: cấu hình và public key không đổi; chờ đồng bộ giờ trước khi hiển thị QR.
7. Phần lệnh admin chỉ kiểm tra sau khi bổ sung giao thức thật.

