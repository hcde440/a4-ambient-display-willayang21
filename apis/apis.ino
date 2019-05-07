/*
 * This is a sketch that gets the ESP8266 to the WiFi and then connects to 
 * 2 APIs to retrieve real time and sunset time APIs. 
 * It will see if it's after sunset, and parse the door information from the MQTT. 
 * Finally it’s connected to Adafruit IO, enabling IFTTT to send notifications to my phone.
*/
#include "config.h"

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "Wire.h"           
#include <PubSubClient.h>   
#include <ArduinoJson.h>    
#include <Adafruit_Sensor.h>

//const char* ssid = "NETGEAR38"; // wifi name
//const char* pass = "imaginarysky003"; // wifi password

const char* ssid = "University of Washington"; // wifi name
const char* pass = ""; // wifi password

const String key = "b226ebd2ec194b655cb23e4e46a41e10"; // the key to get IP address and locations
const String weatherKey = "7bc7661254d77ecdaf02640767252c88"; // the key to get weather info

#define mqtt_server "mediatedspaces.net"  //this is its address, unique to the server
#define mqtt_user "hcdeiot"               //this is its server login, unique to the server
#define mqtt_password "esp8266"           //this is it server password, unique to the server

WiFiClient espClient;             
PubSubClient mqtt(espClient);    

char mac[6]; //A MAC address is a 'truly' unique ID for each device
char message[201]; // 201, as last character in the array is the NULL character, denoting the end of the array
int sunset;
int curr;
  
// set up the 'sunset' feed
AdafruitIO_Feed *RTC = io.feed("sunset");

void setup() {
  Serial.begin(115200); // initialize serial communications
  delay(10); // delay 10 miliseconds 
  
  wifi_connect();
  String ipAddress = getIP(); // create a variable to store the IP address

  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback); //register the callback function

//   connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());
  RTC->get();
  
}


void loop() {
 
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop(); // this keeps the mqtt connection 'active'
  // print out results
  getTime();
  getSunset();

  delay(5000);
}


// Monitor the connection to MQTT server, if down, reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe("willa/DOOR"); //we are subscribing to 'theTopic' and all subtopics below that topic
    } else {                        //please change 'theTopic' to reflect your topic you are subscribing to
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

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
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");
  
  DynamicJsonBuffer jsonBuffer; // create the buffer
  JsonObject& root = jsonBuffer.parseObject(payload); //parse it!
  
  if (String(topic) == "willa/DOOR" ) {
    if ( afterSunset() == false) {
      Serial.println("Printing Messages...");
      String message = root["Motion"];
      Serial.print("Motion: ");
      Serial.println(message);
      io.run();  
      Serial.println("AFTER RUNNING...");    
  //    RTC->save("Someone is entering your home!");
      RTC->save(curr);
      Serial.println("AFTER SAVING..."); 
    }

  }
  
  if (!root.success()) { 
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

void getSunset() {
  HTTPClient theClient;  // initialize the HTTPClient
  String apiCall = "http://api.openweathermap.org/data/2.5/weather?q=Seattle"; // store the base url
  apiCall += "&units=imperial&appid=";  // add more conditions
  apiCall += weatherKey;  // add weather api key
  Serial.println("Making API call");    
  theClient.begin(apiCall);  // start listening for the incoming connection
  int httpCode = theClient.GET();  // make a HTTP request
  if (httpCode > 0) {    // if the response is not empty
    if (httpCode == HTTP_CODE_OK) {  // checks the connection to the endpoint
      Serial.println("Received HTTP payload.");        
      String payload = theClient.getString();  // get the request response payload
      DynamicJsonBuffer jsonBuffer;   // intialize the JsonButton
      JsonObject& root = jsonBuffer.parseObject(payload);  // parse the json
      Serial.println("Parsing...");
      if (!root.success()) {  // if the parsing fails
        Serial.println("parseObject() failed in getSunset().");
        return;
      }
      sunset = root["sys"]["sunset"].as<int>(); // store the unix time as integers
//      int sunset = root["sys"]["sunset"]; // store the unix time as integers
    }
  }
  else {
    Serial.printf("Something went wrong with connecting to the endpoint in getSunset().");
  }
}


// get cat facts from the api of cat-fact.herokuapp.com
// get cat facts from the api of cat-fact.herokuapp.com
void getTime() {
  HTTPClient theClient;  // initialize the HTTPClient
  Serial.println("Making HTTP request for time");  
  theClient.begin("http://worldtimeapi.org/api/ip"); // start listening for the incoming connection
  int httpCode = theClient.GET();   // make a HTTP request
  if (httpCode > 0) {               // if the response is not empty
    if (httpCode == 200) {          // check the connection to the endpoint
      Serial.println("Received HTTP payload.");     
      DynamicJsonBuffer jsonBuffer;   //  intialized the JsonBuffer
      String payload = theClient.getString();  // get the request response payload
      Serial.println("Parsing...");
      JsonObject& root = jsonBuffer.parse(payload);  // parse the json
      // Test if parsing succeeds.
      if (!root.success()) {
        Serial.println("parseObject() failed");
        Serial.println(payload);
        return;
      }
      curr = root["unixtime"].as<int>();
    } else {
      Serial.println("Something went wrong with connecting to the endpoint.");
    }
  }
}


bool afterSunset() {
  if (curr > sunset) {
    return true;
  } else {
    return false;
  }
}



//  create an adafruit feed to display the time
//  upload to adafruit io periodically
//if (time == sunset) 
//  trigger a message
//  adafruit io asks me to turn on the sensor or not
//  
//  turn on the sensor from adafruit;
//  if (people are moving)
//    alarm rings and LED lights up
