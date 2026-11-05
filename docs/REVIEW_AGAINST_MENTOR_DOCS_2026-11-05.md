# Đánh giá firmware 0.1.10 theo 4 tài liệu mentor

Ngày: 05/11/2026 (Asia/Saigon). Mã nguồn: commit `4f7c9ce`.

## Kết luận

Chưa đủ điều kiện nghiệm thu toàn bộ project theo bộ tài liệu này.
Phần nền ESP32, màn hình, lưu key/Wi-Fi, setup và ký QR đã có; demo kết nối
Cloudflare DO cũng đã chạy. Tuy nhiên giao thức QR hiện tại không tương thích
`contract.md`, chưa có check-in thật trên Worker theo contract, chưa có factory
reset xóa cả Wi-Fi và key, và còn thiếu các phép thử vận hành bắt buộc.

Kết luận trước đây “done cho mentor” chỉ áp dụng yêu cầu hẹp **tạo DO và kết nối
thử**, không phải nghiệm thu 9 step của bộ tài liệu mới.

## Tài liệu và cách đánh giá

Đã đọc đủ nội dung:

- `F:/Downloads/firmware-tasks.md`
- `F:/Downloads/contract.md`
- `F:/Downloads/overview.md`
- `F:/Downloads/firmware.md`

Các file `server.md`, `server-tasks.md`, `apps/worker` được nhắc tới nhưng chưa
được cung cấp và không có trong repo đang đánh giá. Không suy đoán mã nguồn
website sự kiện. Phần server bên dưới chỉ đánh giá Worker DO và web Python
trong repo này, cộng với yêu cầu ghép nối ghi trong 4 tài liệu.

Ưu tiên contract chi tiết cho giao tiếp; giữ nguyên các mục **[CẦN QUYẾT]** là
chưa chốt. Không tự biến Ed25519, cảm ứng, WebSocket hoặc reset từ xa thành
yêu cầu bắt buộc. Không chấm phần trăm vì checklist trộn hạng mục firmware,
server, phần cứng và nghiệm thu thực tế có trọng số rất khác nhau.

## 1. Các điểm chặn ghép firmware với server

### 1.1. Payload và URL chưa khớp contract — ưu tiên cao nhất

Contract mục 1–2 yêu cầu:

```text
t = floor(now / N) * N
message = v1.<did>.<t>
https://<host>/checkin?v=1&did=<did>&t=<t>&sig=<sig>
```

Code hiện tại:

```text
message = v1\n<dev>\n<kid>\n<iat>\n<ttl>
https://vnfam.999.vn/?v=1&dev=...&kid=...&iat=...&ttl=...&sig=...#/
```

- `src/main.cpp:251`: dựng payload và URL theo giao thức riêng hiện tại.
- `src/main.cpp:342`: dùng `now / qrSeconds` để chọn lúc đổi mã nhưng truyền
  **`now`** vào `nextQr`, không truyền `slot * qrSeconds`. Vì vậy nhịp đổi mã
  đi theo slot, còn thời gian được ký chưa làm tròn như contract.
- `tools/verify_qr.py:22`: verifier chỉ nhận đúng bộ trường cũ; không nhận
  `did`, `t` của contract.
- `src/main.cpp:274`: MAC 12 hex dùng làm ID. Giá trị này thuộc regex `did`
  của contract, nên dùng MAC làm ID không tự nó là lỗi; tên trường và giới hạn
  validator mới là điểm không khớp. ID kiểu `door-1` chưa được hỗ trợ bởi web
  test/verifier hiện tại.
- `include/AppConfig.h:32`: URL mặc định hiện là trang sự kiện `#/`, chưa phải
  route `/checkin` trong contract. URL còn có thể sửa từ trang setup, trong khi
  contract đề xuất host cố định. Cần chốt host/route với phía web.

Đã kiểm tra với một cặp key dùng thử, giữ cùng base URL để loại trừ lỗi route:
QR giao thức hiện tại được verifier hiện tại chấp nhận; QR ký đúng message và
tham số contract bị từ chối với `Missing, unexpected or duplicate parameters`.
Chỉ đổi URL mà không đổi payload/verifier sẽ không giải quyết được.

URL mẫu hiện tại dài 235 ký tự (chữ ký mẫu thay đổi độ dài), vượt khuyến nghị
khoảng 200 ký tự. Đây là khuyến nghị về khả năng quét, không phải bằng chứng
mã QR hiện tại không hợp lệ. Rút gọn contract sẽ giảm độ dày mã.

