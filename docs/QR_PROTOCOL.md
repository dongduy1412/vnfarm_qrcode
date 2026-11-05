# QR contract v1 — firmware 0.2.0

Theo [contract mentor](mentor/contract.md). Thuật toán trong bản nháp cho phép
P-256 hoặc Ed25519; implementation này giữ **ECDSA P-256/SHA-256**, bổ sung
encoding cụ thể để ESP, Python và WebCrypto cùng đọc được.

| Trường | Giá trị |
|---|---|
| Device ID | `did`, regex `[a-z0-9-]{1,32}`; board hiện dùng MAC 12 hex |
| Chu kỳ firmware | N=30 giây mặc định; setup cho phép 10–60 |
| Thời gian | `t=floor(now/N)*N`, Unix UTC giây, thập phân canonical |
| Message | ASCII `v1.<did>.<t>`, không newline cuối |
| Private key | P-256 trong NVS `qr-keys/private-der`, không xuất |
| Public key | SEC1/X9.62 uncompressed: `04 || X[32] || Y[32]`, 65 byte |
| Chữ ký | IEEE P1363: `r[32] || s[32]`, big-endian, 64 byte |
| Encoding | Base64url canonical, không padding `=`; key 87 ký tự, sig 86 |

```text
https://<host>/checkin?v=1&did=<did>&t=<unix>&sig=<base64url>
```

Chỉ nhận đúng 4 tham số; từ chối trùng, thiếu, dư, fragment/hash-router và
encoding không canonical. Không có `dev`, `kid`, `iat`, `ttl` trong QR mới.
Host/route cố định trong build; server không tin host do người quét cung cấp.
P-256 signer bên ESP vẫn tạo và tự verify DER trong nội bộ, sau đó đổi DER
thành raw cho QR. Heartbeat DO vẫn giữ DER và payload `ping-v1` riêng.

Server tra key theo `did`, verify message rồi kiểm tra thời gian bằng giờ
server. TTL mặc định 60 giây, SKEW=5; TTL chọn 10/20/30/60 theo server policy:

- Thiếu/sai tham số hoặc `age < -SKEW`: 400 `bad_request`.
- Device ID chưa đăng ký: 404 `unknown_device`.
- Chữ ký sai: 401 `invalid_signature`.
- `age > TTL`: 410 `expired`; `age == TTL` vẫn chấp nhận theo contract.
- Quá tần suất: 429 `rate_limited`.
- Đã có phiên đăng nhập: hợp lệ → ghi `(user,did,t)` idempotent, 302 `/stamp`.
- Chưa đăng nhập: chuyển đăng nhập rồi quay lại URL check-in.

N và TTL độc lập. Khuyến nghị TTL >= N+10; không gọi chu kỳ đổi mã là TTL.
Public key mới được đổi biểu diễn từ key cũ; không cần xoay private key.
Public key PEM/SPKI của bản 0.1.x khác raw; không chỉ bỏ dòng BEGIN/END.

## Trạng thái server

Worker hiện triển khai trên host thử nghiệm, với raw public key board được
đăng ký trong `CHECKIN_PUBLIC_KEY`. Phiên người dùng là **phiên test riêng**,
không đại diện đăng nhập website sự kiện. `CHECKIN_VALID_SECONDS=1800` chỉ là
policy test; thời hạn thật, route xin tem, CMS admin và middleware bảo vệ ảnh
phải ghép vào app thật khi có mã nguồn. Không tự coi các mục đó đã hoàn thành.

`GET /api/checkin/status` trong Worker trả `checkedIn`, `expiresAt`, `deviceId`.
DO lưu kết quả bền vững theo user và t. Public key trong config là dữ liệu
công khai; Cloudflare OAuth token và private key thiết bị không nằm trong repo.

## Test vector

[contract-vectors.json](contract-vectors.json) chứa key fixture **TEST ONLY**,
public raw, private PKCS8, DER/raw signature, message, URL và 7 trường hợp
trong contract. Không nạp fixture lên board thật. Python, native firmware
helpers và Worker WebCrypto cùng kiểm tra file này. HTTP/login/replay được
thử với Worker+SQLite DO trong `tools/test_do.py`.
