# Kiểm tra NTP / QR_INFO — 0.3.1

## Trạng thái lần kiểm tra hiện tại

- Build `python -m platformio run -e s3tim`: PASS.
- Python `python -m unittest discover -s tests`: 43/43 PASS.
- Ban đầu không có COM5; sau đó board xuất hiện lại. Đã nạp các thay đổi chẩn đoán bằng PlatformIO qua COM5, hash ghi flash được xác minh; không erase NVS.
- 10 yêu cầu QR_INFO trong khoảng 20 giây đều nhận public key/URL. Log đã lược bỏ dữ liệu QR: `web/.runtime/qr-info-diagnostic.log`.
- `python tools/verify_live_contract.py --port COM5`: PASS, QR board được Python, Worker đã deploy và web cục bộ chấp nhận; check-in lặp không đổi kết quả.
- Capture sau upload không có banner boot nên công cụ báo NOT VERIFIED đúng dự kiến; chưa dùng capture đó làm bằng chứng NTP trước FIRST QR. Cần capture trước khi nhấn RST.
- Chưa thử chặn/mở NTP trên router và chưa quan sát đủ một giờ sync định kỳ trong lần này.
- Chưa kết luận nguyên nhân thực tế của QR_INFO chập chờn. Không đổi điều kiện để bỏ qua NTP hoặc tự retry che lỗi.
- Code xử lý USB ở đầu loop, trước cập nhật trạng thái Wi-Fi/NTP và dựng QR. Vì thế yêu cầu có thể đến trước lần cập nhật đầu tiên; đây chỉ là khả năng từ code, chưa phải nguyên nhân đã xác nhận trên board.
- Bổ sung `reason`, uptime, reset reason và trạng thái vào phản hồi chưa sẵn sàng để phân biệt Wi-Fi, NTP, setup, render và loop chưa cập nhật.
- Xóa URL chẩn đoán đã lưu khi ngừng QR do mất Wi-Fi; phản hồi QR_INFO còn yêu cầu NTP hiện hợp lệ và không có remote restart.
- Log sync in thêm interval thực tế của SNTP. Chưa quan sát sync định kỳ thật trong lần này.

## Quyết định và phạm vi do người dùng xác nhận

- Giữ key trong cả ba lệnh quản trị Wi-Fi; không thực hiện factory reset xóa key. Khác biệt với checklist mentor phải được mentor chấp nhận nếu dùng để nghiệm thu.
- Nguồn 5 lần / router / chuyển mạng / 24 giờ: người dùng báo đã test. Chưa có log hoặc biên bản từng phép thử được kiểm tra trong lần này; không ghi là agent tự chạy.
- Hai lệnh xóa mạng trên board thật: test sau.
- Worker dự án: chờ URL và mã nguồn/API từ lead.
- Tại cửa và E2E thật: làm sau.
- Nhiều cửa dùng chung key trong tài liệu là cùng private key (và cùng public key tương ứng). Hiện mỗi board tự sinh một cặp riêng, không có quy trình cấp key chung; không xuất private key thật.

## Quy trình bằng chứng thật (không xóa Wi-Fi/key)

Chạy trong `F:/vnfarm/qrcode`. Khi COM5 có lại, nạp build thường (không erase NVS):

```powershell
python -m platformio run -e s3tim -t upload --upload-port COM5
python tools/check_ntp_boot.py --port COM5 --mode boot --seconds 90
```

Sau khi công cụ mở cổng, nhấn/thả RST, không giữ BOOT. Cần thấy banner firmware, sync NTP rồi FIRST QR. Công cụ không coi log bắt đầu giữa boot là bằng chứng. File có timestamp nằm trong `web/.runtime/` và được flush từng dòng.

### Chặn NTP rồi phục hồi

Chỉ thực hiện khi có quyền quản lý router/AP của BOARD. Không chặn NTP trên Windows rồi coi là đã chặn board.

1. Trên router, chặn UDP đích 123 cho board, giữ Wi-Fi, DNS và Internet khác hoạt động. Bảo đảm không có đường NTP IPv4/IPv6 thay thế đi qua.
2. Chạy `python tools/check_ntp_boot.py --mode recovery --seconds 600` rồi nhấn RST.
3. Chờ log `NTP not synchronized; QR blocked`, kiểm tra màn hình không phát QR.
4. Bỏ chặn UDP/123, không restart board. Chờ sync và QR tự phục hồi.
5. Nếu hết thời gian khi SNTP còn đang retry, thu lại với thời lượng dài hơn; chưa có bằng chứng thì không đánh dấu PASS.

Log checker kiểm tra thứ tự quan sát, không chứng minh router đã cấu hình đúng hoặc thay việc nhìn màn hình. Ghi thời điểm chặn/mở và quan sát màn hình trong biên bản riêng.

### Đồng bộ định kỳ thật

```powershell
python tools/check_ntp_boot.py --mode periodic --seconds 3900
```

Mở capture trước, nhấn RST. Giữ nguồn/mạng ít nhất hơn một giờ. Phải có hai callback sync trong cùng boot, cách nhau ít nhất 59 phút; không coi hai callback sát nhau là sync hàng giờ. Nếu mạng làm sync trễ, kéo dài capture. Không thay giờ hoặc giảm interval để giả làm bằng chứng một giờ thật.

### QR_INFO chập chờn

Sau nạp, chạy `python tools/verify_live_contract.py --port COM5` khi web test đang hoạt động. Nếu lỗi chưa sẵn sàng, thu nguyên dòng `QR not ready; reason=... uptime_ms=... reset=...` (không cần chia sẻ URL/chữ ký). Đối chiếu banner/reset, log Wi-Fi và NTP; chưa có dữ liệu đó thì chưa khẳng định nguyên nhân.