### 1.2. Public key xuất khác định dạng contract

- Contract mục 1, 5: public key là **raw key được mã hóa base64url**.
- `src/DeviceKeys.cpp:59`: xuất **PEM SubjectPublicKeyInfo**; hash `kid` tính
  từ DER SPKI. Web test cũng đăng ký PEM (`web/server.py:55`).
- Với P-256 hiện tại, SPKI DER là 91 byte; điểm công khai raw uncompressed là
  65 byte. Bỏ dòng BEGIN/END của PEM chưa biến DER SPKI thành raw key.

ECDSA P-256/SHA-256 hiện có nằm trong lựa chọn thuật toán contract cho phép,
và đã chạy trên ESP lẫn WebCrypto DO. Không bắt buộc chuyển Ed25519 chỉ vì
tài liệu ưu tiên nó. Hai phía vẫn cần chốt ECDSA nếu giữ cách này, chốt raw
public key cụ thể và chữ ký DER hay raw `r||s`: contract hiện chưa mô tả đủ
chi tiết biểu diễn chữ ký ECDSA. Code hiện dùng DER base64url, DO chuyển DER
sang raw trước khi gọi WebCrypto (`cloudflare/src/index.ts:11`).

### 1.3. Chu kỳ N và TTL đang bị gộp trong giao thức cũ

- Contract mục 4: N mặc định 30 giây ở firmware; TTL do server giữ, mặc định
  60 giây, khuyến nghị `TTL >= N + 10`; SKEW mặc định 5 giây.
- Code firmware lấy `ttl = qrSeconds`, đưa vào QR và chuỗi ký.
- Verifier kiểm tra TTL bằng policy đã đăng ký, không tin TTL tùy ý từ client,
  nhưng web test yêu cầu TTL này khớp chu kỳ firmware. Mặc định hiện là 30/30.
- `tools/verify_qr.py:45`: từ chối mọi thời gian tương lai và từ chối khi
  `age >= TTL`; contract cho SKEW và mô tả hết hạn khi `age > TTL`.

Cần sửa cả firmware và verifier/server cùng một lần. Không tự kết luận TTL
60 giây là bắt buộc trong mọi cấu hình; đó là mặc định của contract, còn
N=30/TTL=30 hiện không thỏa khoảng đệm mà contract khuyến nghị.

### 1.4. DO ping/pong chưa phải hệ thống check-in

- `src/CloudLink.cpp:110`: ký message riêng `ping-v1\n...`, gửi HTTPS mỗi
  30 giây để thử kết nối.
- `cloudflare/src/index.ts:89`: chỉ có `/health`, `/api/devices/{id}/ping`
  và `/api/devices/{id}/status`; chưa có API check-in/admin theo contract.
- DO đã xác minh được chữ ký ping và có lưu trạng thái. Điều đó chứng minh
  kết nối hai chiều và khả năng dùng P-256 trên Workers; không chứng minh
  server verify được QR/check-in cho tài khoản người chơi.
- Web Python có check-in thử qua `POST /api/checkin`, cookie phiên test trong
  RAM, không có tài khoản app, redirect `/stamp`, CMS theo contract hay kiểm
  tra check-in ở API xin tem/ảnh. Lỗi thường trả 400, chưa theo bảng 401/404/410.
- `web/server.py:106`: tạo token phiên mới mỗi lần, chưa có idempotence theo
  `(user, did, t)` như contract. Replay guard bằng nonce của DO heartbeat là
  cơ chế khác, không thay thế mục này.

Những phần này thuộc server/tích hợp, không nên ghi toàn bộ là lỗi riêng của
firmware. Nhưng thiếu chúng thì chưa nghiệm thu được project end-to-end.

### 1.5. Chưa có reset toàn bộ theo Step 8

- `src/main.cpp:45`: `SETUP_WIFI` chỉ đặt `configured=false` và restart.
  Nó chủ ý giữ key, Wi-Fi gần nhất và danh sách 5 mạng đã lưu.
- Không có handler xóa cả `qr-config`, `qr-networks` và `qr-keys`, hay hàm
  xóa/đổi key. Vì vậy chưa đáp ứng checklist factory reset của Step 8.
- Flash app bằng PlatformIO hiện **không xóa NVS**. Nhận định trong tài liệu
  “flash lại firmware” dẫn tới xóa Wi-Fi + key không tự đúng với cách nạp của
  project. Cần có bước erase/reset rõ ràng nếu chọn cách này; không thực hiện
  erase trong lần đánh giá này.
