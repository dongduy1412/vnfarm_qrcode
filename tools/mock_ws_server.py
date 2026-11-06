"""Mock WebSocket Server according to vnfam-verification-do endpoints.md

Listens on all local interfaces on port 8787.
Handles WebSocket upgrade at /api/devices/{deviceId}/connection.
Sends WELCOME, verifies BOOT_REPORT, replies to "ping", and allows sending test COMMANDs.
"""
import asyncio
import json
import uuid
import sys
from datetime import datetime

try:
    import websockets
except ImportError:
    print("Vui lòng cài đặt websockets: pip install websockets")
    sys.exit(1)

connected_clients = set()

async def handler(websocket):
    path = websocket.request.path if hasattr(websocket, 'request') else websocket.path
    print(f"\n[+] Thiết bị đang kết nối tới path: {path}")
    
    # 1. Gửi bản tin WELCOME ngay sau khi kết nối (mục 3.7 endpoints.md)
    session_id = str(uuid.uuid4())
    welcome_msg = {
        "version": 1,
        "type": "WELCOME",
        "eventId": str(uuid.uuid4()),
        "payload": {
            "sessionId": session_id,
            "generation": 1,
            "serverTimeMs": int(datetime.now().timestamp() * 1000)
        }
    }
    await websocket.send(json.dumps(welcome_msg))
    print(f"[->] Đã gửi WELCOME (sessionId: {session_id})")
    
    connected_clients.add(websocket)
    try:
        async for message in websocket:
            # Heartbeat ping
            if message == "ping":
                await websocket.send("pong")
                print(f"[Heartbeat] Nhận 'ping' -> Đã trả 'pong'")
                continue
                
            try:
                data = json.loads(message)
                msg_type = data.get("type")
                event_id = data.get("eventId")
                
                if msg_type == "BOOT_REPORT":
                    payload = data.get("payload", {})
                    print("\n[V] NHẬN THÀNH CÔNG BÁO CÁO KHỞI ĐỘNG (BOOT_REPORT):")
                    print(f"    - Firmware: {payload.get('firmwareVersion')}")
                    print(f"    - Target:   {payload.get('hardwareTarget')}")
                    print(f"    - Wi-Fi:    {payload.get('wifiSsid')}")
                    print(f"    - Tính năng hỗ trợ: {payload.get('capabilities')}")
                    
                    # Trả EVENT_ACK
                    ack = {
                        "version": 1,
                        "type": "EVENT_ACK",
                        "eventId": str(uuid.uuid4()),
                        "payload": {"eventId": event_id}
                    }
                    await websocket.send(json.dumps(ack))
                    print(f"[->] Đã phản hồi EVENT_ACK cho BOOT_REPORT\n")
                    
                elif msg_type == "COMMAND_ACK":
                    print(f"[<--] Thiết bị xác nhận nhận lệnh (COMMAND_ACK) opId: {data.get('operationId')}")
                elif msg_type == "COMMAND_RESULT":
                    print(f"[<--] Kết quả thực thi lệnh (COMMAND_RESULT): {data.get('payload')}")
                else:
                    print(f"[<--] Nhận bản tin: {message}")
            except json.JSONDecodeError:
                print(f"[<--] Raw message: {message}")
                
    except websockets.exceptions.ConnectionClosed as e:
        print(f"[-] Thiết bị ngắt kết nối: {e}")
    finally:
        connected_clients.remove(websocket)

async def command_sender():
    """Hỗ trợ gõ lệnh từ bàn phím để test gửi lệnh xuống ESP32"""
    loop = asyncio.get_event_loop()
    while True:
        line = await loop.run_in_executor(None, sys.stdin.readline)
        cmd = line.strip().upper()
        if not connected_clients:
            print("[!] Chưa có thiết bị nào kết nối WebSocket để gửi lệnh!")
            continue
            
        action = None
        if cmd in ("REBOOT", "RESET_WIFI", "SYNC_TIME"):
            action = cmd
        else:
            print("Lệnh hợp lệ: REBOOT, RESET_WIFI, SYNC_TIME")
            continue
            
        op_id = str(uuid.uuid4())
        command_msg = {
            "version": 1,
            "type": "COMMAND",
            "eventId": str(uuid.uuid4()),
            "operationId": op_id,
            "payload": {
                "action": action,
                "deadline": int(datetime.now().timestamp() * 1000) + 60000,
                "parameters": {}
            }
        }
        for ws in list(connected_clients):
            await ws.send(json.dumps(command_msg))
        print(f"[->] Đã bắn lệnh {action} (opId: {op_id}) xuống thiết bị")

async def main():
    port = 8787
    server = await websockets.serve(handler, "0.0.0.0", port)
    print(f"=====================================================")
    print(f"  MOCK WEBSOCKET SERVER ĐANG CHẠY TRÊN CỔNG {port}")
    print(f"  URL test cho ESP32: http://192.168.100.23:{port}")
    print(f"=====================================================")
    print("Mẹo: Sau khi ESP32 kết nối, bạn có thể gõ REBOOT, RESET_WIFI hoặc SYNC_TIME rồi Enter để test gửi lệnh.")
    
    await asyncio.gather(server.wait_closed(), command_sender())

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nĐã dừng server.")
