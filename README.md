# VNFARM · ESP32-S3 QR check-in

Firmware **0.3.0**, ESP32-S3 mạch tím XH-S3E-AI + TFT ST7789 240×240.
Giao thức QR theo [contract mentor](docs/mentor/contract.md), với lựa chọn
ECDSA P-256/SHA-256, public key raw uncompressed và chữ ký raw `r||s`, base64url.

## URL và luồng

```text
t = floor(now / N) * N
message = v1.<did>.<t>
https://vnfarm-qrcode-device-link.dong141220047.workers.dev/checkin?v=1&did=<did>&t=<t>&sig=<sig>
```

- N mặc định 30 giây trên firmware. Server giữ TTL=60 giây, SKEW=5 giây;
  không lấy TTL từ QR.
- Host/route cố định theo build, không nhập URL tùy ý qua trang setup.
- Board giữ nguyên private key NVS từ phiên bản trước. Trang setup xuất
  public key **raw base64url**, không phải PEM. Không xuất private key.
- Bản Worker riêng này đang ở **TEST_MODE**: mở `/login` và bấm bắt đầu phiên
  test trước khi quét. Đây là phiên khách thử nghiệm, không phải tài khoản
  sự kiện. `/stamp` chỉ hiển thị kết quả thử, không cấp tem/thưởng.
- `GET /checkin` xác minh chữ ký/thời gian, ghi idempotent trong DO và trả
  302 `/stamp`; `GET /api/checkin/status` trả trạng thái. Đã thử mã thật từ
  board qua cả Worker đã deploy lẫn web Python.
- Phần đăng nhập thật, CMS và API xin tem/ảnh cần tích hợp vào `apps/worker`
  của sự kiện. Repo này chưa có mã nguồn app đó. Không coi test thành công
  ở đây là check-in đã ghi vào website `vnfam.999.vn`.

Chi tiết: [giao thức](docs/QR_PROTOCOL.md), [báo cáo chuyển đổi](docs/CONTRACT_MIGRATION.md).

## Wi-Fi và màn hình

- Chưa setup: bắt `VNFARM-xxxxxx`, mật khẩu `12345678`, mở
  `http://192.168.4.1/`, nhập Wi-Fi 2.4 GHz và copy public key.
- Trang setup chia hai bước: nút **Sao chép public key**/tải file, rồi chọn
  mạng đã lưu hoặc thêm Wi-Fi. Có ẩn/hiện mật khẩu và trạng thái kết nối/lưu.
  Giao diện tải trực tiếp từ board, không cần Internet; xem [setup UI](docs/SETUP_UI.md).
- Nhớ tối đa 5 mạng, ưu tiên mạng lưu gần nhất; thử mạng tiếp theo sau 15 giây
  nếu chưa nối được. NVS giữ Wi-Fi và key qua restart.
- Gửi `SETUP_WIFI` kèm newline qua USB để mở lại setup, giữ nguyên danh sách
  mạng và key. Đây không phải factory reset.
- AP/trang setup tắt trong chế độ chạy bình thường; có Wi-Fi và giờ hợp lệ
  thì vẽ QR động. QR màu đỏ, logo giữa, sửa lỗi H, giữ quiet zone.
- Có hai NTP server; framework poll định kỳ. Chưa nghiệm thu 5 lần mất điện,
  24 giờ và chuyển đổi giữa hai AP qua sóng thật.

Đấu dây theo [sơ đồ](docs/s3tim154st7789.jpg): SCLK=42, MOSI=41, DC=43,
RST=44, CS=GND, VCC/BL=3V3. Dùng SPI mode 3; không dùng UART0 trên 43/44.
Log qua USB CDC, board hiện là COM5, flash 16 MB, PSRAM 8 MB.

## Build và nạp

```powershell
python -m pip install platformio -r requirements.txt
python -m platformio run -e s3tim
python -m platformio run -e s3tim -t upload --upload-port COM5
```

Upload dùng watchdog reset. Nạp app không xóa NVS. `dist/firmware.bin` là app
offset `0x10000`, không phải full flash. Dùng PlatformIO để nạp đúng offsets.
Không có OTA. Backup flash và dữ liệu chạy thử được bỏ khỏi Git.

## Kiểm tra

```powershell
python -m unittest discover -s tests
cd cloudflare
npm ci
npm run types
npm run check
cd ..
python tools/test_do.py
node --experimental-transform-types tools/test_contract_worker.mjs
python tools/verify_live_contract.py --port COM5
```

[Test vectors chung](docs/contract-vectors.json) có **private key fixture công
khai dành riêng cho test**, hoàn toàn khác key board thật. Không dùng fixture
để cấu hình thiết bị thật. Hướng dẫn kiểm tra native/ảnh QR trong tài liệu
chuyển đổi; cần `requirements-preview.txt`.

## Web/Cloudflare

- [Worker và DO](cloudflare/README.md).
- [Web test trên máy tính](web/README.md): `./web/start.ps1`, mở
  `http://127.0.0.1:8765/`. Dán URL QR của Worker để thử; địa chỉ trang lab và
  địa chỉ tin cậy trong QR là hai giá trị riêng.
- [Trạng thái heartbeat board](https://vnfarm-qrcode-device-link.dong141220047.workers.dev/api/devices/7ce8b1b17e60/status).
  Kênh heartbeat vẫn dùng giao thức riêng `ping-v1`; nó không thay thế check-in.

Các báo cáo 0.1.x trong repo là lịch sử. Bản 0.2.0 sửa tương thích QR; còn
factory reset, cấp key chung nhiều cửa và nghiệm thu toàn bộ checklist cần
thực hiện riêng theo các mục đang mở của tài liệu mentor.

## Điều khiển Wi-Fi từ server

Web test trên máy tính có ba lệnh: kết nối lại (giữ mạng), quên mạng hiện tại, xóa toàn bộ Wi-Fi. Cả ba giữ key; board xác nhận với Worker trước khi restart khi kết nối còn hoạt động. Xem [giao thức, vận hành và kiểm tra](docs/REMOTE_WIFI_COMMANDS.md).
