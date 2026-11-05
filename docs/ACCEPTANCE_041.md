# Đánh giá nghiệm thu firmware 0.4.1

Đối chiếu firmware-tasks.md, firmware.md, contract.md, overview.md và giao thức
đăng ký hai request của server upstream 2f26f9b. Dùng bằng chứng đã kiểm tra,
không chạy lại hoặc tự coi test 24 giờ đã hoàn thành trong lần rà soát này.

## Kết luận

Hai API theo phạm vi lead vừa giao đã chạy trên board thật với Worker test riêng:
challenge 201, complete 201, WELCOME, BOOT ACK, identity lưu qua restart, QR verify.
Không nhập token thủ công, không CMS approve, không cố định URL lead trong firmware.
Có thể bàn giao mốc kết nối để lead thử server của họ. Chưa gọi toàn bộ firmware
checklist hoặc toàn bộ tính năng CMS là đã nghiệm thu.

## Còn để chốt mốc kết nối

- Test cuối trên điện thoại: mở setup, nhập URL Worker test và lưu; board kết nối.
  Lần test thành công vừa rồi đổi URL qua USB để giữ Wi-Fi; Playwright kiểm tra
  form không thay thế phép thử nhập/lưu trên board thật.
- Lead nhập URL thật và kiểm tra cùng hai API/BOOT trên môi trường của họ.
  Đây là phép thử tích hợp bàn giao, không yêu cầu hardcode URL hoặc đổi thuật toán.
- Recovery khi response complete bị mất: firmware dừng an toàn, chưa có luồng
  nhập pairing recovery; cần thực hiện thêm nếu yêu cầu vận hành bao gồm trường hợp này.

## Đối chiếu checklist gốc

- Step 1: P-256, raw key/signature, message v1.did.t, N=30 và vector đã có.
- Step 2: build/flash/QR hoạt động; khoảng cách quét tại cửa và nhiều điện thoại còn test thực địa.
- Step 3: NVS key/Wi-Fi/identity, restart giữ dữ liệu đã có bằng chứng; test cúp nguồn theo checklist cần biên bản.
- Step 4: AP/setup/copy key có; kiểm tra nhập server bằng điện thoại và AP tắt sau lưu trên bản cuối.
- Step 5: NTP gate/callback và interval một giờ có, sync thật đã quan sát;
  chặn/mở NTP và quan sát sync định kỳ một giờ còn thiếu bằng chứng.
- Step 6: QR ký thật/server verify đạt; nhiều giờ/RAM/chu kỳ còn cần log độ bền.
- Step 7: tài liệu project ghi người dùng báo test 5 lần nguồn/router/24 giờ;
  chưa kiểm tra biên bản nên không xác nhận đã đạt cho build 0.4.1.
- Step 8: giữ key khi đổi Wi-Fi theo quyết định người dùng; khác nguyên văn
  xóa Wi-Fi + key của checklist, cần mentor chấp nhận. Reset polling cũ hoạt động
  ở chế độ Worker cũ; 0.4.1 KHÔNG có RESET_WIFI/REBOOT qua WebSocket dự án.
- Step 9: QR valid trên test Worker không phải check-in người chơi/nhận tem;
  ghép backend tổng, tại cửa và vài người chơi thật chưa nghiệm thu.

## Phạm vi server CMS chưa làm trong firmware này

Capabilities WebSocket đang rỗng, chưa có REQUEST_STATUS, APPLY_CONFIG, REBOOT,
RESET_WIFI, SCAN_WIFI, rotation, OTA. Chưa đọc desired-state/time-settings.
Đây là phần ngoài mốc kiểm tra hai API hiện tại; không khai đã hỗ trợ trên CMS.
Những tính năng này chỉ trở thành việc cần hoàn thành nếu nằm trong phạm vi bàn giao tiếp theo.

## Các quyết định cần ghi lại

Nhiều cửa dùng chung key trong tài liệu gốc chưa được triển khai; mỗi board tự
sinh key riêng theo thiết kế server mới. Nếu dùng hai cửa, mentor cần chốt key
riêng theo deviceId hay cấp key chung. Không xuất private key thật để sao chép.

Bằng chứng chi tiết: docs/ENROLLMENT_LAB_041.md. Worker test riêng:
https://vnfarm-enrollment-lab.dong141220047.workers.dev.
