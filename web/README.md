# Web test key và kết nối

Phạm vi hiện tại theo người dùng: public key, chữ ký QR, hạn mã và kết nối. Không có giao diện hay API gửi ảnh vật phẩm.

## Chạy

Để server chạy nền và không phụ thuộc phiên terminal, mở PowerShell rồi chạy:

```powershell
Set-Location 'F:\vnfarm\qrcode'
.\web\start.ps1
```

Script dùng IPv4 adapter Wi-Fi hiện tại, giữ đăng ký public key trong SQLite và ghi log vào `web/.runtime/`. Không tự tạo autorun khi bật Windows và không tự đổi Windows Firewall.

Hoặc chạy trực tiếp trong terminal (server chỉ chạy khi tiến trình này còn hoạt động):

```powershell
Set-Location 'F:\vnfarm\qrcode'
python web/server.py --lan-ip 192.168.1.221 --port 8765
```

- Máy tính đăng ký public key: `http://127.0.0.1:8765/`.
- Điện thoại cùng Wi-Fi: `http://192.168.1.221:8765/`.
- URL firmware đưa vào QR: `http://192.168.1.221:8765/checkin`.

IP `192.168.1.221` được đọc từ adapter Wi-Fi máy này lúc setup. Nếu IP đổi, sửa cả `--lan-ip` và URL trong cấu hình firmware/PlatformIO. Server và board phải cùng mạng có truy cập server này; Wi-Fi board cũng cần truy cập NTP để lấy giờ.

## Bản firmware để test cục bộ

Đã có environment `s3tim_local` trong `platformio.ini`. Environment này cho phép HTTP tới IP LAN riêng và có URL mặc định trỏ tới server ở trên. Environment `s3tim` thông thường vẫn yêu cầu HTTPS.

```powershell
python -m platformio run -e s3tim_local
```

Lệnh nạp thử (bản 0.1.1 đã được nạp, người dùng đã xác nhận hiện chữ/Wi-Fi):

```powershell
python -m platformio run -e s3tim_local -t upload --upload-port COM5
```

Đã dùng `board_upload.after_reset = watchdog_reset` để ứng dụng chạy sau nạp, và SPI mode 3 để màn hình này hiển thị được. Không cần nạp lại chỉ để nhập Wi-Fi hoặc lấy public key.

## Cách thử với board thật sau nạp

1. Với bản 0.1.4, chọn mạng `VNFARM-…` trong cài đặt Wi-Fi điện thoại, nhập mật khẩu cố định `12345678`. Sau khi kết nối, quét QR trên màn hình để mở trang cấu hình `http://192.168.4.1/`.
2. Copy/tải public key trước khi lưu Wi-Fi. Không nhập hoặc gửi private key lên web test.
3. Trên máy tính, quay về mạng Wi-Fi chung, mở `http://127.0.0.1:8765`, đăng ký public key với MAC board và chu kỳ khớp firmware (mặc định 30 giây).
4. Ở trang thiết lập board, nhập Wi-Fi. Điền URL `http://192.168.1.221:8765/checkin` hoặc để trống để dùng mặc định của bản `s3tim_local`. Lưu và chờ board lấy giờ, hiện QR.
5. Điện thoại quay về Wi-Fi chung, dùng camera điện thoại quét QR trên màn hình. Trang mở ra sẽ tự xác minh.
6. Hoặc dán nguyên URL đọc từ QR vào web test. Chữ ký/key đúng và mã còn hạn thì báo thành công; sửa dữ liệu hoặc mã quá hạn thì báo lỗi.
7. Sau khi restart/rút cắm điện, public key phải giữ nguyên. QR mới vẫn xác minh được với key đã đăng ký.

## Ý nghĩa trạng thái

- Trình duyệt → server: một request HTTP thật tới `/api/status`.
- Máy tính → USB: liệt kê cổng `303A:1001` bằng pyserial, không mở cổng, không reset board, không chiếm COM5 khi nạp.
- QR thiết bị → server: kết quả xác minh bằng public key, không phải phép đo Wi-Fi/heartbeat trực tiếp. Dấu thời gian hiển thị là lần xác minh thành công gần nhất, không phải thông báo board đang online liên tục.
- Kết quả thành công nhớ 30 phút trong trình duyệt qua phiên server; từng QR mới luôn kiểm tra thời hạn riêng. Đây là chính sách của web test, không phải thời hạn phiên chơi đã được lead chốt.

Public key lưu SQLite trong `web/.runtime/` (đã gitignore). Khởi động lại server giữ đăng ký key, xóa các phiên test. Đăng ký/xóa key chỉ cho phép truy cập từ máy tính qua loopback. Điện thoại dùng trang LAN để xác minh.

Nếu điện thoại không mở được URL LAN: kiểm tra cùng Wi-Fi, IP hiện tại, cách ly client của mạng và Windows Firewall cho Python/cổng 8765. Chưa tự thay đổi firewall trên máy.

## Kiểm tra tự động

```powershell
python -m unittest discover -s tests -v
node --check web/static/app.js
```

Python đang dùng có `cryptography==50.0.0`, `pyserial==3.5`. Nếu chuyển máy khác, cần cài các package tương ứng. Không cần npm, CDN hoặc framework frontend để chạy trang.

