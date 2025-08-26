import socket
import json
import time

# 보드 ID와 IP/포트를 환경에 맞게 채우기
# (보드 ID는 ESP 보드의 agent_id와 같은 문자열을 써야 일치가 됨)
BOARD_INFO = {
    "A": ("10.55.63.142", 8001),
    "B": ("10.55.63.201", 8002),
    # "C": ("192.168.0.12", 8001),
}

def send_to_board(board_id: str, payload: dict):
    if board_id not in BOARD_INFO:
        print(f"보드 ID {board_id}는 등록되어 있지 않습니다.")
        return

    ip, port = BOARD_INFO[board_id]
    try:
        json_message = json.dumps(payload, ensure_ascii=False)
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(3.0)
            sock.connect((ip, port))
            sock.sendall((json_message + '\n').encode('utf-8'))
            print(f"보드 {board_id} ({ip}:{port})로 메시지 전송 완료:\n{json_message}")
    except Exception as e:
        print(f"보드 {board_id} ({ip}:{port}) 연결 실패: {e}")

def build_payload(agent_id: str, speed: float, mode: str, enable: bool) -> dict:
    return {
        "file_name": f"JSON_{agent_id}",
        "agent_id": agent_id,
        "speed": speed,
        "mode": mode,
        "enable": enable,
        "message_received_ts": int(time.time() * 1000),  # epoch(ms)
    }

def main():
    print("ESP32 Dict(JSON) 메시지 전송 (단일 보드, Ctrl+C 종료)\n")
    print(f"등록 보드: {', '.join(BOARD_INFO.keys())}")

    while True:
        board_id = input("보드 ID 입력 (예: A): ").strip()
        agent_id = input("agent_id 입력(미입력 시 보드 ID 사용): ").strip()
        speed = input("speed 입력(숫자): ").strip()
        mode = input("mode 입력(예: forward/backward/left/right 등): ").strip()
        enable = input("enable 입력(True/False): ").strip()

        if "" in [board_id, speed, mode, enable]:
            print("필수 입력이 비었습니다. 다시 입력.\n")
            continue

        # agent_id가 비어 있으면 보드 ID로 설정(권장 매핑)
        if agent_id == "":
            agent_id = board_id

        try:
            speed_val = float(speed)
            enable_val = True if enable.lower() == "true" else False
        except Exception:
            print("speed는 숫자, enable은 True/False로 입력하세요.\n")
            continue

        payload = build_payload(agent_id=agent_id, speed=speed_val, mode=mode, enable=enable_val)
        send_to_board(board_id, payload)
        print("-" * 40)

if __name__ == "__main__":
    main()
