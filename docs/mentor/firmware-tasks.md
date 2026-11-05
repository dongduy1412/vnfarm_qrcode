# Firmware: các step công việc, checklist, kết quả đầu ra

Thiết kế xem [firmware.md](./firmware.md). Phần server xem [server-tasks.md](./server-tasks.md).

## Step 1. Chốt thiết kế khóa và giao thức QR

Việc cần làm:
- Chọn thuật toán ký chạy được trên ESP (Ed25519 hoặc ECDSA).
- Chốt định dạng payload và URL: `https://<host>/checkin?t=<unix>&sig=<base64url>`.
- Chốt chu kỳ đổi mã N (mặc định 30 giây) và quy tắc tính `t` (làm tròn theo N).

Checklist:
- [ ] Đã chọn thuật toán và thư viện ký trên ESP
- [ ] Đã chốt định dạng chuỗi được ký (ví dụ chỉ `t`, hoặc `t` kèm id thiết bị)
- [ ] Đã thống nhất format với server (xem Step 1 của server-tasks)

Kết quả đầu ra: một đoạn đặc tả ngắn (thuật toán, payload, URL, chu kỳ) dùng chung cho firmware và server.

## Step 2. Dựng khung firmware và hiển thị màn hình

Việc cần làm:
- Khởi tạo project, driver màn hình.
- Vẽ được một QR tĩnh từ chuỗi bất kỳ lên màn hình.

Checklist:
- [ ] Build và flash được lên thiết bị
- [ ] Màn hình hiển thị QR, điện thoại quét đọc được
- [ ] QR đủ lớn, đủ tương phản để quét từ khoảng cách cửa

Kết quả đầu ra: firmware chạy được, hiện QR tĩnh quét được.

## Step 3. Lưu cấu hình bền vững

Việc cần làm:
- Lưu wifi và private key vào flash (NVS hoặc tương đương).
- Đọc lại khi boot, phân biệt trạng thái "chưa setup" và "đã setup".

Checklist:
- [ ] Rút điện cắm lại vẫn giữ cấu hình
- [ ] Private key không bao giờ hiển thị hoặc gửi ra ngoài
- [ ] Có hàm xóa cấu hình (dùng cho reset ở Step 8)

Kết quả đầu ra: module cấu hình bền vững, đọc ghi được wifi và key.

## Step 4. Chế độ setup lần đầu

Việc cần làm:
- Khi chưa setup: bật AP và trang settings.
- Trang cho nhập wifi, tạo hoặc nhập private key, hiển thị public key để copy.
- Sau khi lưu xong thì tắt AP và trang settings.

Checklist:
- [ ] Vào được trang settings bằng điện thoại
- [ ] Nhập wifi và lưu thành công
- [ ] Public key hiển thị và copy được, không có chỗ set public key ngược vào
- [ ] Sau setup, AP và trang settings đã tắt (không ai vào nghịch được)

Kết quả đầu ra: quy trình setup một lần, thiết bị có public key để dán vào web console server.

## Step 5. Kết nối wifi và đồng bộ giờ NTP

Việc cần làm:
- Boot: kết nối wifi, đồng bộ giờ qua NTP.
- Cấu hình nhiều NTP server dự phòng (Google, Microsoft, time.is hoặc pool.ntp.org).
- Chưa có giờ chuẩn thì không sinh QR.

Checklist:
- [ ] Đồng bộ giờ thành công sau mỗi lần cắm điện
- [ ] Rớt wifi hoặc NTP lỗi thì thử lại, hiện trạng thái trên màn hình
- [ ] Không hiện QR khi chưa có giờ chuẩn
- [ ] Đồng bộ lại giờ định kỳ để tránh trôi đồng hồ

Kết quả đầu ra: thiết bị luôn có giờ đúng trước khi sinh mã.

## Step 6. Sinh QR đổi mã

Việc cần làm:
- Mỗi N giây: tính `t`, ký bằng private key, dựng URL, vẽ lại QR.
- Đổi mã đúng nhịp, không giật hoặc đứng.

Checklist:
- [ ] `sig` do server verify được bằng public key
- [ ] QR đổi đúng chu kỳ N
- [ ] Chạy liên tục nhiều giờ không treo hay rò bộ nhớ
- [ ] Có thể hiện đếm ngược nhỏ để người dùng biết khi nào mã đổi (tùy chọn)

Kết quả đầu ra: QR đổi mã tự động, server xác thực được.

## Step 7. Độ bền khi vận hành

Việc cần làm:
- Mất điện rồi có điện lại: tự chạy lại hoàn toàn, không cần thao tác.
- Mất wifi tạm thời: tự nối lại.

Checklist:
- [ ] Test rút điện và cắm lại 5 lần liên tiếp đều tự lên
- [ ] Test tắt wifi rồi bật lại thì tự phục hồi
- [ ] Chạy thử liên tục 24 giờ không lỗi

Kết quả đầu ra: thiết bị chạy bền, cắm điện là dùng được.

## Step 8. Cơ chế reset (cần quyết)

Việc cần làm: chọn một hoặc nhiều cách.
- Flash lại firmware (luôn có).
- Chạm màn hình 5 lần rồi nhập PIN (chỉ khi màn hình có cảm ứng).
- Nút cứng đấu ra ngoài vỏ.
- Lệnh xóa wifi từ xa do server gửi.

Checklist:
- [ ] Đã chọn cách reset và ghi lại quyết định
- [ ] Reset xóa sạch wifi và key, thiết bị quay về chế độ setup
- [ ] Cách reset không bị người lạ lợi dụng

Kết quả đầu ra: quy trình reset được chốt và hoạt động.

## Step 9. Lắp đặt và nghiệm thu

Checklist:
- [ ] Vỏ in 3D và vị trí đặt ở cửa
- [ ] Public key đã đăng ký trên server, quét QR thật thì check-in thành công
- [ ] Quét mã cũ quá TTL thì server từ chối
- [ ] Chạy thử với vài người chơi thật

Kết quả đầu ra: thiết bị đặt tại cửa, check-in end-to-end hoạt động.

## Tổng hợp kết quả đầu ra của firmware

- Firmware chạy ổn định, có chế độ setup một lần.
- Public key để đăng ký lên server.
- QR đổi mã theo chu kỳ N, ký bằng private key.
- Quy trình reset và hướng dẫn vận hành ngắn.
