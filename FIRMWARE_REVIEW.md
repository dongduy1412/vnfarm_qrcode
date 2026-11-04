# Rà soát firmware theo hội thoại lead — 04/11/2026

**Kết luận: phần tạo QR có chữ ký đã chạy trên thiết bị thật; chưa hoàn thành toàn bộ phạm vi firmware trong hội thoại.**

Đối chiếu bản chép người dùng cung cấp với mã nguồn 0.1.4, log nạp, xác nhận màn hình/điện thoại của người dùng, QR thật trong ảnh và lần đọc log trực tiếp trong buổi rà soát này. Không nạp lại hoặc reset board khi rà soát.

## Đối chiếu yêu cầu

| Yêu cầu từ hội thoại | Mốc hội thoại | Trạng thái và bằng chứng |
| --- | --- | --- |
| Thiết lập Wi-Fi ban đầu | 6:42–8:25; 10:53–11:02 | Đã làm và người dùng đã kết nối, mở trang cấu hình, lấy public key. |
| Private key ký, public key để server xác minh | 4:54–5:10; 8:49–10:00 | Đã làm. Khóa tự sinh/lưu NVS, public key xuất ở trang thiết lập. Key người dùng gửi đã xác minh được chữ ký của QR thật trong ảnh. Đoạn lead đổi ý về cách cấp khóa đã được ghi nhận; bản hiện tại đi theo hướng tự sinh khóa. |
| Dữ liệu QR có thời gian sinh mã và chữ ký | 5:11–5:31 | Đã làm và xác minh bằng QR thật. Dữ liệu ảnh có device ID, fingerprint đúng key, thời gian và TTL 30 giây. Kiểm tra ảnh lịch sử chỉ xác minh tại thời điểm tạo mã, không coi QR cũ còn hạn hiện tại. |
| QR tự đổi theo chu kỳ cấu hình được | 4:08–4:36; 5:34–5:52 | Đã có cấu hình 10–60 giây, mặc định 30. Đọc log trực tiếp COM5 quan sát hai lần cập nhật có `iat` 1793789070 và 1793789100, đều TTL 30: cách nhau đúng 30 giây. Chưa đổi sang chu kỳ khác để thử trên board. |
| Thiết bị tự tạo mã, không xin server từng QR | 6:53–6:56 | Đã có trong firmware: ký bằng mbedTLS tại thiết bị, không có request lấy QR từ backend. |
| Đồng bộ giờ qua mạng khi khởi động | 7:45–8:25; 10:35–10:39 | Có code NTP và không phát QR khi giờ chưa được thiết lập. Đã có QR thật với thời gian tương ứng ảnh/giờ người dùng; chưa kiểm thử riêng cold boot sau rút nguồn. |
| Lưu cấu hình/khóa, cắm điện lại tự chạy | 10:26–10:39 | Có lưu/đọc NVS; luồng lưu cấu hình rồi ESP.restart đã chuyển sang màn hình QR và key vẫn khớp. Chưa thử rút nguồn vật lý rồi cắm lại, nên chưa đánh dấu nghiệm thu đầy đủ mục này. |
| Chế độ thiết lập không mở tùy tiện lúc chạy thường | 10:02–11:02 | Code chỉ mở AP/trang thiết lập khi chưa có cấu hình, chế độ bình thường dùng Wi-Fi STA. Chưa kiểm tra riêng danh sách AP sau thiết lập. |
| Khi bật, kết nối và duy trì luồng tới box/hệ thống quản lý để nhận lệnh admin | 12:49–13:18 | **Chưa triển khai.** Không có client/giao thức kết nối quản lý trong firmware hiện tại. |
| Nhận lệnh đổi private key | 13:23–13:29 | **Chưa triển khai.** Không có handler đổi khóa; cần xử lý cả public key mới/đăng ký tin cậy phía server. |
| Nhận lệnh xóa Wi-Fi để thiết lập lại | 13:29–13:30 | **Chưa triển khai.** Sau khi đã cấu hình, firmware chưa có luồng nhận lệnh xóa cấu hình và trở về chế độ thiết lập. Nạp lại app hiện tại không tự xóa NVS. |

## Phần cần làm để bàn giao đầy đủ firmware

1. Thống nhất endpoint, giao thức và xác thực của hệ thống quản lý; triển khai client duy trì kết nối/nhận lệnh trên thiết bị.
2. Triển khai đổi khóa và cách chuyển public key mới cho hệ thống xác minh. Kiểm tra key mới xác minh được và key cũ không còn được chấp nhận theo quy trình đã thống nhất.
3. Triển khai xóa Wi-Fi, trở lại thiết lập; giữ/xóa khóa theo đúng lệnh đã thống nhất, không tự gộp hai hành vi.
4. Thử rút/cắm nguồn: tự nối lại Wi-Fi, lấy giờ, giữ cấu hình và public key, tiếp tục đổi QR. Thử chu kỳ khác 30 giây trên board.

Không coi các nội dung ở mục 1–3 là chỉ thiếu backend: **mã firmware cho chúng cũng chưa được viết**.

## Tích hợp ngoài firmware

- URL web sự kiện và hợp đồng dữ liệu/chữ ký cần thống nhất với backend thật. Bản hiện tại dùng web test LAN, thuật toán ECDSA P-256/SHA-256 và định dạng mô tả trong `docs/QR_PROTOCOL.md`.
- Backend chịu trách nhiệm xác minh chữ ký/thời hạn và gắn check-in với người chơi. Điều kiện cho phép gửi ảnh/quét tem thuộc web/backend (lead chốt 14:55–14:56), không chạy trên mạch tím.
- Chưa có xác nhận điện thoại check-in thành công trên web test sau lần khởi động lại server; việc đã xác minh chữ ký QR trong ảnh là bằng chứng của phần ký, không thay cho nghiệm thu toàn luồng web sự kiện.

## Phân biệt yêu cầu và lựa chọn triển khai

- Lead mô tả “flash từ xa” bằng hành vi nhận lệnh xóa Wi-Fi/đổi khóa; không có đủ căn cứ thêm yêu cầu OTA tải bản firmware mới.
- Chính sách dừng QR khi mất Wi-Fi là lựa chọn của bản nền, không được lead chốt trong hội thoại; cần thống nhất trước bàn giao nếu muốn giữ hành vi này.
- QR mở trang thiết lập, mật khẩu AP `12345678` và cách hiển thị trên màn hình là các yêu cầu bổ sung đã được người dùng chốt trong chat này.

## Bằng chứng mã nguồn

- `src/main.cpp`: thiết lập/lưu/đọc Wi-Fi và chu kỳ; tạo payload/URL ký; đồng bộ NTP; vòng lặp đổi QR. Không có kết nối quản lý hoặc handler đổi khóa/xóa Wi-Fi.
- `src/DeviceKeys.cpp`: tự tạo/đọc private key từ NVS, xuất public key, ký và tự kiểm tra chữ ký.
- `src/Screen.cpp`: màn hình SPI mode 3; QR URL thiết lập và QR check-in.
- `SETUP_REPORT.md`: diễn biến kiểm tra phần cứng, nạp và xác minh QR thực tế.
