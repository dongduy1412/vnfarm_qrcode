# Báo cáo setup firmware — 04/11/2026

## Kết quả

Đã tạo dự án tại `F:\vnfarm\qrcode`, build thành công bản nền `s3tim`. Chưa ghi firmware mới vào flash; chưa kiểm tra ứng dụng mới chạy trên board.

## Đã làm

- Giao tiếp trực tiếp bootloader trên COM5, xác nhận ESP32-S3 revision v0.2, flash 16 MB quad, PSRAM 8 MB, MAC `7c:e8:b1:b1:7e:60`.
- Đọc toàn bộ 16.777.216 byte flash cũ vào `backups/original-flash-16MB.bin`.
- Dùng esptool `verify_flash` đối chiếu backup với thiết bị: `verify OK (digest matched)`. Không dùng `write_flash` hoặc upload firmware mới.
- SHA-256 backup: `2a0efc89f5d2d0e7ca6717f28f31610c4ab548ff0990d9b74213bfa4f5c562e5`.
- Lưu sơ đồ người dùng vào `docs/s3tim154st7789.jpg`; cấu hình màn hình đúng các chân trong sơ đồ, đối chiếu TX/RX với Arduino variant ESP32-S3.
- Cài toolchain ESP32-S3 còn thiếu qua PlatformIO; pin platform và thư viện trong `platformio.ini`.
- Viết phần thiết lập Wi-Fi, lưu cấu hình, tự sinh/lưu cặp khóa, xuất public key, đồng bộ NTP, ký dữ liệu và hiển thị QR đổi theo chu kỳ.
- Giữ log qua native USB CDC; không dùng UART0 trên các chân màn hình.
- Đóng gói bốn ảnh nạp và manifest offset/hash trong `dist`.

## Kiểm chứng

- `python -m platformio run -e s3tim`: SUCCESS.
- Bộ nhớ tĩnh: RAM 46.224 byte / 327.680; app 799.721 byte / 3.145.728. File app đóng gói: 800.080 byte.
- `python -m unittest discover -s tests -v`: 10/10 PASS.
- Tests trên máy tính kiểm tra verifier tham khảo: mã đúng, biên hết hạn, mã tương lai, sửa thời gian, sai khóa/thiết bị, TTL trái chính sách, tham số trùng và chữ ký sai encoding. Chúng chưa xác minh mã thực sự sinh trên MCU hoặc việc quét trên màn hình.
- Đã kiểm tra code thư viện QR, bổ sung chặn quá dung lượng trước khi mã hóa; phân vùng NVS không chồng với ảnh `boot_app0` của Arduino.

## Mức sẵn sàng

**Sẵn sàng để nạp thử bản nền thiết lập/màn hình**, sau đó quan sát thiết bị và kiểm tra checklist trong README. Không coi là bản hoàn thiện yêu cầu lead.

**Chưa sẵn sàng chạy check-in thật** do chưa có URL/backend được xác nhận; để trống URL thì thiết bị không phát QR check-in.

**Chưa triển khai kết nối nhận lệnh admin đổi khóa/xóa Wi-Fi**, vì chưa có endpoint, giao thức và cơ chế xác thực. Không có OTA cập nhật firmware; không tự chọn giao thức quản lý thay phía server.

Hướng khóa tự sinh, ECDSA P-256/SHA-256, định dạng QR, chu kỳ mặc định 30 giây, server NTP và chính sách mất mạng là lựa chọn minh bạch cho bản nền; chi tiết ở README và `docs/QR_PROTOCOL.md`, không gán thành lời lead đã chốt.

## Lệnh nạp khi chuyển sang bước nạp thử

```powershell
Set-Location 'F:\vnfarm\qrcode'
python -m platformio run -e s3tim -t upload --upload-port COM5
```

Lệnh thay ứng dụng Xiaozhi cũ bằng bản nền mới. Bản sao full-flash đã được lưu và đối chiếu trước khi chuẩn bị bước này. Không ghi riêng `dist/firmware.bin` từ offset 0.

## Còn cần

1. Nạp thử, kiểm tra màn hình, Wi-Fi thiết lập, public key và khởi động lại khi mất điện.
2. URL check-in thật và xác nhận hợp đồng chữ ký với backend.
3. Thông tin giao thức admin để bổ sung nhận lệnh và cập nhật public key khi đổi khóa.

## Cập nhật sau khi tạo web test

- Phạm vi người dùng chốt: chỉ thử key và kết nối; đã bỏ giao diện/API gửi ảnh.
- Web tại `web/`, hướng dẫn ở `web/README.md`; máy tính `http://127.0.0.1:8765`, URL QR LAN `http://192.168.1.221:8765/checkin`.
- Đã bổ sung environment firmware `s3tim_local` cho HTTP trên IP LAN riêng. Cả `s3tim` và `s3tim_local` build thành công; không đổi điều kiện HTTPS của bản thường.
- Package mới ở `dist/` và `dist/s3tim_local/`; manifest trong từng thư mục ghi hash/offset mới thay cho thông số file app ở báo cáo ban đầu.
- 21/21 tests verifier + HTTP backend PASS; JavaScript syntax check PASS. Đã thử giao diện xác minh thành công bằng key/QR giả lập và đã xóa đăng ký/phiên giả lập sau thử.
- Chưa nạp firmware mới, chưa thử public key/QR thực sự sinh trên board. Cổng USB được nhận không đồng nghĩa firmware mới đã chạy.

