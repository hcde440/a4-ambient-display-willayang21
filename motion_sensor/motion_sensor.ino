/*
 * This is a sketch that gets the ESP8266 connected to the motion sensor.
 * It sends door status to the MQTT in JSON format for another ESP to parse.
*/


#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "Wire.h"  
#include <PubSubClient.h>   
#include <ArduinoJson.h>    

//const char* ssid = "NETGEAR38"; // wifi name
//const char* pass = "imaginarysky003"; // wifi password

const char* ssid = "University of Washington"; // wifi name
const char* pass = ""; // wifi password

#define mqtt_server "mediatedspaces.net"  //this is its address, unique to the server
#define mqtt_user "hcdeiot"               //this is its server login, unique to the server
#define mqtt_password "esp8266"           //this is it server password, unique to the server

WiFiClient espClient;             
PubSubClient mqtt(espClient);  

char mac[6]; //A MAC address is a 'truly' unique ID for each device
char message[201]; // 201, as last character in the array is the NULL character, denoting the end of the array

int inputPin = 2;               // choose the input pin (for PIR sensor)
int pirState = LOW;             // we start, assuming no motion detected
int val = 0;                    // variable for reading the pin status

void setup() {
  
  pinMode(inputPin, INPUT);     // declare sensor as input
  delay(10); // delay 10 miliseconds 

  Serial.begin(115200);
  wifi_connect();
  String ipAddress = getIP(); // store the IP address

  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback); //register the callback function
  
}
 
void loop(){
 
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop(); // this keeps the mqtt connection 'active'

  val = digitalRead(inputPin);  // read input value
  if (val == HIGH) {            // check if the input is HIGH
    if (pirState == LOW) { // if PIR sensor is low
      // we have just turned on
      Serial.println("Motion detected!"); 
      sprintf(message, "{\"Motion\":\"%s\"}", "detected"); // generate a json formatted message
      Serial.println(message); 
      mqtt.publish("willa/DOOR", message); // publish to MQTT
     
      // We only want to print on the output change, not state
      pirState = HIGH;
    }
  } else { // if the input is LOW
    if (pirState == HIGH){ // if PIR sensor is high
      // we have just turned of
      Serial.println("Motion ended!");
      sprintf(message, "{\"Motion\":\"%s\"}", "ended"); // generate a json formatted message
      mqtt.publish("willa/DOOR", message);  // publish to MQTT
      Serial.println(message);       
      // We only want to print on the output change, not state
      pirState = LOW;
    }
  }
}

// Monitor the connection to MQTT server, if down, reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID
      Serial.println("connected");
      mqtt.subscribe("willa/+"); //we are subscribing to 'theTopic' and all subtopics below that topic
    } else {                        //please change 'theTopic' to reflect your topic you are subscribing to
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// connects wifi
void wifi_connect() {
  // print wifi name
  Serial.print("Connecting to "); 
  Serial.println(ssid); 
  
  WiFi.mode(WIFI_STA); // start connecting to wifi
  WiFi.begin(ssid, pass); // initialize the wifi name and pw

  while (WiFi.status() != WL_CONNECTED) {   // keep connecting until the wifi is connected
    delay(500); // wait for 0.5 sec
    Serial.print(".");  // print dots
  }

  // inform the wifi is connected and print the IP address
  Serial.println(); Serial.println("WiFi connected"); Serial.println();
  Serial.print("Your ESP has been assigned the internal IP address ");
  Serial.println(WiFi.localIP());   
  
}

//By subscribing to a specific channel or topic, we can listen to those topics we wish to hear.
//We place the callback in a separate tab so we can edit it easier
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic); //'topic' refers to the incoming topic name
  Serial.println("] ");

  DynamicJsonBuffer  jsonBuffer; // create the buffer
  JsonObject& root = jsonBuffer.parseObject(payload); //parse it!
//  JsonObject& alaa = jsonBuffer.parseObject(payload); //parse it!  
  
  if (!root.success()) { // well?
    Serial.println("parseObject() failed, are you sure this message is JSON formatted.");
    return;
  }
  
  root.printTo(Serial); //print out the parsed message
  Serial.println(); //give us some space on the serial monitor read out
}

// get the IP Address
String getIP() {
  HTTPClient theClient;   // Initialize the client library
  String ipAddress;       // create a string
  theClient.begin("http://api.ipify.org/?format=json");  // start listening for the incoming connection
  int httpCode = theClient.GET();  // make a HTTP request
  if (httpCode > 0) {     // if the response is not empty
    if (httpCode == 200) {  // check the connection to the endpoint                                
      DynamicJsonBuffer jsonBuffer;  // creates an entry point that handles
                                     //  the memory management and calls the parser
      String payload = theClient.getString();  // get the request response payload
      JsonObject& root = jsonBuffer.parse(payload);  // parse the json
      ipAddress = root["ip"].as<String>();   // return and store the ipaddress as strings
    } else {  // bad connection to the endpoint
      Serial.println("Something went wrong with connecting to the wifi endpoint.");
      return "error";    // error message
    }
  }
  return ipAddress;  // return an IPaddress if the response is valid
}
