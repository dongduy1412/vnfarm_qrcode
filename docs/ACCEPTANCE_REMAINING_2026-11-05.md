> Báo cáo lịch sử trước firmware 0.4.1. Hiện trạng mới: ENROLLMENT_LAB_041.md và ACCEPTANCE_041.md.

# Đối chiếu nghiệm thu — firmware 0.3.0

Đánh giá ngày 2026-11-05. Bốn bản docs/mentor trùng byte với bản F:/Downloads.
Đối chiếu code hiện tại và bằng chứng đã ghi trong project/cuộc làm việc;
không chạy lại test phần cứng hoặc giả định đã hoàn thành thử 24 giờ trong lần rà soát này.
Báo cáo REVIEW_AGAINST_MENTOR_DOCS_2026-11-05.md cũ mô tả phiên bản trước chuyển contract,
không dùng các thiếu sót giao thức trong báo cáo đó để đánh giá phiên bản 0.3.0.

## Kết luận

Đủ bàn giao bản demo firmware + Worker test. Chưa đủ nghiệm thu toàn bộ checklist mentor.

| Step | Hiện trạng | Còn cần để nghiệm thu |
|---|---|---|
| 1. Khóa/giao thức | P-256/SHA-256, raw public key, raw r||s, base64url; message v1.did.t, timestamp làm tròn theo N; có test vector | Phía server dự án xác nhận đặc tả và các lựa chọn đang mở trong contract |
| 2. Màn hình | Build/flash và QR thật đã hoạt động; QR toàn màn hình có logo | Đo khoảng cách quét tại vị trí cửa, ánh sáng thực tế, nhiều điện thoại |
| 3. Lưu cấu hình | Wi-Fi/key trong NVS; reconnect thật giữ 2 profile và public key; không có API xuất private key | Xác nhận bằng mất nguồn thật; chốt hàm xóa key theo phạm vi reset |
| 4. Setup | AP, nhập Wi-Fi, board tự sinh key, copy/tải public key; normal boot dùng STA và không khởi động portal | Ghi biên bản test điện thoại: lưu xong AP biến mất, không truy cập settings trong chế độ chạy |
| 5. NTP | configTime với Google và pool.ntp.org; cơ chế SNTP nền; chặn epoch chưa hợp lệ | Theo dõi lần sync NTP thành công trong boot hiện tại, kiểm tra khi NTP bị chặn và khi phục hồi; ghi bằng chứng sync định kỳ |
| 6. QR động | N=30; server verify QR thật; URL 192 ký tự trong lần test; contract vectors/test HTTP có bằng chứng pass | Chạy nhiều giờ, theo dõi RAM/reset và nhịp đổi QR; tích hợp verifier thật |
| 7. Độ bền | Có reconnect thật qua server; các ca lưu trữ mất điện mô phỏng đã pass | 5 lần rút/cắm nguồn thật; tắt/bật router; chạy liên tục 24 giờ |
| 8. Reset | 3 lệnh có xác thực, poll/ACK, journal; reconnect thử thật; forget/wipe thử mô phỏng | Chốt mâu thuẫn xóa Wi-Fi giữ key với checklist xóa Wi-Fi + key; test forget/wipe trên board thật; quy trình đổi key |
| 9. Lắp đặt/E2E | Có thiết bị/vỏ theo ảnh, check-in thành công trên Worker test | Vị trí cửa; public key trên server dự án; đăng nhập thật, QR hợp lệ/TTL quá hạn, xin tem/ảnh, vài người chơi thật |

## Các điểm phải xử lý/chốt

1. **NTP:** src/main.cpp gọi configTime nhưng coi `time(nullptr) >= 1704067200`
   là đủ để phát QR. Đây chưa phải bằng chứng đồng bộ thành công trong boot hiện tại.
   Bổ sung cờ/callback đồng bộ thật; boot phải chờ sync mới. Kiểm tra NTP lỗi,
   phục hồi và đồng bộ định kỳ. Chính sách khi mất NTP lâu cần thống nhất.
