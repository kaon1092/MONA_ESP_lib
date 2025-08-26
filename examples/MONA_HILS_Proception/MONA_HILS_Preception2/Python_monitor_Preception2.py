import socket
import json
import time
import pprint

ESP32_IP = "10.216.81.142"   # 모니터링할 보드 IP (control과 동일한 IP 주소 설정)
ESP32_PORT = 8001           # 보드 포트 (연결하고자 하는 보드의 포트 입력)

def pretty_print_json(data):
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
                            print("연결이 끊겼습니다.")
                            break
                        buffer += data.decode("utf-8", errors="ignore")
                        while "\n" in buffer:
                            line, buffer = buffer.split("\n", 1)
                            line = line.strip()
                            if not line:
                                continue
                            try:
                                msg = json.loads(line)
                            except Exception as e:
                                print(f"JSON 분석 실패: {e}")
                                continue

                            file_name = msg.get("file_name", "파일명 없음")
                            self_msg = msg.get("self_message", {})
                            recv_msgs = msg.get("received_messages", {})

                            agent_id = self_msg.get("agent_id", "UNKNOWN")
                            print(f"\n--- {agent_id} 수신 내용 ---")
                            print(f"수신된 JSON 파일명: {file_name}")

                            print(f"[{agent_id}가 받은 내용]")
                            pretty_print_json(self_msg)

                            print("\n[외부 다른 보드로부터 받은 내용]")
                            pretty_print_json(recv_msgs)
                            print("-" * 50)

                    except socket.timeout:
                        # 2초 타임아웃: 계속 수신 대기
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
