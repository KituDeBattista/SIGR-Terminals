////////////////////////////////////////// DECLARATIONS //////////////////////////////////////////

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Adafruit_I2CDevice.h>
#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <ArduinoJson.h>

#define WIFI_SSID "Kituuu"
#define WIFI_PASSWORD "41819096"
#define MQTT_HOST IPAddress(192, 168, 101, 7)
#define MQTT_PORT 1883
#define MSG	(5)
#define TFT_CS         2
#define TFT_RST        14                                            
#define TFT_DC         12
#define TFT_MOSI 13  // Data out
#define TFT_SCLK 15  // Clock out
// Color definitions
#define BLACK 0x0000
#define RED 0x001F
#define BLUE 0xF800
#define GREEN 0x07E0
#define YELLOW 0x07FF
#define MAGENTA 0xF81F
#define CYAN 0xFFE0
#define WHITE 0xFFFF

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

StaticJsonDocument<200> data;
StaticJsonDocument<200> tig;

unsigned long timer, dally;

bool flag = 0;
bool flagCall = 0;
bool flagAccepted = 0;
bool flagRejected = 0;
bool flagMessage = 0;

char msg[MSG];
char *json;

const char* TIGID = "01";
const char* bed;
const char* room;
const char* pacient;
const char* diagnosis;
String tigJSON;

void call();
void escribir(byte x_pos, byte y_pos, const char *text, byte text_size, uint16_t color);
void accepted();
void rejected();
void JSON();
IRAM_ATTR void interruptAccepted();
IRAM_ATTR void interruptRejected();

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

////////////////////////////////////////// MQTT //////////////////////////////////////////

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  mqttClient.subscribe("SIGR/TIG", 2);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.println("Publish received.");
  Serial.println(topic);
  Serial.println(payload);
  json = payload;
  flagMessage = 1;
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
}

////////////////////////////////////////// DISPLAY //////////////////////////////////////////

void call(){
  tig["room"] = "02";
  tig["bed"] = "08";
  tig["pacient"] = "Rolhaiser";
  tig["diagnosis"] = "hola";
  serializeJson(tig, tigJSON);
  Serial.println(tigJSON);
  DeserializationError error = deserializeJson(data, tigJSON);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

 
  bed = data["bed"]; 
  room = data["room"];
  pacient = data["pacient"];
  diagnosis = data["diagnosis"];
  tft.fillRect(0, 32, 128, 26, RED);
  tft.fillRect(0, 58, 128, 54, BLACK);
  tft.fillRect(0, 112, 128, 33, BLUE);
  tft.fillRect(0, 143, 128, 2, BLACK);
  tft.fillRect(63, 60, 2, 50, WHITE);
  tft.fillRect(0, 145, 64, 19, GREEN);
  tft.fillRect(64, 145, 64, 19, RED); 
  escribir(25,37,"C A L L",2,WHITE);
  escribir(19,62,"ROOM",1,WHITE);
  escribir(89,62,"BED",1,WHITE);
  escribir(8,76, room,4,CYAN);
  escribir(76,76, bed,4,CYAN);
  escribir(3,117, pacient,1,WHITE);
  escribir(3,131, diagnosis,1,WHITE);
  escribir(16, 149,"Acept",1,BLACK);
  escribir(77,149,"Decline",1,BLACK);
  digitalWrite(0, HIGH);
  flagCall = 1;
}

void accepted(){
  tft.fillScreen(GREEN);
  escribir(15,89,"ACCEPTED",2,BLACK);
  escribir(16,88,"ACCEPTED",2,WHITE);
  digitalWrite(0, HIGH);
  Serial.println("Accepted");
  tig["TIGID"] = TIGID;
  tig["answer"] = "A";
  tig["bed"] = bed;
  serializeJson(tig, tigJSON);
  Serial.println(tigJSON);
  const char* tigmsg = tigJSON.c_str();
  Serial.println(tigmsg);
  mqttClient.publish("SIGR/TIGAnswer", 2, true, tigmsg);
  timer = millis();
  flag = 1;
  tigJSON = "0";
}

void rejected(){
  tft.fillScreen(RED);
  escribir(15,89,"REJECTED",2,WHITE);
  escribir(16,88,"REJECTED",2,BLACK);
  digitalWrite(0, HIGH);
  Serial.println("Rejected");
  Serial.println("Accepted");
  tig["TIGID"] = TIGID;
  tig["answer"] = "D";
  tig["bed"] = bed;
  serializeJson(tig, tigJSON);
  const char* tigmsg = tigJSON.c_str();
  mqttClient.publish("SIGR/TIGAnswer", 2, true, tigmsg);
  timer = millis();
  flag = 1;
  tigJSON = "0";
}

void escribir(byte x_pos, byte y_pos, const char *text, byte text_size, uint16_t color) {
  tft.setCursor(x_pos, y_pos);
  tft.setTextSize(text_size);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
  return;
}

////////////////////////////////////////// SETUP //////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  
  connectToWifi();
  pinMode(0, OUTPUT);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(4), interruptAccepted, FALLING);
  attachInterrupt(digitalPinToInterrupt(5), interruptRejected, FALLING);
  tft.initR(INITR_BLACKTAB); 
  tft.setRotation(0); // set display orientation
}

////////////////////////////////////////// INTERRUPTIONS //////////////////////////////////////////

IRAM_ATTR void interruptAccepted(){
  flagAccepted = 1;
}

IRAM_ATTR void interruptRejected(){
  flagRejected = 1;
}

/////////////////////////////////////////////// LOOP ///////////////////////////////////////////////

void loop(){
  if (flagCall == 1){
    if (flagAccepted == 1){
      if (flag == 0){
        accepted();
      }
      if(millis() - timer >= 2000){
        flagAccepted = 0;
        flagCall = 0;
        flag = 0;
      }
    }
    if (flagRejected == 1){
      if (flag == 0){
        rejected();
      }
      if(millis() - timer >= 2000){
        flagRejected = 0;
        flagCall = 0;
        flag = 0;
      }
    }
    if(millis() - dally >= 15000 && flagAccepted == 0 && flagRejected == 0){
      flagCall = 0;
    }
  }
  else{
    digitalWrite(0, LOW);
  }
  if (flagMessage == 1){
    JSON();
    dally = millis();
    call();
  }
}


void JSON (){
  DeserializationError error = deserializeJson(data, json);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  bed = data["bed"];
  room = data["room"];
  pacient = data["pacient"];
  diagnosis = data["diagnosis"];
  
  flagMessage = 0;
}