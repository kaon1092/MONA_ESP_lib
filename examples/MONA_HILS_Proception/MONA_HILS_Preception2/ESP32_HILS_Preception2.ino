#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <map>
#include <ArduinoJson.h>

// 사용자 설정
const char* ssid = "SSID";
const char* password = "PW";
const String SELF_ID = "A";         // 각 보드의 고유 ID (문자: A, B ~~)
const uint16_t SERVER_PORT = 8001;  // 각 보드의 고유 포트 (8001~~)

// 전역 변수
WiFiServer server(SERVER_PORT);

// 변경된 부분으로 client를 2개로 관리한다.
WiFiClient client1;
WiFiClient client2;
DynamicJsonDocument selfMessageDoc(512);     // PC로부터 받은 데이터를 저장

std::map<String, DynamicJsonDocument*> receivedMap; // 이웃 보드별 최신 JSON 포인터 저장

unsigned long lastBroadcast = 0;
const unsigned long BROADCAST_INTERVAL = 2000;

bool upsertReceivedJson(const String& senderID, const String& jsonStr) {
  DynamicJsonDocument* doc = new DynamicJsonDocument(512);
  DeserializationError err = deserializeJson(*doc, jsonStr);
  if (err) {
    delete doc;
    return false;
  }

  auto it = receivedMap.find(senderID);
  if (it != receivedMap.end()) {
    delete it->second;
    it->second = doc;
  } else {
    receivedMap[senderID] = doc;
  }
  return true;
}

// ==== ESP-NOW 수신 콜백 ====
void receiveCallback(const uint8_t* macAddr, const uint8_t* incomingData, int len) {
  if (len <= 1) return;

  // ESP-NOW 데이터는 [sender_id_len] + [sender_id_str] + [json_string] 형식으로 진행됨
  uint8_t senderIDLen = incomingData[0];
  if (len < 1 + senderIDLen) return;

  char senderIDBuf[senderIDLen + 1];
  memcpy(senderIDBuf, &incomingData[1], senderIDLen);
  senderIDBuf[senderIDLen] = '\0';
  String senderID = String(senderIDBuf);

  if (senderID == SELF_ID) return;

  // JSON 데이터 분석 대상 문자열(후단 전체)
  String jsonStr = String((char*)(incomingData + 1 + senderIDLen));

  if (upsertReceivedJson(senderID, jsonStr)) {
    Serial.println("\n--- 이웃에게 수신됨 ---");
    Serial.printf("ID %s 로부터 메시지: %s\n", senderID.c_str(), jsonStr.c_str());
  } else {
    Serial.println("ESP-NOW JSON 분석 실패");
  }
}

// ==== ESP-NOW Broadcast 함수 ====
void broadcastMessage() {
  if (selfMessageDoc.isNull()) return; // PC로부터 받은 메시지가 없으면 Broadcast하지 않음

  uint8_t broadcastAddr[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  if (!esp_now_is_peer_exist(broadcastAddr)) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddr, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }

  // 브로드캐스트 데이터 생성
  char buffer[512];
  size_t json_payload_len = serializeJson(selfMessageDoc, buffer, sizeof(buffer));

  // ESP-NOW 데이터 구성: [sender_id_len] + [sender_id_str] + [json_string]
  uint8_t senderIDLen = SELF_ID.length();

  uint8_t data[1 + senderIDLen + json_payload_len];
  data[0] = senderIDLen;
  memcpy(&data[1], SELF_ID.c_str(), senderIDLen);
  memcpy(&data[1 + senderIDLen], buffer, json_payload_len);

  esp_now_send(broadcastAddr, data, 1 + senderIDLen + json_payload_len);

  Serial.println("\n--- 브로드캐스트 송신 ---");
  Serial.printf("보낸 JSON: %s\n", buffer);
}

// ==== WiFi + ESP-NOW 초기화 ====
void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  esp_wifi_set_ps(WIFI_PS_NONE);

  Serial.println("Wi-Fi 연결 중...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n Wi-Fi 연결 완료!");
  Serial.print("보드 IP 주소: ");
  Serial.println(WiFi.localIP());

  server.begin();
  Serial.printf("TCP 서버 시작 (포트 %d)\n", SERVER_PORT);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW 초기화 실패");
    ESP.restart();
  }

  esp_now_register_recv_cb(receiveCallback);
}

// ==== 주기적 반복 루프 ====
void loop() {
  // 1. PC로부터 명령 수신 (TCP/IP) : 새 클라이언트 수락 (최대 2개)
  WiFiClient newcomer = server.available();
  if (newcomer) {
    if (!client1 || !client1.connected()) {
      client1 = newcomer;
      Serial.println("client1 연결됨");
    } else if (!client2 || !client2.connected()) {
      client2 = newcomer;
      Serial.println("client2 연결됨");
    } else {
      newcomer.stop(); // 이미 2개 차 있으면 거절
    }
  }

  // client1에서 수신
  if (client1 && client1.connected() && client1.available()) {
    String incoming = client1.readStringUntil('\n');
    incoming.trim();
    if (incoming.length() > 0) {
      DeserializationError error = deserializeJson(selfMessageDoc, incoming);
      if (!error) {
        Serial.println("\n--- client1로부터 수신 ---");
        Serial.printf("%s\n", incoming.c_str());
      } else {
        Serial.println("client1 JSON 분석 실패");
      }
    }
  }

  // client2에서 수신
  if (client2 && client2.connected() && client2.available()) {
    String incoming = client2.readStringUntil('\n');
    incoming.trim();
    if (incoming.length() > 0) {
      DeserializationError error = deserializeJson(selfMessageDoc, incoming);
      if (!error) {
        Serial.println("\n--- client2로부터 수신 ---");
        Serial.printf("%s\n", incoming.c_str());
      } else {
        Serial.println("client2 JSON 분석 실패");
      }
    }
  }

  // 2. 주기적으로 Broadcast (ESP-NOW)
  if (millis() - lastBroadcast >= BROADCAST_INTERVAL) {
    lastBroadcast = millis();
    broadcastMessage();
  }

  // 3. 통합 데이터 모니터링 (Monitor.py) (TCP/IP)
  DynamicJsonDocument monitorDoc(1024);

  // self_message 구성
  if (!selfMessageDoc.isNull()) {
    monitorDoc["file_name"] = "JSON_" + selfMessageDoc["agent_id"].as<String>() + "_send";
    monitorDoc["self_message"] = selfMessageDoc.as<JsonObject>();
  } else {
    monitorDoc["file_name"] = "JSON_UNKNOWN_send";
    monitorDoc["self_message"]["agent_id"] = String("UNKNOWN");
  }

  // received_messages 구성
  JsonObject receivedMsgs = monitorDoc.createNestedObject("received_messages");
  for (auto const& pair : receivedMap) {
    if (pair.second) {
      receivedMsgs[pair.first] = pair.second->as<JsonObject>();
    }
  }

  // JSON 직렬화 및 전송
  String output;
  serializeJson(monitorDoc, output);
  output += "\n"; // 줄바꿈 필수

  // 두 클라이언트 모두에 JSON 송신
  if (client1 && client1.connected()) client1.print(output);
  if (client2 && client2.connected()) client2.print(output);

  //Loop의 전체 주기
  delay(200);
}
