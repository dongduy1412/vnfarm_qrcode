# Bàn giao kết nối Worker dự án

## Test LAN (0.3.3)

Nhập `http://192.168.1.12:8787`: tự ghép `/checkin` và các route poll/ACK,
giữ port. HTTP chỉ được chấp nhận với IP private RFC1918, không dùng hostname,
loopback hay IP public. Trang setup có xác nhận cảnh báo: HTTP không xác thực
server bằng TLS, chỉ dùng mạng LAN test tin cậy. Đây là ngoại lệ test so với
contract yêu cầu HTTPS. Khi chạy thật dùng HTTPS. Port được kiểm tra 1–65535.
Server cần listen `0.0.0.0`, firewall cho phép port, điện thoại/board truy cập
được máy server; API phải có sẵn, không được tạo bởi firmware.
HTTPS có thể kèm port và vẫn xác minh CA/hostname, không redirect.

## Setup không cần flash lại (0.3.2)

Trang setup có **Link Worker dự án** (HTTPS origin, không path). Khi nhập link,
URL QR tự đổi thành `<origin>/checkin`, heartbeat extension mặc định tắt.
Mục nâng cao cho URL check-in riêng và bật lại ping/pong nếu backend hỗ trợ.
Cả form mạng mới và mạng đã lưu đều gửi cấu hình server. Cấu hình được lưu NVS
và không bị mặc định build ghi đè sau reboot/upload app. Key không thay đổi.
Đổi host chỉ trong setup qua AP; normal mode không mở portal. Mở lại bằng USB
`SETUP_WIFI`. Không tự fallback sang Worker cá nhân nếu host mới sai.

TLS tin tập root công khai GTS/ISRG/GlobalSign, kiểm tra hostname và không redirect.
Không tự tải CA từ host lạ và không cho upload private key. Host dùng CA khác
hoặc thay đổi CA ngoài tập tin cậy vẫn cần firmware cập nhật root. Chỉ nhập URL
của đơn vị quản trị tin cậy: host đó có thể phát lệnh Wi-Fi cho board.
Setup kiểm tra định dạng và kết nối Wi-Fi, **chưa kiểm tra API/TLS Worker trước
khi lưu**. Nếu sai endpoint, mở lại setup để sửa. Phải đăng ký public key trên
Worker mới và test poll/ACK/check-in sau chuyển.

## Mặc định build

Cấu hình tập trung ở `include/ServerConfig.h`:

| Macro build | Chức năng | Mặc định hiện tại |
|---|---|---|
| `QR_DEFAULT_CHECKIN_URL` | Điện thoại mở sau quét QR | Worker cá nhân + `/checkin` |
| `QR_CLOUD_BASE_URL` | Board gọi server quản trị | Origin HTTPS Worker cá nhân, không slash cuối |
| `QR_CLOUD_HEARTBEAT_ENABLED` | Bật extension ping/pong | `1` cho bản test cá nhân |
| `QR_DEVICE_POLL_INTERVAL_MS` | Chu kỳ hỏi lệnh | `60000` ms |

Các macro có thể override bằng build_flags PlatformIO; không đưa private key/admin token vào build_flags. Khi tích hợp server chỉ có API theo contract, đặt `QR_CLOUD_HEARTBEAT_ENABLED=0`: poll và ACK vẫn hoạt động, không gọi API ping riêng. Mặc định chưa thay đổi để không gián đoạn demo hiện tại.

Origin mặc định được kiểm tra lúc compile; origin nhập setup được kiểm tra runtime qua `ServerOrigin.h`: HTTPS hoặc HTTP private IPv4 cho test LAN, port tùy chọn 1–65535, không path/query/fragment/credentials. Đây là kiểm tra cấu hình chứ không thay thế kiểm tra TLS. Firmware vẫn kiểm tra root CA + hostname và không theo redirect. Khi đổi host, kiểm tra chuỗi chứng chỉ rồi cập nhật `include/WorkerRootCAs.h` nếu cần; không dùng setInsecure.

## Giao thức đã có / cần phía dự án xác nhận

- Theo contract mục 6: HTTPS polling `POST /api/device/poll`, body `{v,did,t,sig}`, ký `v1.poll.<did>.<t>`.
- `POST /api/device/ack`, body `{v,did,cmdId,t,sig}`, thành công 204.
- Chuỗi ACK `v1.ack.<did>.<cmdId>.<t>` là bổ sung hiện tại vì contract chưa chốt chuỗi này.
- Implementation chọn P-256/SHA-256, public key raw uncompressed 65 byte, chữ ký raw r||s 64 byte, base64url không padding. Phải được phía server dự án chấp nhận.
- Implementation chọn tin server qua HTTPS đúng hostname/CA, không xác minh chữ ký server riêng; lựa chọn này còn cần phía dự án xác nhận theo mục [CẦN QUYẾT].
- `wipe_wifi` là lệnh trong contract. `reconnect_wifi`, `forget_current_wifi` và heartbeat ping-v1 là extension.
- Người dùng chọn cả ba lệnh Wi-Fi giữ key. Không gọi là factory reset; chưa đáp ứng nguyên văn checklist xóa Wi-Fi + key nếu mentor chưa chấp thuận khác biệt này.
- Admin đăng ký did + raw public key qua CMS/API `PUT /api/admin/checkin/devices/{did}`. Không đưa private key lên server.

## Cần lead cung cấp / xác nhận

1. Origin Worker quản trị và URL check-in thực tế (có thể khác tên miền).
2. API poll/ACK đã được triển khai chưa; repo/đầu mối backend để ghép và test.
3. Chấp nhận encoding P-256, chuỗi ACK, HTTPS trust và các extension nào.
4. CMS/đầu mối nhận did + public key; route xin tem và CHECKIN_VALID_SECONDS do server chốt.

Không cần hỏi lại polling hay WebSocket: tài liệu hiện mô tả polling. Không có yêu cầu URL trực tiếp của DO hay cách chia DO trong bốn tài liệu.

## Trạng thái

Chưa đổi sang Worker dự án, chưa deploy server hay thay key. Build s3tim, 43 test Python và Playwright setup (cả form mạng đã lưu/mới, đổi Worker, chặn URL không hợp lệ) đã đạt. Chưa xác nhận lưu server tùy chọn trên board thật trong lần triển khai này. NTP, các test phần cứng còn lại xem `NTP_031_VERIFICATION.md`.
