/*
  Socket_control.ino -
  Control the Mona ESP through the network using sockets.
  Created by Bart Garcia, January 2021.
  bart.garcia.nathan@gmail.com
  Released into the public domain.
===========================================================
	To use:
	-In this code modify  line 26 and 27, set the ssid and password of
	the network that will be used to control Mona_ESP.
	-Compile and upload the code to Mona_ESP.
	-Open a serial terminal (For example the arduino serial Monitor)
	and check what is the IP given to Mona_ESP
	-Modify in the file 'Control_client.py' the host value, enter
	the IP read from the terminal in the previous step. Save the file
	-Run with python the file 'Control_client.py'
	-Enjoy controlling Mona_ESP through the network.
*/
//Include the Mona_ESP library
#include "Mona_ESP_lib.h"
#include <Wire.h>
#include <WiFi.h>

//Variables
bool IR_values[5] = {false, false, false, false, false};
//Threshold value used to determine a detection on the IR sensors.
//Reduce the value for a earlier detection, increase it if there
//false detections.
int threshold = 50;
//State Machine Variable
// 0 -move forward , 1 - forward obstacle , 2 - right proximity , 3 - left proximity
int state=0, old_state=0;

//Enter the SSID and password of the WiFi you are going
//to use to communicate through
const char* ssid = "NetworkForMonaESP";
const char* password =  "WeLoveMONA123";
//A server is started using port 80
WiFiServer wifiServer(80);

//Socket 통신 유지 관련 전역 변수
WiFiClient activeClient;         
char pendingCmd = 0;             // 읽어둔 마지막 명령 저장
unsigned long lastClientRX = 0; 

// 함수 선언
void read_IR_sensor();
void update_state();
void avoid_moving();
void socket_read_nonblocking();
void socket_control(char c);

void setup() {
  //Initialize the MonaV2 robot
	Mona_ESP_init();
	//Turn LEDs to show that the Wifi connection is not ready
	Set_LED(1,20,0,0);
	Set_LED(2,20,0,0);
  //Initialize serial port
  Serial.begin(115200);
  //Connect to the WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print("Connecting to WiFi ");
    Serial.print(ssid);
    Serial.println("....");
  }
  Serial.println("Connected to the WiFi network");
  //Print the IP of the Mona_ESP, which is information
  //needed communicate throught the sockets.
  Serial.println(WiFi.localIP());
  //Start the server as a host
  wifiServer.begin();
  wifiServer.setNoDelay(true);   // 작은 패킷 즉시 전송

	//Blink Leds in green to show end of booting/connecting
	Set_LED(1,0,20,0);
	Set_LED(2,0,20,0);
	delay(500);
	Set_LED(1,0,0,0);
	Set_LED(2,0,0,0);
	delay(500);
	Set_LED(1,0,20,0);
	Set_LED(2,0,20,0);
	delay(500);
	Set_LED(1,0,0,0);
	Set_LED(2,0,0,0);

  //회피 초기설정
  //Initialize variables
  state=0;
  old_state=0;

  pendingCmd = 0;
  lastClientRX = millis();
}

void loop() {
  // TCP 세션 유지 위해 keyboard 명령어 항상 읽기 수행 (회피 중에도 수행함)
  socket_read_nonblocking();

  // 센서/상태 갱신
  read_IR_sensor();
  update_state();

  bool obs = (state != 0);
  static bool was_obs = false;

  if (obs) {
    // 장애물 O: 회피만 수행 (키 실행은 보류)
    avoid_moving();
  } else {
    // 회피 이후 1회 정지
    if (was_obs) {
      Motors_stop();
    }
    // 장애물 X : 저장된 키를 실행 (socket_read_nonblocking에서 저장된 값)
    if (pendingCmd) {
      socket_control(pendingCmd);
      pendingCmd = 0;   // 소진
    }
  }

  was_obs = obs;
  delay(10);
}

void socket_control(char c) {
  if(c=='F'){
    Motors_forward(150);
    delay(1000);
    Motors_stop();
  }
  else if(c=='B'){
    Motors_backward(150);
    delay(1000);
    Motors_stop();
  }
  else if(c=='R'){
    Motors_spin_right(150);
    delay(500);
    Motors_stop();
  }
  else if(c=='L'){
    Motors_spin_left(150);
    delay(500);
    Motors_stop();
  }
  else {
    Motors_stop();
  }
}

// 소켓 읽기만 수행 -> 통신 연결 유지
void socket_read_nonblocking() {
  // 연결 미존재 or 끊겼으면 통신 accept
  if (!activeClient or !activeClient.connected()) {
    activeClient = wifiServer.available();
    if (activeClient) {
      activeClient.setNoDelay(true);
    }
  }

  // 연결 존재 시 수신 버퍼 비우기(마지막 명령만 저장)
  if (activeClient && activeClient.connected()) {
    while (activeClient.available() > 0) {
      char c = activeClient.read();
      if (c=='F' or c=='B' or c=='L' or c=='R' or c=='S') {
        pendingCmd = c;
      }
    }
  }
}

void avoid_moving(){
  //--------------Motors------------------------
  //Set motors movement based on the state machine value.
  static unsigned long turn_until = 0;
  unsigned long now = millis();

  if (now >= turn_until) {
    if(state == 0){
      // Start moving Forward
      //Motors_forward(150);
      Motors_stop();
      return;
    }
    else if(state == 1){
      //Spin to the left
      Motors_spin_left(100);
    }
    else if(state == 2){
      //Spin to the left
      Motors_spin_left(100);
    }
    else if(state == 3){
      //Spin to the right
      Motors_spin_right(100);
    }
    //turn_until = now +250;
  }
}

void read_IR_sensor(){
  //--------------IR sensors------------------------
  //Decide future state:
	//Read IR values to determine maze walls
  IR_values[0] = Detect_object(1,threshold);
  IR_values[1] = Detect_object(2,threshold);
  IR_values[2] = Detect_object(3,threshold);
  IR_values[3] = Detect_object(4,threshold);
  IR_values[4] = Detect_object(5,threshold);
}

void update_state(){
	//--------------State Machine------------------------
	//Use the retrieved IR values to set state
	//Check for frontal wall, which has priority
	if(IR_values[2] or IR_values[3] or IR_values[4]){
		state=1;
	}
	else if(IR_values[0]){ //Check for left proximity
		state=3;
	}
	else if(IR_values[4]){// Check for right proximity
		state=2;
	}
	else{ //If there are no proximities, move forward
		state=0;
	}

	//delay(5);
}
