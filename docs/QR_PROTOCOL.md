# Giao thức QR bản nền 0.1

Đây là lựa chọn kỹ thuật của bản triển khai để thử nghiệm, không phải chi tiết lead đã chỉ định. Cần phía server thống nhất trước khi dùng thật. Không phải JWT, không phải TOTP.

## Cặp khóa

- Thiết bị tự tạo ECDSA P-256, ký với SHA-256 bằng mbedTLS có sẵn trong Arduino ESP32.
- Private key lưu DER trong NVS namespace `qr-keys`, không đưa vào QR, log hoặc HTTP response.
- Public key xuất PEM SubjectPublicKeyInfo từ trang thiết lập.
- `kid` là SHA-256 của DER SubjectPublicKeyInfo, viết thành 64 ký tự hex thường.
- Device ID là MAC Wi-Fi dạng 12 ký tự hex thường, bỏ dấu `:`.
- Server phải đăng ký public key tin cậy cho đúng device ID. Không nhận key do người quét tự gửi rồi coi là tin cậy.

## Dữ liệu ký

Ghép ASCII/UTF-8 theo đúng thứ tự, ngăn bằng LF, không có newline cuối:

```text
v1
<device-id>
<kid>
<iat>
<ttl>
```

- `iat`: số giây Unix UTC lúc tạo QR.
- `ttl`: chu kỳ cấu hình, số giây dạng thập phân. Bản nền cho nhập 10–60, mặc định thử nghiệm 30.
- Ký toàn bộ các byte trên bằng ECDSA/SHA-256. Chữ ký ASN.1 DER, chuyển sang base64url không có padding `=`.
- ECDSA mỗi lần ký có thể tạo chữ ký khác dù dữ liệu giống nhau; server xác minh bằng public key, không so sánh chuỗi chữ ký với một chữ ký mẫu.

## URL

```text
<checkin-base-url>?v=1&dev=<device-id>&kid=<kid>&iat=<iat>&ttl=<ttl>&sig=<signature>
```

URL nền phải là HTTPS ASCII, tối đa 160 ký tự, không có query sẵn. Bản test LAN có thể cho HTTP tới IP riêng. Từ 0.1.5 hỗ trợ fragment hash-router bắt đầu bằng `/`, ví dụ `https://vnfam.999.vn/#/`; khi đó đặt toàn bộ query phía trước fragment:

```text
https://vnfam.999.vn/?v=1&dev=<device-id>&kid=<kid>&iat=<iat>&ttl=<ttl>&sig=<signature>#/
```

Đây là URL sự kiện người dùng chọn. Fragment `#/` được giữ nguyên để mở đúng route. Phía web đọc query của URL và chuyển dữ liệu cho backend xác minh; firmware không tự thêm API/check-in vào website. Verifier tham khảo kiểm tra cả địa chỉ và fragment phải khớp URL nền đã tin cậy.

## Kiểm tra phía server

1. Chỉ nhận các trường đúng phiên bản, không nhận trường lặp.
2. Tra public key tin cậy của device ID và đối chiếu `kid`.
3. Dựng lại đúng các byte ký, xác minh chữ ký.
4. Kiểm tra `ttl` khớp chính sách server và `0 <= server_now - iat < ttl`. Không tin `ttl` tùy ý từ client. Bản mẫu không cho phép thời gian tương lai.
5. Gắn kết quả check-in vào tài khoản/phiên người chơi theo backend thực tế; bước này không có trong firmware hay tool mẫu.

`tools/verify_qr.py` là verifier offline tham khảo, không phải backend đã triển khai. Chữ ký và thời hạn không ngăn người khác chuyển tiếp mã còn hạn — phù hợp với giới hạn lead đã thảo luận.

Khi đổi private key, public key và `kid` cũng đổi. Server cần cập nhật đăng ký tin cậy trước khi chấp nhận mã mới. Quy trình đổi khóa từ admin chưa triển khai vì chưa có giao thức quản lý.

