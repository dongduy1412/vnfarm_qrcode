# Check-in bằng QR đổi mã: tổng quan luồng

Mục đích: người chơi phải có mặt tại cửa mới xin được tem (chống ngồi nhà chụp ảnh trên mạng).
Thiết bị ở cửa hiển thị QR đổi mã liên tục, server xác thực chữ ký và thời gian.

- Chi tiết firmware: [firmware.md](./firmware.md)
- Chi tiết worker server: [server.md](./server.md)

## Tổng quan hai đối tượng

```mermaid
flowchart LR
    subgraph FW["Firmware (thiết bị ở cửa, ESP + màn hình)"]
        direction TB
        F1["Private key (ký)"]
        F2["Đồng hồ NTP"]
        F3["Sinh QR mỗi N giây"]
        F1 --> F3
        F2 --> F3
    end

    subgraph SV["Worker server"]
        direction TB
        S1["Public key (chỉ xác thực)"]
        S2["Cấu hình TTL: 10/20/30/60s"]
        S3["Endpoint check-in"]
        S4["Trang xin tem vật phẩm"]
        S1 --> S3
        S2 --> S3
        S3 --> S4
    end

    NTP["NTP server (Google, Microsoft, time.is)"]
    U["Người chơi (điện thoại)"]

    NTP -- "đồng bộ giờ chuẩn" --> F2
    F3 -- "hiển thị QR" --> U
    U -- "quét QR, gửi t + chữ ký" --> S3
    F1 -. "copy public key qua web console" .-> S1
```

## Sequence: một lần check-in và xin tem

```mermaid
sequenceDiagram
    autonumber
    participant FW as Firmware
    participant NTP as NTP server
    participant U as Người chơi (điện thoại)
    participant W as Worker server

    FW->>NTP: Đồng bộ giờ chuẩn (lúc boot)
    NTP-->>FW: Thời gian chuẩn

    loop Mỗi N giây
        FW->>FW: t = thời điểm sinh mã<br/>sig = Sign(privKey, t)
        FW->>FW: Vẽ QR chứa URL (t, sig)
    end

    U->>FW: Quét QR trực tiếp tại cửa
    U->>W: Mở URL /checkin?t=…&sig=…
    W->>W: Verify(pubKey, t, sig)
    alt Chữ ký sai
        W-->>U: Từ chối (mã giả)
    else now - t > TTL
        W-->>U: Từ chối (mã hết hạn, nghi gửi cho người khác)
    else Hợp lệ
        W->>W: Ghi nhận check-in cho user
        W-->>U: Check-in thành công, vào trang xin tem
    end

    Note over U,W: Nếu user vào trang xin tem mà chưa check-in<br/>thì trang hiện nút "Check in" để quét mã
```

## Quyết định thiết kế đã chốt

- Chống ngồi nhà: bắt buộc check-in tại cửa. Không chặn tuyệt đối (một người ở cửa vẫn có thể gửi mã cho người khác trong TTL) nhưng giảm rất nhiều.
- Private key chỉ nằm ở firmware. Server chỉ giữ public key. Lộ public key không gây gian lận.
- TTL và chu kỳ đổi mã cấu hình được (10/20/30/60 giây). 5 giây thì rủi ro cao vì độ trễ mạng.
- Firmware cần wifi mỗi lần boot để đồng bộ NTP (mất điện là lệch giờ).
- Nhiều cửa dùng chung một private key. Dự kiến chỉ 1 đến 2 thiết bị.
- Chưa chốt: cách reset thiết bị (xem [firmware.md](./firmware.md)).
