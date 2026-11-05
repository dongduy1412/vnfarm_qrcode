> LỊCH SỬ: báo cáo này dựa trên commit 3b29dc3. Bước CMS approve đã bị bỏ ở 2f26f9b. Xem SERVER_CURRENT_STATE.md trước khi triển khai.

# Kiểm tra đăng ký thiết bị và WebSocket

Ngày 2026-11-05. Server source HEAD 3b29dc3ae0469f5dd921e565f193b132c58a9e63.
URL staging lấy từ README: https://devices-staging.castorice.tech.
Chưa có bằng chứng URL này là URL hiện lưu trong board; hiện không có cổng USB ESP32.

## Probe trực tiếp

- GET /health: 200, service vnfam-device-api.
- GET /openapi.json: 200, có routes đăng ký/submit/status/claim và connection.
- POST /api/device-registrations với {}: 422 VALIDATION_ERROR đúng kiểm tra đầu vào.
- GET Upgrade WebSocket /api/devices/7ce8b1b17e60/connection không Authorization: 401 UNAUTHORIZED.

Không tạo registration rác, không gửi claimSecret/token/private key, không thử duyệt
CMS hay thực hiện thao tác quản trị. Probe này xác nhận reachable và negative auth,
không chứng minh happy path registration, WSS 101/WELCOME/BOOT hay board online.

## Firmware hiện tại

Không có code gọi registration/submit/status/claim. deviceId được lấy từ MAC.
WebSocketLink::begin(workerOrigin,deviceId,"",...) truyền token rỗng; client chỉ
thêm Authorization khi token không rỗng. Server bắt buộc Bearer token 43 ký tự
và so hash với credential đã cấp cho deviceId. Vì vậy code hiện tại không thể
hoàn tất authentication với server này theo cấu hình đang có.
BOOT_REPORT còn ghi cứng keyId không đúng chuẩn; cần sửa kể cả sau khi có token.
Mặc định build vẫn là Worker cá nhân; runtime có thể đổi URL qua setup nhưng
chưa đọc được NVS/diagnostic trên board để xác nhận URL thực tế.

## Ý lead và contract thực tế

UI mong muốn: Wi-Fi + URL server → OK → tự xác thực/kết nối. Private key giữ
trên board; public key có thể copy. Không nên bắt người vận hành tự điền token,
deviceId hay chuỗi chữ ký.

Repo hiện dùng proof-of-possession và token do server cấp:
1. Giữ cặp P-256 hiện có; keyId=base64url(SHA256(rawPublicKey)).
2. POST registrations {publicKey,hardwareTarget,serial?}; lưu challenge kín.
3. Ký REGISTER_SUBMIT_V1|registrationId|nonce|keyId; POST submit.
4. Chờ CMS approve, poll status >=2 giây/lần.
5. Ký REGISTER_CLAIM_V1|registrationId|nonce|keyId; POST claim.
6. Lưu deviceId,generation,keyId,token; WSS Authorization Bearer token.
7. WELCOME → BOOT_REPORT đúng identity, bootId, build/capabilities thực → EVENT_ACK.
8. Reconnect dùng lại identity/token; không đăng ký lại khi chỉ mất mạng.

Server tạo token ngẫu nhiên; không nhận JWT hoặc token firmware tự ký ở endpoint
connection hiện tại. Chữ ký private key chỉ là proof ở bước submit/claim.
Nếu key đã đăng ký: dùng recovery pairing/CMS theo repo; không tạo key mới hoặc
xóa identity để né xung đột.

## Hai hướng

A — Khuyến nghị nếu giữ repo server: triển khai tự đăng ký/claim trong firmware,
CMS duyệt một lần, các lần sau chỉ tự kết nối. UI sau OK hiển thị kết nối server,
chờ duyệt, đã kết nối hoặc lỗi cụ thể. Cần xác nhận với lead bước duyệt một lần.
B — Nếu lead yêu cầu public key được nhập trước và kết nối ngay bằng token tự ký:
phía backend phải thêm/chốt API provisioning/challenge hoặc signed-token auth;
định nghĩa payload/expiry/nonce/chống replay và cách ánh xạ identity. Không thể
chỉ đổi client gửi token tự ký vào endpoint Bearer hiện tại.

## Tiêu chí nghiệm thu đúng phạm vi hai API

- Private key không xuất; proof đúng được chấp nhận, proof sai bị từ chối.
- Duyệt/claim thành công, identity lưu qua restart; token không log/đưa vào URL.
- WSS 101 + WELCOME + BOOT được ACK, CMS thấy đúng board online.
- Token sai/thu hồi bị từ chối; mất mạng rồi nối lại dùng identity cũ.
- Màn setup chỉ yêu cầu Wi-Fi/URL; không bắt nhập token thủ công.

Chưa triển khai/flash thay đổi trong lần audit này.