- `/api/device/poll`, `/api/device/ack`, command queue và xác thực lệnh reset
  chưa có. Theo contract đây là tùy chọn, chỉ trở thành phần phải làm sau
  khi chọn reset từ xa. Không mặc định WebSocket là bắt buộc.

## 2. Đánh giá 9 step

| Step | Trạng thái | Nhận xét |
|---|---|---|
| 1. Chốt khóa/giao thức | **Chưa đạt contract** | Có P-256 và đặc tả riêng `docs/QR_PROTOCOL.md`, nhưng khác message, URL, key format và t làm tròn; chưa có test vector chung. |
| 2. Khung firmware/màn hình | **Có chức năng; chưa đủ nghiệm thu** | Build/nạp và màn ST7789 đã chạy, QR có logo; đã có 108 lượt giải mã ảnh native trước đây. Chưa đo khoảng cách quét tại cửa và chưa có biên bản nghiệm thu QR logo lớn với nhiều điện thoại/ánh sáng. |
| 3. Lưu cấu hình | **Đạt phần lưu; còn thiếu** | Key trong `qr-keys`, cấu hình `qr-config`, tối đa 5 Wi-Fi trong `qr-networks`. Không có route/log xuất private key. Đã kiểm tra giữ key/mạng qua restart; chưa có biên bản rút điện nhiều lần và chưa có hàm xóa toàn bộ. |
| 4. Setup lần đầu | **Đạt luồng chính; lệch key format** | AP + web nhập Wi-Fi, tự tạo key, xuất public key; boot đã setup dùng STA và không chạy portal. Có bằng chứng người dùng setup được. Public key chưa là raw base64url. Tạo hoặc nhập key là phương án còn mở, không bắt buộc cả hai. |
| 5. Wi-Fi/NTP | **Có triển khai; cần gia cố/kiểm chứng** | Hai NTP server, SNTP poll định kỳ; Wi-Fi tự retry, có trạng thái chờ giờ. Chưa theo dõi callback/last-sync để chứng minh mỗi boot đã nhận NTP mới; chưa có test NTP lỗi/khôi phục và chạy đủ chu kỳ sync. |
| 6. QR động | **Có chức năng; chưa đạt contract** | Nhịp mặc định 30 giây, có ký và vẽ lại; verifier riêng đã kiểm tra chữ ký đúng/sai/hết hạn. Chưa dùng đúng payload/t của contract; chưa có xác nhận check-in Worker thật và soak test nhiều giờ. Đếm ngược là tùy chọn, không tính là thiếu bắt buộc. |
| 7. Độ bền vận hành | **Chưa nghiệm thu** | Có code phục hồi Wi-Fi và boot đọc NVS; chưa thử 5 lần mất điện, tắt/bật AP Wi-Fi, failover 2 mạng và chạy 24 giờ có theo dõi lỗi/heap. |
| 8. Reset | **Chưa đạt checklist factory reset** | USB mở setup là chức năng có ích nhưng không xóa Wi-Fi + key. Cách reset chính thức và bảo vệ thao tác chưa chốt. Reset từ xa còn tùy chọn. |
| 9. Lắp đặt/check-in thật | **Chưa nghiệm thu** | Có ảnh vỏ thiết bị, chưa xác nhận vị trí cửa/khả năng quét thực tế; chưa có luồng tài khoản → QR → check-in → xin tem/ảnh trên hệ thống thật, TTL thật và thử nhiều người. |

## 3. Chi tiết NTP cần đánh giá đúng

Không kết luận “chưa đồng bộ định kỳ” chỉ vì app gọi `configTime()` một lần:

- `src/main.cpp:288`: gọi `configTime` với `time.google.com`, `pool.ntp.org`.
- Arduino ESP32 2.0.17 trong toolchain: `esp32-hal-time.c:55` bật
  `SNTP_OPMODE_POLL`, `:59` gọi `sntp_init()`.
- SDK đang dùng cho ESP32-S3 qio_opi có
  `CONFIG_LWIP_SNTP_UPDATE_DELAY = 10800000` ms, tương đương **3 giờ**.

