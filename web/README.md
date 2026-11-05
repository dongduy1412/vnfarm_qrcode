# Local firmware lab — contract v1

```powershell
python -m pip install -r requirements.txt
.\web\start.ps1
```

Trang máy tính: `http://127.0.0.1:8765/`. Điện thoại dùng IP Wi-Fi máy tính
được script in ra. Chỉ loopback được đăng ký/gỡ public key; trang lab dùng
raw P-256 base64url lấy từ setup firmware 0.2.0. Đăng ký cũ dạng PEM trong
SQLite tự chuyển sang cùng key raw và TTL mặc định 60; không xoay key.

URL trang lab và URL QR tin cậy là hai địa chỉ khác nhau. Mặc định lab xác
minh QR của Worker:
`https://vnfarm-qrcode-device-link.dong141220047.workers.dev/checkin`.
Dán URL quét được vào lab để kiểm tra; quét QR trực tiếp sẽ mở Worker.

```powershell
python web/server.py --lan-ip 192.168.1.3 --port 8765 --checkin-url https://vnfarm-qrcode-device-link.dong141220047.workers.dev/checkin
```

Signature raw `r||s`, message `v1.did.t`, TTL độc lập N; TTL=60, SKEW=5 mặc
định. Có trạng thái `/api/checkin/status` và phiên test 30 phút, không có
tài khoản sự kiện hoặc gửi ảnh vật phẩm. API `/api/checkin` giữ POST để hỗ
trợ dán URL vào lab; HTTP `GET /checkin` đúng contract nằm ở Worker.

## Đồng hồ local

Windows hiện bị chậm khoảng 52 giây và W32Time chưa chạy; không đủ quyền mở
dịch vụ đồng bộ. Vì thế `start.ps1` bật tùy chọn `--clock-url` trỏ tới `/health`
của Worker qua HTTPS đã xác minh chứng chỉ. Lab lấy HTTP Date, chạy tiếp bằng
monotonic clock và thử đồng bộ lại mỗi 5 phút. Nguồn giờ quá cũ hơn 10 phút
thì từ chối kiểm tra với 503. Không nới SKEW/TTL hay thay giờ Windows.

Worker thật luôn dùng `Date.now()` của Cloudflare như contract; ESP dùng NTP.
Không bật `--clock-url` thì lab dùng giờ hệ thống bình thường, cần đồng bộ
Windows đúng giờ. Địa chỉ nguồn giờ là cấu hình tin cậy của người vận hành,
không lấy từ QR hoặc request của người chơi.

Server chạy nền, log/SQLite/PID trong `web/.runtime/`, được gitignore. Không
tự đổi Windows Firewall hoặc cài autorun. Dừng đúng PID/command của server
trước khi khởi động lại sau cập nhật code; không dừng ứng dụng khác ở cổng này.
