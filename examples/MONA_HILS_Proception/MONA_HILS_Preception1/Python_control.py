import socket
import json
import time

# 보드 ID와 IP 및 포트 매핑
BOARD_INFO = {
    "1": ("192.168.0.10", 8001),  # 예시: A보드 (ID="1")
    # 추가 보드 있으면 더 추가
}

def send_to_board(board_id, data_dict):
    if board_id not in BOARD_INFO:
        print(f"❌ 보드 ID {board_id}는 등록되어 있지 않습니다.")
        return

    ip, port = BOARD_INFO[board_id]
    try:
        file_name = f"JSON_{data_dict['agent_id']}"
        payload = {
            "file_name": file_name,
            **data_dict
        }
        json_message = json.dumps(payload, ensure_ascii=False)
        # --- 연결해서 명령 송신 후 바로 종료 ---
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(3.0)
            sock.connect((ip, port))
            sock.sendall((json_message + '\n').encode('utf-8'))
            print(f"보드 {board_id} ({ip}:{port})로 메시지 전송 완료:\n{json_message}")
            # 연결 종료 → Monitoring이 바로 재연결!
    except Exception as e:
        print(f"보드 {board_id} ({ip}:{port}) 연결 실패: {e}")

def main():
    print("ESP32 Dict(JSON) 메시지 전송 터미널 (Ctrl+C로 종료)\n")
    while True:
        board_id = input("보드 ID 입력 (예: 1): ").strip()
        agent_id = input("agent_id 입력: ").strip()
        speed = input("speed 입력(숫자): ").strip()
        mode = input("mode 입력(전진/후진/좌회전/우회전 등): ").strip()
        enable = input("enable 입력(True/False): ").strip()

        # 유효성 체크
        if "" in [board_id, agent_id, speed, mode, enable]:
            print("입력이 비어있습니다. 다시 시도하세요.\n")
            continue

        try:
            speed = float(speed)
            enable = True if enable.lower() == "true" else False
        except Exception:
            print("speed는 숫자, enable은 True/False로 입력하세요.\n")
            continue

        data = {
            "agent_id": agent_id,
            "speed": speed,
            "mode": mode,
            "enable": enable
        }

        send_to_board(board_id, data)
        print("-" * 40)

if __name__ == "__main__":
    main()
