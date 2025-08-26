#include <WiFi.h>
#include <ArduinoJson.h>

const char* ssid = "SSID";
const char* password = "PW";
const int listenPort = 8001;

WiFiServer server(listenPort);
const char* AGENT_ID = "1";

#define MAX_CLIENTS 3 // 동시에 연결 가능한 최대 클라이언트 수
WiFiClient clients[MAX_CLIENTS];

DynamicJsonDocument selfMsgDoc(256);
DynamicJsonDocument outgoingDoc(512);

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  server.begin();
  Serial.println("TCP server started");
}

void loop() {
  // (1) 새로운 클라이언트가 오면 배열에 추가
  if (server.hasClient()) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (!clients[i] || !clients[i].connected()) {
        if (clients[i]) clients[i].stop();
        clients[i] = server.available();
        Serial.print("Client connected at slot ");
        Serial.println(i);
        break;
      }
    }
    // 배열이 다 찼으면 새 클라이언트 거절
    WiFiClient rejectClient = server.available();
    rejectClient.stop();
  }

  // (2) 각 클라이언트별 데이터 처리
  for (int i = 0; i < MAX_CLIENTS; i++) {
    WiFiClient& client = clients[i];
    if (client && client.connected()) {
      static String incomings[MAX_CLIENTS];
      static unsigned long lastSends[MAX_CLIENTS] = {0};

      // (a) 명령 데이터 수신 체크
      while (client.available()) {
        char c = client.read();
        incomings[i] += c;
        // 중괄호 체크
        int leftBraces = 0, rightBraces = 0;
        for (unsigned int k = 0; k < incomings[i].length(); ++k) {
          if (incomings[i][k] == '{') leftBraces++;
          if (incomings[i][k] == '}') rightBraces++;
        }
        if (c == '\n' || (leftBraces > 0 && leftBraces == rightBraces)) {
          incomings[i].trim();
          Serial.print("[DEBUG] slot "); Serial.print(i);
          Serial.print(" incoming.length(): "); Serial.println(incomings[i].length());
          Serial.print("[DEBUG] slot "); Serial.print(i);
          Serial.print(" incoming: "); Serial.println(incomings[i]);
          if (incomings[i].length() >= 2 && incomings[i].startsWith("{") && incomings[i].endsWith("}")) {
            DeserializationError error = deserializeJson(selfMsgDoc, incomings[i]);
            if (error) {
              Serial.println("JSON 파싱 실패: ");
              Serial.println(error.c_str());
            }
            // file_name 생성
            String fileName = "JSON_";
            if (selfMsgDoc.containsKey("agent_id")) {
              fileName += String((const char*)selfMsgDoc["agent_id"]);
            } else {
              fileName += AGENT_ID;
            }
            fileName += "_send";
            Serial.println("[DEBUG] file_name: " + fileName);
            selfMsgDoc["file_name"] = fileName;
          } else if (incomings[i].length() > 0) {
            Serial.println("데이터 O, JSON이 비정상: " + incomings[i]);
          }
          incomings[i] = "";
        }
      }

      // (b) 1초마다 상태 전송 (모든 클라이언트)
      if (millis() - lastSends[i] >= 1000) {
        outgoingDoc.clear();
        String fileName = "JSON_";
        if (selfMsgDoc.containsKey("agent_id")) {
          fileName += String((const char*)selfMsgDoc["agent_id"]);
        } else {
          fileName += AGENT_ID;
        }
        fileName += "_send";
        outgoingDoc["file_name"] = fileName;

        if (selfMsgDoc.isNull() || selfMsgDoc.size() == 0) {
          DynamicJsonDocument dummy(128);
          dummy["agent_id"] = AGENT_ID;
          dummy["info"] = "아직 명령 없음";
          outgoingDoc["self_message"] = dummy.as<JsonObject>();
        } else {
          // self_message에서 file_name 키는 제외
          DynamicJsonDocument temp(256);
          for (JsonPair kv : selfMsgDoc.as<JsonObject>()) {
            if (String(kv.key().c_str()) != "file_name") {
              temp[kv.key()] = kv.value();
            }
          }
          outgoingDoc["self_message"] = temp.as<JsonObject>();
        }
        outgoingDoc.createNestedObject("received_messages");

        String outgoingStr;
        serializeJson(outgoingDoc, outgoingStr);
        outgoingStr += "\n";

        client.print(outgoingStr);
        Serial.print("[전송된 메시지 to slot "); Serial.print(i); Serial.println("] " + outgoingStr);

        lastSends[i] = millis();
      }
    }
    // 클라이언트 연결이 끊겼으면 소켓 정리
    else if (client) {
      client.stop();
    }
  }
}
