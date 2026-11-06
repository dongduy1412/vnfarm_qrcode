# VNFARM — ESP32-S3 QR firmware 0.4.1

## Phạm vi hiện tại

Wi-Fi + URL server → NTP → đăng ký challenge/proof → lưu credential → WebSocket
WELCOME/BOOT_REPORT/EVENT_ACK → QR động. Người vận hành không nhập token.
Firmware hiện **không chạy quản trị HTTP polling**, không hỗ trợ COMMAND/OTA/đổi key
qua WebSocket (capabilities rỗng). Các thư mục `cloudflare/`, `web/static/` và tài
liệu 0.1–0.3 là lab/lịch sử, không phải backend chính cho bài test hiện tại.

## 1. Clone và build firmware

Yêu cầu Python 3 và PlatformIO; lần build đầu cần Internet để tải toolchain.

```powershell
git clone https://github.com/dongduy1412/vnfarm_qrcode.git
cd vnfarm_qrcode
python -m pip install platformio -r requirements.txt
python -m serial.tools.list_ports
python -m platformio run -e s3tim
python -m platformio run -e s3tim -t upload --upload-port COM5
```

Thay COM5 bằng cổng thực của máy lead. Dừng Serial Monitor/watch_device trước upload.
Board: ESP32-S3 XH-S3E-AI, flash 16MB, PSRAM 8MB, TFT ST7789 240×240.
Dây: SCLK42, MOSI41, DC43, RST44, CS GND, VCC/BL 3V3; SPI mode 3.
[Sơ đồ](docs/s3tim154st7789.jpg). Không dùng UART0 vì chân 43/44 nối màn hình.
Upload không xóa NVS/key. Không lấy file `dist` cũ làm bản hiện hành; build bằng PlatformIO.

## 2. Chạy backend repo Hiếu trên máy lead

```powershell
cd F:\
git clone https://github.com/hieudo2808/vnfam-verification-do.git
cd vnfam-verification-do
bun --version
bun install --frozen-lockfile
bun run db:init
npx wrangler dev --local --ip 0.0.0.0 --port 8787
```

Dùng phiên bản Bun theo `packageManager`/`engines` của backend (snapshot từng kiểm
tra yêu cầu >=1.4.4); không dùng Bun cũ tự ghi lại lockfile. `db:init` chỉ cho DB
local mới: không chạy lại schema tùy tiện trên database có dữ liệu.
**Không chạy `bun run dev --ip ...`**: script có sẵn IP có thể gây trùng tham số.
Dùng trực tiếp Wrangler như trên, không deploy staging.
Backend có thể thay API; đã đối chiếu revision `63b3070` (BOOT không có appliedRevision).
Đọc endpoints.md/README của revision bạn clone nếu upstream thay đổi sau này.

Chạy `ipconfig`, tìm IPv4 Wi-Fi thật, không chọn VPN/localhost. Ví dụ IP là
192.168.1.62 thì URL board là `http://192.168.1.62:8787`.
Điện thoại thử mở `<URL>/health` qua cùng mạng trước. HTTP 200 trên chính máy
server chưa chứng minh thiết bị ngoài truy cập được. Nếu cần mở firewall, dùng
PowerShell Administrator và rule hẹp cho mạng test:

```powershell
New-NetFirewallRule -DisplayName "VNFAM LAN test 8787" -Direction Inbound -Protocol TCP -LocalPort 8787 -RemoteAddress LocalSubnet -Profile Private,Public -Action Allow
```

Không tắt toàn bộ firewall. Router guest/AP isolation cũng có thể chặn LAN.

## 3. Setup board

- Bắt `VNFARM-xxxxxx`, password `12345678`; mở `http://192.168.4.1/`.
- Copy/tải public key nếu cần đối chiếu. Private key không hiển thị.
- Nhập origin backend, ví dụ `http://192.168.1.62:8787`; tự ghép `/checkin` và WS path.
- HTTP chỉ cho private IPv4, port 1–65535, xác nhận mạng test tin cậy; HTTPS giữ CA/hostname verification.
- Chọn Wi-Fi 2.4GHz, lưu. Board cần Internet/NTP ngay cả khi backend chạy local.
- Không nhập token: board ký `REGISTER_V1|registrationId|nonce|keyId`, server verify,
  cấp deviceId/token, firmware lưu NVS theo origin rồi kết nối `/api/devices/{did}/connection`.
- Nhấn RST **hoặc bật nguồn** mở lại setup, giữ Wi-Fi/key/server/identity. Sau lưu,
  software restart chạy bình thường. Đây là chính sách người dùng đã chọn, khác
  cơ chế tự chạy sau mất điện trong checklist mentor ban đầu.

## 4. Xác nhận kết nối

```powershell
cd <thu-muc-vnfarm_qrcode>
python tools/watch_device.py --port COM5
```

Cần thấy WELCOME, `WS BOOT acknowledged`, `identity=1 ws=1 status=ONLINE`, sau đó
`WS heartbeat pong received`. `lost=0` là cờ nội bộ, không phải thống kê mất gói.
Server đăng ký mới trả 201 challenge/complete, upgrade WS trả 101.
Quét QR trả `valid:true` chứng minh chữ ký/thời gian hợp lệ, **không phải** đã tạo
check-in tài khoản hay cấp quyền xin tem trên app sự kiện.

## 5. Mang cùng máy server sang Wi-Fi khác

Giữ database local của backend. Trong setup nhập IP mới, chọn **Cùng backend, giữ
đăng ký hiện tại**, nhập URL cũ đã đăng ký thành công, chọn Wi-Fi và lưu.
Identity được copy (nguồn giữ nguyên), không đăng ký lại. Không dùng tùy chọn này
khi chuyển backend khác/staging: có nguy cơ gửi credential nhầm nơi.
Nếu migration 409, không xóa key/DB: kiểm tra URL nguồn và trạng thái identity.
Nếu KEY_ALREADY_REGISTERED, public key đã có trong DB nhưng origin hiện tại chưa
có credential phù hợp. Mất token hoặc DB thay đổi cần recovery, không có tự reset key.

## 6. Kiểm tra không cần board

```powershell
python -m unittest discover -s tests
python -m pip install -r requirements-ui.txt
python -m playwright install chromium
python tools/test_setup_ui.py
```

Test UI dùng API mock, không chứng minh Wi-Fi/flash NVS thực. Test vector có key
fixture công khai dành riêng test, không dùng làm key thiết bị thật.
Không có `npm test` ở root. `tools/test_project_link.ts` hiện phụ thuộc checkout
backend reference ngoài repo; không coi là lệnh chạy ngay từ clone sạch.
`worker-test/` là snapshot lab cũ có config tài khoản cá nhân, **không deploy bằng
config đó trên máy lead**; test bằng backend Hiếu local như mục 2.

## Giới hạn đã biết

- Chưa nghiệm thu staging; từng gặp staging từ chối TLS1.2 trong khi toolchain này
  dùng TLS1.2. Test TLS thành công không đảm bảo toàn bộ API/WS thành công.
- Root CA ngoài GTS/ISRG/GlobalSign cần cập nhật trust store; không setInsecure.
- Chưa có COMMAND/OTA/rotation/recovery tự động đầy đủ. Token NVS không đồng nghĩa
  flash được mã hóa; không công bố chống trích xuất key vật lý.
- Không xuất bản `.runtime`, `.wrangler`, backup flash, Wi-Fi/password/token thật.
