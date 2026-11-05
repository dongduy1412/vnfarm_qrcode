# Chuyển QR sang contract v1 — firmware 0.2.0

Ngày 05/11/2026. Tiêu chuẩn gốc được lưu nguyên văn ở `docs/mentor/`.
Chỉ thay giao thức QR/check-in thử; không xóa hoặc xoay private key và không
xóa các mạng Wi-Fi đã lưu. Báo cáo review `REVIEW_AGAINST_MENTOR_DOCS_2026-11-05.md`
đánh giá baseline 0.1.10 trước thay đổi này, không phải kết luận về 0.2.0.

## Thay đổi

- Message ASCII `v1.<did>.<t>`; `t=floor(now/N)*N`; N mặc định 30 giây.
- URL `/checkin?v=1&did=...&t=...&sig=...`, chỉ 4 tham số; host/route cố định
  theo build. Bỏ các trường `dev`, `kid`, `iat`, `ttl` và hash-router cũ.
- Chọn P-256/SHA-256 trong hai lựa chọn contract cho phép. Giữ thuật toán và
  private key cũ. Chốt encoding implementation: raw public key uncompressed
  65 byte và chữ ký IEEE P1363 `r||s` 64 byte, base64url không padding.
- Trang setup/download public key xuất raw. Local SQLite tự chuyển PEM/SPKI
  cũ sang đúng raw của cùng key, TTL mặc định 60; có test migration.
- Server giữ TTL=60, SKEW=5. Chấp nhận `age == TTL` và `age == -SKEW`; từ chối
  `age > TTL` và `age < -SKEW`. Chu kỳ N không còn bị gọi là TTL.
- Worker xác minh bằng WebCrypto, lưu check-in idempotent `(user,did,t)`
  trong SQLite DO, trả 302 `/stamp`; có `/api/checkin/status`, lỗi JSON và
  trang lỗi với 400/401/404/410, giới hạn tần suất 429.
- Kênh DO heartbeat có domain `ping-v1` giữ nguyên, không chuyển signature
  format của nó. Public key/nonce/URL diagnostics chỉ qua lệnh USB `QR_INFO`;
  routine log không in toàn bộ `sig`.

## Phạm vi app còn chờ

Worker hiện tại là **server thử riêng**, `TEST_MODE=true`. `/login` tạo phiên
khách test bằng token ngẫu nhiên; chưa phải đăng nhập app sự kiện. `/stamp`
chỉ xác nhận thử, không cấp tem/thưởng. `CHECKIN_VALID_SECONDS=1800` giữ policy
test 30 phút trước đây; tài liệu mentor chưa chốt thời hạn thật.

Để ghép sản phẩm thật cần nguồn `apps/worker`: thay resolver phiên test bằng
user đăng nhập có sẵn, route xin tem thật, CMS đăng ký raw public key/config,
middleware bảo vệ API nhận dữ kiện/ảnh. Không tự tạo API giả hay coi các mục
này đã chạy trên `vnfam.999.vn`. Reset toàn bộ, cấp key chung nhiều cửa,
thử mất điện 5 lần/24 giờ/đặt tại cửa là công việc riêng, chưa hoàn tất ở đây.

## Kiểm chứng

- 37 unit/integration tests Python pass; TypeScript check pass.

- Cùng `contract-vectors.json` được Python verifier, Worker WebCrypto và
  native helper firmware đọc đúng. Fixture có private key công khai dành
  riêng cho test, không dùng trên board/Worker tin cậy thật.
- Test HTTP local chạy trên Workers runtime + SQLite DO: 302/400/401/404/410/
  429, login return URL, TTL/SKEW, check-in status và idempotence.
- 108 lượt decode ảnh QR xuất từ bộ vẽ firmware với payload contract mới,
  gồm ảnh xoay/mờ nhẹ, đều đọc đúng URL.
- Firmware 0.2.0 build và upload thành công qua COM5, hash verified.
- QR thực đang hiển thị trên board, lấy qua USB `QR_INFO`, đã được **Worker
  deploy thật, Python verifier và web test local** chấp nhận. Worker trả 302
  `/stamp`; gửi lại cùng QR giữ nguyên kết quả. URL thực dài **192 ký tự**.
- Raw public key của board trùng chính xác key được chuyển từ SPKI trước
  đây. Wi-Fi vẫn kết nối, heartbeat DO vẫn nhận pong; private key không xuất.

Việc kiểm tra USB chứng minh dữ liệu/signature của QR đang hiển thị, không
thay thế phép thử camera, khoảng cách quét tại cửa hay nghiệm thu 24 giờ.

## Giờ Windows khi test local

Phát hiện Windows chậm khoảng 52 giây, dịch vụ W32Time chưa chạy và phiên làm
việc không có quyền khởi động dịch vụ. Không nới SKEW hay sửa giờ hệ thống.
`web/start.ps1` dùng nguồn Date của Worker qua HTTPS đã xác minh, neo monotonic
clock và đồng bộ lại định kỳ cho **lab local**. Worker luôn dùng `Date.now()`,
ESP dùng NTP như contract. Có test nguồn giờ local và fail-closed khi stale.

## Chạy lại kiểm tra

```powershell
python -m unittest discover -s tests
cd cloudflare
npm ci
npm run types
npm run check
npx wrangler deploy --dry-run
cd ..
python tools/test_do.py
node --experimental-transform-types tools/test_contract_worker.mjs
python -m ziglang c++ -shared -O2 -Iinclude tools/contract_native.cpp -o .pio/contract-native.dll
python tools/test_contract_native.py
python tools/test_qr_render.py
.\web\start.ps1
python tools/verify_live_contract.py --port COM5
```

Native tests và QR rendering cần `requirements-preview.txt` và DLL renderer
được build theo `docs/BRANDED_QR.md`. Live test cần board đã setup Wi-Fi/NTP.
Nạp: `python -m platformio run -e s3tim -t upload --upload-port COM5`.

Code 0.1.x và verifier cũ không tương thích QR 0.2.0; cập nhật hai phía cùng
nhau. `s3tim_local` dành riêng cho build lab HTTP LAN; nếu dùng nó, đổi endpoint
theo IP thật và chạy lab với `--checkin-url` tương ứng. Không dùng HTTP LAN
cho đường dẫn check-in triển khai thật.