## Cập nhật: đã nạp bản test LAN

- Đã chạy `python -m platformio run -e s3tim_local -t upload --upload-port COM5` thành công ngày 04/11/2026.
- Bản backup flash cũ 16 MB vẫn có và SHA-256 còn khớp với bản đã kiểm tra.
- Esptool ghi bootloader ở `0x0`, partition table ở `0x8000`, boot_app0 ở `0xe000` và ứng dụng 800.496 byte ở `0x10000`. Tất cả đều báo `Hash of data verified`; toàn lệnh upload `SUCCESS`.
- Công cụ đã thực hiện hard reset sau nạp. Board vẫn xuất hiện trên COM5.
- Đọc USB Serial 12 giây sau nạp chưa thu được log khởi động; chưa xác nhận màn hình hoặc chế độ Wi-Fi thiết lập thực sự đang chạy. Không coi thành công upload là đã kiểm thử xong ứng dụng.
- Web test vẫn phản hồi bình thường, chưa có public key thật đăng ký và chưa có QR thật xác minh.
- Bước tiếp theo: kiểm tra màn hình; nếu cần, nhấn thả RST (không giữ IO0) để chạy ứng dụng. Sau đó dùng điện thoại nối Wi-Fi thiết lập, lấy public key, cấu hình Wi-Fi và thử QR với web.

## Cập nhật: màn hình đã hiện chữ/Wi-Fi trên bản 0.1.1

- Thu được log ROM `DOWNLOAD(USB/UART0)` / `waiting for download`, xác định board đang chờ nạp, chưa chạy ứng dụng. Dùng `--after watchdog_reset` đã khởi động được app 0.1.0 và thu log `Setup Wi-Fi: VNFARM-b17e60`, `URL: http://192.168.4.1` (mật khẩu không ghi vào báo cáo).
- Người dùng vẫn thấy màn hình đen dù app chạy. Đổi ST7789 từ SPI mode 0 mặc định sang SPI mode 3, giữ nguyên chân theo sơ đồ và chức năng firmware.
- Đặt `board_upload.after_reset = watchdog_reset` trong PlatformIO, xác nhận uploader 4.11.0 hỗ trợ. Tham khảo cách xử lý download mode qua USB Serial/JTAG trong [Espressif troubleshooting](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/troubleshooting.html).
- Build/nạp 0.1.1 qua COM5 thành công: app 800.640 byte, hash flash verified, log upload xác nhận `Hard resetting with a watchdog` và `SUCCESS`.
- Sau lần nạp này, người dùng trả lời **“Đã hiện chữ/Wi-Fi”**. Đây là xác nhận hiển thị thực tế, không chỉ suy ra từ upload thành công.
- Chưa kiểm tra Wi-Fi người dùng, copy public key, QR thật hoặc khởi động lại qua mất điện. Bước kế tiếp là lấy public key trên trang thiết lập rồi đăng ký vào web test.

## Cập nhật 0.1.2: quét QR để kết nối Wi-Fi thiết lập

