import socket
import json
import time

ESP32_IP = "192.168.0.10"
ESP32_PORT = 8001

def pretty_print_json(data):
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    pp.pprint(data)

def monitor_board(ip, port):
    while True:
        try:
            print(f"연결 시도 중: {ip}:{port}")
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((ip, port))
                s.settimeout(2.0)

                print(f"연결 성공! {ip}:{port} 로부터 수신 대기 중...\n")
                buffer = ""
                while True:
                    try:
                        data = s.recv(1024)
                        if not data:
                            # 연결이 끊기면 break
                            print("연결이 끊겼습니다.")
                            break
                        buffer += data.decode("utf-8")
                        while "\n" in buffer:
                            line, buffer = buffer.split("\n", 1)
                            try:
                                msg = json.loads(line.strip())
                            except Exception as e:
                                print(f"JSON 파싱 실패: {e}")
                                continue

                            # --- 파일명 추출 및 출력부 ---
                            file_name = msg.get("file_name", "파일명 없음")
                            agent_id = msg.get("self_message", {}).get("agent_id", "UNKNOWN")
                            print(f"\n--- {agent_id} 수신 내용 ---")
                            print(f"수신된 JSON 파일명: {file_name}")

                            # 자기 메시지
                            print(f"[{agent_id}가 받은 내용]")
                            pretty_print_json(msg.get("self_message", {}))

                            # 외부 메시지
                            print("\n[외부 다른 보드로부터 받은 내용]")
                            pretty_print_json(msg.get("received_messages", {}))
                            print("-" * 50)

                    except socket.timeout:
                        # 데이터가 없으면 계속 대기 (2초마다 recv 재시도)
                        continue
                    except Exception as e:
                        print(f"수신 중 오류 발생: {e}")
                        break
        except Exception as e:
            print(f"연결 실패: {e}")

        print("3초 후 재연결 시도...")
        time.sleep(3)

if __name__ == "__main__":
    monitor_board(ESP32_IP, ESP32_PORT)
