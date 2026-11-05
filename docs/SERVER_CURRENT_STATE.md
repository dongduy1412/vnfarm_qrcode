> Báo cáo lịch sử trước firmware 0.4.1. Hiện trạng mới: ENROLLMENT_LAB_041.md và ACCEPTANCE_041.md.

# Hiện trạng server và hướng ghép firmware

Kiểm tra ngày 2026-11-05. Commit server mới nhất đã đọc:
2f26f9b5b5ddb595716bea55d6d8c823ce301696 — Replace device enrollment with two-request flow.
Staging OpenAPI tại devices-staging.castorice.tech đã có /complete, không còn
/submit, /status, /claim hoặc /approve trong nhóm đăng ký.
Không tạo thiết bị staging trong lần kiểm tra; chưa thử happy path với board.

## Thay đổi đã xác nhận

- Bỏ CMS approve/reject. Trạng thái đăng ký: CHALLENGE/COMPLETED/LOCKED/EXPIRED.
- POST /api/device-registrations nhận {publicKey,hardwareTarget,serial?}, trả
  {registrationId,nonce,expiresAt}. Nonce cần giữ kín.
- Ký REGISTER_V1|registrationId|nonce|keyId bằng P-256/SHA-256 raw r||s.
- keyId = base64url(SHA256(rawPublicKey)), không dùng kid hex SPKI cũ.
- POST /api/device-registrations/{registrationId}/complete nhận {nonce,proof}.
  Proof đúng: tạo thiết bị/khóa ACTIVE và trả 201 {deviceId,generation,keyId,token}.
- Token vẫn do server cấp, không phải token tự ký được gửi thẳng vào WebSocket.
- WSS vẫn nhận Authorization Bearer token, chờ WELCOME rồi BOOT_REPORT hợp lệ.
  confirmedAt/online chưa được xác nhận chỉ bởi complete thành công.

## Firmware hiện tại

Code main vẫn lấy did từ MAC, mở WebSocket với token rỗng. Chưa có client đăng ký
hai bước hoặc lưu identity/token do server cấp. BOOT keyId/OTA ghi cứng, bootId
được tạo lại khi reconnect; chưa có outbox EVENT_ACK. Không thể coi đã ghép đúng.
Build mặc định trỏ Worker cá nhân; URL runtime phải xác nhận từ cấu hình thiết bị.

## Quyết định cách làm

Dùng giao thức mới của server, không thêm CMS duyệt và không bắt nhập device token.
Sau setup Wi-Fi/URL: NTP → nếu chưa có identity cho server này, gửi public key,
nhận nonce, ký proof bằng private key đang lưu, complete, lưu identity atomically.
Sau đó WSS bằng token server cấp → WELCOME → BOOT hợp lệ → EVENT_ACK/online.
Có identity thì dùng lại khi reboot/mất mạng, không đăng ký mới.
Không tạo key mới để né KEY_ALREADY_REGISTERED. Nếu complete response mất, cần
recovery theo cơ chế backend, không retry mù để mong được trả lại token.
Identity phải gắn với origin server; không mang token của host cũ gửi tới host mới.

Ưu tiên: registration/identity → WSS/BOOT đúng schema → test happy path, restart,
reconnect, proof sai và token sai. Lệnh điều khiển/OTA mở rộng xử lý sau đúng phạm vi.
Private key không xuất; UI chỉ yêu cầu Wi-Fi/URL và có nút copy public key.

Nguồn: src/registration-contracts.ts, src/routes/registrations.ts,
src/routes/devices.ts, src/platform.ts, src/contracts.ts và README của server.
