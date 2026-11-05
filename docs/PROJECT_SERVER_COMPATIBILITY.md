> LỊCH SỬ: báo cáo này dựa trên commit 3b29dc3. Bước CMS approve đã bị bỏ ở 2f26f9b. Xem SERVER_CURRENT_STATE.md trước khi triển khai.

# Đối chiếu firmware với server dự án

Nguồn: https://github.com/hieudo2808/vnfam-verification-do
Commit đã đọc: 3b29dc3ae0469f5dd921e565f193b132c58a9e63 (HEAD tại lúc kiểm tra).
Bản tham chiếu: F:/vnfarm/reference-vnfam-verification-do.
Đây là rà soát source, chưa đăng ký board lên staging, chưa chạy E2E hay sửa firmware.

## Kết luận

WebSocket đúng hướng cho repo này; contract polling trước đây không phải giao thức
quản trị của backend này. Giữ P-256 và format QR v1.did.t, tích hợp identity và
WebSocket theo schema hiện hành. Không chỉ đổi URL là ghép được.

## Các chỗ cần khớp

| Hạng mục | Server yêu cầu | Firmware hiện tại / việc cần làm |
|---|---|---|
| Thuật toán | P-256/SHA-256, public raw65, chữ ký raw64, base64url canonical | Phần QR đã phù hợp; tái sử dụng signQr cho proof |
| keyId | Base64URL(SHA256(raw public key)), 43 ký tự | keyId() hiện hex hash SPKI; WS ghi cứng đoạn public key: phải tính ID riêng đúng chuẩn server |
| Đăng ký | POST registrations → submit proof → CMS approve → claim | Chưa có client đăng ký/claim; giữ private key hiện tại, không tự sinh lại |
| Identity | Lưu deviceId/generation/keyId/token do server cấp | did hiện MAC; QR và mọi API phải dùng deviceId được cấp |
| WS auth | Authorization Bearer device-token | begin truyền token rỗng sẽ bị từ chối |
| BOOT | keyId tin cậy, bootId giữ trong cùng boot, cấu hình/build/OTA thực tế | bootId tạo lại mỗi reconnect, keyId/slot ghi cứng: sửa trước khi ghép |
| Lưu sự kiện | Persist event/result tới EVENT_ACK đúng eventId; retry cùng nội dung/ID | Hiện chỉ log ACK; chưa có outbox hoặc phục hồi operation qua mất mạng |
| Lệnh | Journal trước side effect; operationId, deadline, generation/session hợp lệ | Tái sử dụng nguyên tắc journal của WifiCommands cho operation mới; không gọi thẳng reset trong callback |
| REBOOT result | bootId mới khớp BOOT đã nhận | Hiện báo true trước restart, thiếu bootId: không đủ bằng chứng thành công |
| RESET_WIFI result | wifiCleared:true | Hiện chỉ success:true, bỏ qua lỗi storage, thiếu journal: chưa đạt |
| SYNC_TIME result | syncedAtMs | Hiện restart SNTP rồi báo true: cần đợi callback và report thời gian ms |
| Status/config | REQUEST_STATUS; GET desired-state, APPLY_CONFIG với revision | Chưa triển khai; phải áp dụng qrBaseUrl/scopeId/appliedRevision và đối chiếu activeOperation |
| Wi-Fi qua CMS | runtimeConfigVersion=1; GET network-config theo operationId; verify reconnect bằng mạng mới | Chưa có; khác với lưu Wi-Fi local hiện tại |
| SNTP | Bootstrap rồi GET time-settings trước mỗi lần tự sync; report revision/server/time | Hiện hai host cố định, sync giờ một lần: cần nối policy server và giữ epoch UTC |
| Check-in GET | v1.did.t, t giây, N=30; 200 JSON valid, không redirect/session | Encoding QR khớp; dùng ID được cấp, URL từ config; sửa tiêu chí test 302 của Worker cũ |
| LAN | API có thể HTTP/WS local; qrBaseUrl trong schema vẫn HTTPS | Không tự coi URL HTTP setup hiện tại là cấu hình QR hợp lệ trên server dự án |
| Scan/rotation/OTA | Capability theo tính năng thực sự có | Chưa triển khai; không khai capability giả |

## Đăng ký

POST /api/device-registrations body {publicKey,hardwareTarget,serial?}
→ {registrationId,nonce,claimSecret,expiresAt}; lưu kín trước request tiếp.
POST /api/device-registrations/{id}/submit {claimSecret,proof}
proof ký REGISTER_SUBMIT_V1|registrationId|nonce|keyId.
Poll /status bằng claimSecret, không nhanh hơn 2 giây; CMS duyệt.
POST /claim {claimSecret,proof}, ký REGISTER_CLAIM_V1|registrationId|nonce|keyId.
Lưu bền response {deviceId,generation,keyId,token} rồi mở WSS.
Nếu mất response claim: dùng recovery pairing qua admin, không tự đăng ký máy mới.
Token/claimSecret không log, không QR/query URL, không commit.

## WebSocket

wss://<api-host>/api/devices/<deviceId>/connection; token trong Authorization.
WELCOME → BOOT_REPORT hợp lệ → duy trì ping/pong và xử lý operation.
JSON tối đa 16KiB; ghép đầy đủ frame trước parse. bootId một lần/boot;
eventId không thay đổi khi retry cùng event. Hỗ trợ close code/backoff/jitter,
thu hồi credential, kiểm tra generation và deadline. Callback mạng đưa lệnh vào
queue; main task xử lý NVS/QR/restart để tránh cùng sửa SavedNetworks.
GET desired-state khi reconnect, đối chiếu journal; không thực thi lặp UNKNOWN.

## Phạm vi triển khai đề xuất

1. Identity/registration + keyId + token + WSS/BOOT: CMS thấy đúng board online.
2. desired-state/APPLY_CONFIG tối thiểu cho QR; QR thật GET /checkin trả valid:true,
   mã cũ 410, chữ ký sửa 401. Backend tổng vẫn phải nối người chơi/nhận thưởng.
3. Journal/outbox/status và REBOOT/RESET_WIFI/SYNC_TIME với bằng chứng kết quả.
4. Cấu hình giờ/Wi-Fi và SCAN_WIFI theo nhu cầu CMS.
5. Rotation và OTA là giai đoạn riêng: OTA cần partition thực, manifest ký Ed25519,
   app-only update, publisher trust và rollback. Không khai 2MiB OTA khi chỉ có factory.

Nguồn đối chiếu: README.md mục Đội firmware; src/registration-contracts.ts;
src/contracts.ts; src/routes/devices.ts; src/routes/checkin-verification.ts;
src/durable-objects/device.ts; src/operations.ts; src/runtime-contracts.ts.

Các nhận định cũ cần cập nhật: WebSocket là kênh của repo dự án, không phải nhánh
thừa; copy public key thủ công không thay được registration/claim; /checkin 200
valid không đồng nghĩa người chơi đã được ghi check-in/trao thưởng. Polling cũ
chỉ giữ cho chế độ tương thích Worker cá nhân, không bật mặc định cùng WSS dự án.