2. **Reset:** tài liệu Step 8 yêu cầu xóa cả Wi-Fi và key; ba lệnh hiện tại cố ý giữ key.
   Không tự sửa wipe_wifi thành xóa key. Mentor cần chấp nhận phạm vi reset Wi-Fi
   hoặc bổ sung factory reset riêng có bảo vệ và hướng dẫn đăng ký key mới.
   Nạp firmware thông thường hiện giữ NVS, nên không được coi là factory reset.
3. **Nhiều cửa:** overview/firmware nêu dùng chung private key. Code tự sinh key
   riêng trên mỗi board, chưa có import/cấp key chung. Nếu triển khai 2 cửa phải
   chốt dùng key riêng theo did hay quy trình cấp key chung; không xuất key thật
   từ thiết bị hiện tại để sao chép.
4. **Worker dự án:** URL build hiện trỏ Worker cá nhân và TEST_MODE=true.
   Thời hạn phiên 1800s, /stamp và login đều là cấu hình/luồng test. Cần chốt URL,
   route xin tem, CHECKIN_VALID_SECONDS, public key theo did; đội server tích hợp
   session thật, CMS, check-in và chặn API nhận ảnh/dữ kiện khi chưa check-in.
   Việc mở được website chưa chứng minh E2E này đã hoàn thành.
5. **Lệnh quản trị:** HTTPS xác minh CA/hostname, không redirect là phương án tin
   server được chọn từ các lựa chọn contract. ACK ký v1.ack.did.cmdId.t là phần
   đặc tả bổ sung. Hai bên cần xác nhận khi chuyển sang Worker dự án.

## Bài test còn thiếu bằng chứng thật

- Rút/cắm nguồn 5 lần: giữ key/Wi-Fi, tự kết nối, sync NTP mới, phát QR hợp lệ.
- Tắt/bật Wi-Fi: có trạng thái chờ, tự phục hồi; thử chuyển giữa mạng đã lưu.
- Chặn NTP lúc boot: không phát QR; mở lại NTP: tự hoạt động.
- Chạy 24 giờ: log reset, bộ nhớ, nhịp QR, kết quả verify và các lần sync.
- Quên mạng hiện tại: mạng bị xóa, các mạng khác/key còn; không có mạng khả dụng thì setup.
- Wipe Wi-Fi: xóa mọi mạng, setup lại được, key giữ theo hành vi hiện được triển khai.
- Setup hoàn tất: AP/settings tắt; public key copy trên điện thoại thành công.
- Quét QR logo tại cửa bằng nhiều máy; ghi khoảng cách và điều kiện ánh sáng.
- Trên server dự án: QR hợp lệ thành công; chờ quá TTL rồi mở mã cũ bị 410;
  cùng người/cùng mã không tạo lượt mới; người chưa check-in không gửi ảnh/xin tem được.
- Một nhóm người chơi thật đi hết luồng.

## Thứ tự hoàn thiện

1. Sửa xác nhận sync NTP; chốt reset/key chung và thông số với mentor/server.
2. Tích hợp Worker dự án, nạp cấu hình host/CA phù hợp, đăng ký public key.
3. Test chức năng thực tế, nguồn/mạng/reset và 24 giờ; ghi log/biên bản.
4. Lắp ở cửa, test người chơi và bàn giao hướng dẫn setup/reset/đổi key.

Tham chiếu: docs/mentor/firmware-tasks.md, firmware.md, contract.md, overview.md;
src/main.cpp, src/DeviceKeys.cpp, src/WifiCommands.cpp, src/CloudLink.cpp;
cloudflare/wrangler.jsonc; docs/REMOTE_WIFI_COMMANDS.md; docs/CONTRACT_MIGRATION.md;
docs/QR_PROTOCOL.md; docs/contract-vectors.json.
