# Firmware 0.4.1 — đăng ký tự động và WebSocket

Worker test riêng: https://vnfarm-enrollment-lab.dong141220047.workers.dev
Source server: worker-test/src, snapshot upstream 2f26f9b, ghi nguồn trong worker-test/UPSTREAM.md.
Account riêng của người dùng; D1 vnfarm-enrollment-lab, DO DeviceDO. Không sửa/deploy staging của lead.
Không provision R2/OTA/CMS credential; phạm vi kiểm tra là enrollment, WSS BOOT và QR verifier.

## Luồng vận hành

Trang setup chỉ nhập Wi-Fi và URL gốc server. Firmware không có URL staging mặc định;
thiết bị chưa có URL hợp lệ sẽ vào setup. Public key vẫn hiển thị/copy được.
Sau lưu: Wi-Fi → NTP callback → POST /api/device-registrations → ký
REGISTER_V1|registrationId|nonce|keyId → POST /complete → lưu identity → WSS
Authorization Bearer token → WELCOME → BOOT_REPORT → EVENT_ACK.
Không cần nhập token hay CMS duyệt. Token do server cấp sau khi verify proof bằng public key.

Private key giữ nguyên trong qr-keys. Server keyId là base64url SHA256 của raw65,
khác fingerprint hex SPKI cũ. Identity {deviceId,generation,keyId,token} lưu NVS
atomic snapshot, gắn với origin và key. URL mới dùng identity riêng; không gửi token
server cũ sang host mới. Mất response complete hoặc mất điện sau khi gửi complete
thì dừng và báo cần recovery; không retry để xin lại token dùng một lần.
Không tự sinh lại key khi gặp KEY_ALREADY_REGISTERED.

Default server trống; không tự chọn Worker của lead. Worker test cũ cá nhân có
chế độ polling tương thích riêng, chỉ khi người vận hành chủ động chọn host cũ.
Trong chế độ dự án không chạy polling/heartbeat cũ song song WSS.

## WebSocket milestone

BOOT dùng keyId/partition size thực, bootId/eventId ổn định trong cùng boot.
BOOT được lưu trước gửi, retry nguyên payload/eventId đến ACK; reconnect cùng boot
không tự nhận là reboot mới. Online chỉ được xác nhận sau ACK đúng eventId.
JSON tối đa 16KiB, ghép các chunk SDK trước parse; network callback không sửa NVS.
Backoff có jitter, giữ token sau disconnect; không công bố capabilities chưa có.
Bản này không triển khai lệnh quản trị qua WSS/OTA/rotation; capabilities rỗng.
Partition hiện factory, không quảng cáo UPGRADE_FIRMWARE.

USB bảo trì: DEVICE_INFO chỉ in trạng thái và origin, không in token;
SERVER_URL <origin> đổi origin và /checkin, giữ Wi-Fi/key (tiện test trên bàn).
Quy trình thông thường của lead dùng trang setup Wi-Fi. SETUP_WIFI mở lại portal.
Mở pyserial với dtr=False,rts=False trước open để tránh thao tác đường reset khi chỉ đọc.

## Kiểm tra

- PlatformIO s3tim build/upload qua COM5: PASS, hash flash verified, NVS không erase.
- Native test actual Enrollment.cpp: PASS hai request, restart/reuse identity,
  origin/key binding, resume challenge, ambiguous complete, power loss và lỗi lưu.
  HTTP/key/hash adapter trong bài native là giả lập, không phải test crypto.
- Upstream check:registrations chạy workerd local: PASS proof/nonce/token,
  tự đăng ký, concurrency, expiry, lockout, quyền CMS và loại bỏ approve.
- tools/test_project_link.ts với project server workerd: PASS P-256 thực, 201 complete,
  401 thiếu token, 101 WSS, WELCOME, BOOT/ACK, reconnect giữ bootId/eventId,
  ping/pong và QR chữ ký thật. Đây là protocol test, không thay test SDK trên board.
- 43 test Python và Playwright setup: PASS.
- Board thật: challenge 201, complete 201, identity persisted, WELCOME,
  WS BOOT acknowledged; D1 confirmedAt đã có, firmwareVersion=0.4.1.
- tools/verify_project_qr.py: PASS QR thật tại Worker riêng và verifier local;
  deviceId=72e8b1a10bc74570be31f5d84afdc003; URL length=208.
  Dài hơn khuyến nghị khoảng 200 của contract vì hostname test + did 32 ký tự.
  Host thật ngắn hơn sẽ giảm độ dài; quét điện thoại thực vẫn cần kiểm tra.

## Test server deploy lại

Trong worker-test: npm ci; npx wrangler deploy. D1 trong wrangler.jsonc đã khởi tạo;
KHÔNG import lại schema.sql vào DB đã có dữ liệu. Config account/resource là lab,
không dùng config staging của upstream. node_modules/.wrangler được gitignore.
Chỉ dùng dữ liệu test. Các thay đổi API từ lead sau commit pin cần đối chiếu lại.

## Restart thật

Đã dùng esptool watchdog reset (không ghi/xóa NVS). Sau reset, DEVICE_INFO báo
identity=1 ws=1, did không đổi 72e8b1a10bc74570be31f5d84afdc003. D1 có bootId
mới d8860089-fd6d-4177-b120-d04382e50800 và chỉ một registration COMPLETED.
QR sau reset vẫn được Worker chấp nhận. Capture serial mở muộn không thấy ACK
đầu boot nên không dùng assertion thứ tự log của capture đó làm bằng chứng;
đối chiếu trạng thái board và bản ghi D1 để xác nhận phục hồi.

URL test được cài qua USB SERVER_URL trên bàn kiểm tra để giữ Wi-Fi đã lưu.
Form setup trên board vẫn là đường nhập URL cho lead; form được kiểm tra bằng
Playwright, chưa coi USB migration là bài test nhập URL bằng điện thoại.
Không có kết nối tới staging của lead trong bài test hoàn tất này.