- Theo yêu cầu mới của người dùng, thay màn hình nhập mạng/mật khẩu bằng QR Wi-Fi chứa SSID và mật khẩu AP. Dùng định dạng `WIFI:T:WPA;S:...;P:...;;`, escape ký tự theo [ZXing Wi-Fi barcode contents](https://github.com/zxing/zxing/wiki/Barcode-Contents#wi-fi-network-config-android-ios-11).
- QR này chỉ xuất hiện khi chưa thiết lập Wi-Fi, khác với QR có chữ ký dùng check-in sau khi cấu hình xong. Không thay đổi định dạng dữ liệu check-in hoặc tạo lại cặp khóa.
- Thêm DNS wildcard và HTTP redirect để hỗ trợ captive portal mở trang cấu hình. Việc mở tự động do hệ điều hành điện thoại quyết định; vẫn có địa chỉ dự phòng `192.168.4.1` dưới QR.
- Không in mật khẩu AP ra log USB trong bản mới. Mật khẩu vẫn được nhúng trong QR để điện thoại kết nối theo yêu cầu người dùng.
- Build/nạp `s3tim_local` thành công: app 806.496 byte; hash verified; watchdog reset; log `upload-wifi-qr.log`.
- Chưa xác nhận điện thoại người dùng đọc QR, tham gia Wi-Fi hoặc mở captive portal thành công ở thời điểm ghi báo cáo này.

## Cập nhật 0.1.3: QR mở web cấu hình, Wi-Fi chọn thủ công

- Người dùng đính chính flow: mạng thiết bị hiện trong danh sách Wi-Fi để chọn kết nối, QR chỉ mở web lấy public key/chỉnh thiết lập.
- Đã bỏ payload `WIFI:...` của 0.1.2. QR setup chỉ chứa `http://192.168.4.1/`; tên và mật khẩu AP hiện riêng trên màn hình, không in vào log USB.
- Trình tự trên màn hình: (1) kết nối Wi-Fi `VNFARM-…` bằng mật khẩu hiển thị, (2) quét QR để vào web cấu hình. Captive portal vẫn hỗ trợ mở trang, không thay đổi phạm vi quản trị từ xa đang chờ tích hợp.
- Build/nạp bản 0.1.3 `s3tim_local` qua COM5 thành công: app 806.272 byte, hash verified, watchdog reset; log `upload-settings-qr.log`.
- Chưa có xác nhận điện thoại thực tế mở web/lấy key thành công sau lần nạp này.

## Cập nhật 0.1.4: mật khẩu Wi-Fi thiết lập cố định

- Người dùng chọn `12345678` sau khi được giải thích mật khẩu WPA2 cần tối thiểu 8 ký tự.
- Thay mật khẩu AP ngẫu nhiên bằng `SETUP_WIFI_PASSWORD` trong `include/AppConfig.h`; có kiểm tra độ dài khi biên dịch.
- Chỉ thay mật khẩu mạng thiết lập `VNFARM-…`; không thay mật khẩu Wi-Fi đích hoặc tạo lại cặp khóa. QR vẫn chứa URL trang cấu hình.
- Build/nạp `s3tim_local` qua COM5 thành công: app 806.240 byte, hash verified, watchdog reset. Log `upload-fixed-wifi-password.log`.
- Chưa thử kết nối bằng điện thoại với mật khẩu mới. Nếu điện thoại nhớ mật khẩu AP cũ, cần quên mạng rồi nhập mật khẩu mới.

## Cập nhật: đã lấy key thật và xác minh chữ ký từ ảnh thiết bị

- Người dùng xác nhận đã kết nối Wi-Fi, quét QR và cung cấp public key từ trang thiết lập. Ảnh thiết bị đã hiển thị QR check-in dạng đầy màn hình.
- Chuẩn hóa public key PEM người dùng gửi, parse thành công ECDSA P-256 và đăng ký cho MAC `7ce8b1b17e60`, chu kỳ 30 giây trong web test.
- Fingerprint public key: `3b11e24bc51a553f9376dd74ed3e7cc87f99b23a4085ff6074a40717dfd53fdf`.
- Giải mã QR từ ảnh bằng ZXing-C++; QR chứa URL đúng `http://192.168.1.221:8765/checkin`, đúng device ID/fingerprint và chữ ký ECDSA hợp lệ với key người dùng gửi.
- QR ảnh có `iat=1793787960`, `ttl=30`. Kiểm tra offline dùng thời điểm tạo mã để xác minh chữ ký của ảnh lịch sử, không sửa kiểm tra thời hạn trong API thật. Muốn check-in hiện tại phải quét QR mới.
- Khi chẩn đoán trang trắng, server cổng 8765 đã dừng: cả localhost và LAN báo connection refused. Đã khởi động lại bằng Start-Process với cửa sổ ẩn, public key lưu SQLite; localhost/LAN trả HTTP 200 sau khởi động.
- Bổ sung `web/start.ps1` để khởi động server nền theo IPv4 Wi-Fi. Chưa tự đổi firewall; điện thoại vẫn cần cùng mạng Wi-Fi với máy tính để mở web test LAN.
- Chưa có xác nhận điện thoại nhận trang web test sau khi server được khởi động lại.

## Cập nhật 0.1.5: QR trỏ tới web sự kiện

- Người dùng yêu cầu URL `https://vnfam.999.vn/#/`. Đặt làm mặc định của environment `s3tim`, đồng thời chuyển URL LAN test cũ đã lưu trong NVS sang URL mới khi khởi động. Không xóa cấu hình Wi-Fi hoặc khóa.
- Sửa ghép URL để dữ liệu ký nằm trong query trước fragment `#/`; thêm test verifier chấp nhận đúng hash-route và từ chối route khác. 23/23 tests PASS.
- Build/nạp `s3tim` qua COM5 thành công: app 806.528 byte, hash verified, watchdog reset; log `upload-event-url.log`.
- Đọc trực tiếp log sau nạp: `QR refreshed at 1793789700, valid for 30 seconds` và `QR target: https://vnfam.999.vn/#/`. Xác nhận firmware đang dùng URL mới, không chỉ đổi giá trị mặc định trong source.
- Công cụ truy cập web trả 403, chưa xác minh nội dung trang sự kiện. Không có thay đổi nào trên website hoặc API của sự kiện; chưa xác nhận website tự check-in được bằng dữ liệu QR.

