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

/*
  Simple_collision_avoider.ino - Usage of the libraries Example
  Using the Mona_ESP library in C style.
  Created by Bart Garcia, December 2020.
  bart.garcia.nathan@gmail.com
  Released into the public domain.
*/

//Variables
bool IR_values[5] = {false, false, false, false, false};
//Threshold value used to determine a detection on the IR sensors.
//Reduce the value for a earlier detection, increase it if there
//false detections.
int threshold = 35;
//State Machine Variable
// 0 -move forward , 1 - forward obstacle , 2 - right proximity , 3 - left proximity
int state, old_state;


//Enter the SSID and password of the WiFi you are going
//to use to communicate through
const char* ssid = "SSID";
const char* password =  "PW";
//A server is started using port 80
WiFiServer wifiServer(80);

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
  //Initialize the MonaV2 robot
	Mona_ESP_init();
  //Initialize variables
  state=0;
  old_state=0;
}

void loop() {
  socket_control();
  read_IR_sensor();
  update_state();
  avoid_moving();
}

void socket_control() {
  //Create a client object
  WiFiClient client = wifiServer.available();
  //Wait for a client to connect to the socket open in the Mona_ESP
  if (client) {
    if (client.connected()) {
      //Read data sent by the client
      if (client.available()>0) {
        char c = client.read();
        //Decode and execute the obtained message
        if(c=='F'){
          Motors_forward(150);
          delay(1000);
          Motors_stop();
        }
        if(c=='B'){
          Motors_backward(150);
          delay(1000);
          Motors_stop();
        }
        if(c=='R'){
          Motors_spin_right(150);
          delay(500);
          Motors_stop();
        }
        if(c=='L'){
          Motors_spin_left(150);
          delay(500);
          Motors_stop();
        }
      }
    }
    //Client disconnects after sending the data.
    client.stop();
    Serial.println("Client disconnected");
  }
}

void avoid_moving(){
  //--------------Motors------------------------
  //Set motors movement based on the state machine value.
  if(state == 0){
    // Start moving Forward
    //Motors_forward(150);
    return
  }
  if(state == 1){
    //Spin to the left
    Motors_spin_left(100);
  }
    if(state == 2){
    //Spin to the left
    Motors_spin_left(100);
  }
    if(state == 3){
    //Spin to the right
    Motors_spin_right(100);
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

	delay(5);
}