Đây là bằng chứng cơ chế định kỳ ở framework, chưa phải log đã quan sát một
lần đồng bộ sau 3 giờ. App chưa có SNTP notification/last-sync để theo dõi.
`src/main.cpp:327` chỉ kiểm tra giờ lớn hơn mốc năm 2024; đó là kiểm tra đồng
hồ đã có giá trị, chưa chứng minh nhận NTP mới trong lần boot này. Với warm
reset còn giữ giờ, hoặc NTP mất lâu nhưng đồng hồ còn hợp lệ, kiểm tra này
không phản ánh trạng thái sync. Nên theo dõi sự kiện đồng bộ thực sự và có
chính sách khi sync quá lâu không thành công; chốt chính sách trước khi sửa.

## 4. Những điểm tài liệu cần chốt trước khi đổi code

1. Giữ P-256 đã chạy hay đổi Ed25519? Nếu P-256, thống nhất raw public key và
   chữ ký DER hay raw với phía server. Không cần đổi thuật toán chỉ để đạt chữ
   “ưu tiên” trong bản nháp.
2. Chốt host/route `/checkin` thật; route xin tem thay `/stamp`; thời hạn
   check-in cho user. Hai route/thời hạn user là phần server.
3. `overview.md`/`firmware.md` muốn nhiều cửa dùng chung private key; bản hiện
   tại tự sinh một key riêng cho từng board và không có import. Với một board
   không cản demo, nhưng chưa hỗ trợ yêu cầu nhiều cửa dùng chung key. Phương
   án cấp key vẫn chưa chốt, cần thống nhất cùng nguyên tắc private key không
   rời board; không tự xuất key thật để sao chép.
4. Tách **đổi Wi-Fi giữ key** và **factory reset xóa cả Wi-Fi + key**. Step 8
   yêu cầu xóa cả hai, nhưng remote command mẫu chỉ tên `wipe_wifi`. Cần quyết
   định rõ mỗi lệnh xóa gì và cách bảo vệ thao tác.
5. Chốt có làm remote reset không. Demo DO có thể giữ như kênh phụ nhưng không
   phải điều kiện bắt buộc của luồng QR quang học trong contract mục 0.

## 5. Kiểm tra thực hiện trong lần đánh giá

- Đọc code ở commit `4f7c9ce` và đủ 4 tài liệu mentor.
- `python -m unittest discover -s tests`: **23/23 pass**.
- `.pio/test-wifi-profiles.exe`: **pass**, giữ/cập nhật/giới hạn/khôi phục thứ tự
  danh sách Wi-Fi. Không thay thế test chuyển mạng qua sóng thật.
- `npm run check` trong `cloudflare`: **pass**.
- Kiểm tra tương thích bằng key dùng thử: giao thức cũ được chấp nhận, URL
  contract bị verifier hiện tại từ chối do tên trường. Không dùng key thật.
- Kiểm tra cấu hình SNTP trong đúng framework/SDK đang pin.

Test đang pass là test của **giao thức cũ**, không chứng minh đạt contract mới.
Không flash/reset board, thay key, sửa firmware, triển khai Worker hoặc giả lập
đã hoàn thành thử 24 giờ trong lần đánh giá này. Bằng chứng nạp/hardware/DO
trước đây lấy từ tài liệu project và trao đổi trong buổi làm việc.

## 6. Thứ tự hoàn thiện đề xuất

1. Chốt những mục mở về thuật toán/encoding, host/route, reset, cấp key.
2. Sửa **đồng thời firmware và verifier/Worker** theo contract: message,
   did/t, t làm tròn, raw public key và TTL/SKEW phía server.
3. Sinh `contract-vectors.json` bằng **key fixture dùng riêng cho test**, không
   dùng private key board thật; chạy các trường hợp contract mục 9. Có thể giữ
   key thật nếu tiếp tục P-256 và chỉ đổi giao thức/biểu diễn public key.
4. Bổ sung cơ chế reset đã được chọn và kiểm tra boot/NTP thực sự.
5. Ghép với phía web: đăng ký public key, đăng nhập, check-in, idempotence,
   redirect và bảo vệ API xin tem/ảnh. Không tự coi phần này đã tồn tại trên
   website chỉ vì QR mở được trang.
6. Nghiệm thu phần cứng: khoảng cách quét, nhiều điện thoại, 5 lần mất điện,
   mất Wi-Fi/NTP, 2 mạng và chạy 24 giờ, rồi vài người chơi thật.

Có thể bàn giao bản hiện tại như **prototype firmware + demo DO**, kèm danh
sách phần còn thiếu; chưa nên bàn giao dưới nhãn hoàn thành theo contract này.
