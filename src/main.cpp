////////////////////////////////////////// DECLARATIONS //////////////////////////////////////////

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Adafruit_I2CDevice.h>
#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <ArduinoJson.h>
#include <string.h>
#include "LittleFS.h"

#define WIFI_SSID "Kituuu"
#define WIFI_PASSWORD "41819096"
#define MQTT_HOST IPAddress(192, 168, 101, 5)
#define MQTT_PORT 1883
#define MSG (5)
#define TFT_CS 2
#define TFT_RST 14
#define TFT_DC 12
#define TFT_MOSI 13 // Data out
#define TFT_SCLK 15 // Clock out
#define ondisplay 0

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

unsigned long timer, dally, debounce;

bool flag = 0;
bool flagCall = 0;
volatile bool flagAccepted = 0;
volatile bool flagRejected = 0;
bool flagReminder = 0;
bool flagTerminated = 0;
bool flagMessage = 0;

const char *const TIGID = "02";
char bedArray[] = "00";
const char *bed;
const char *ID;
const char *room;
const char *patient;
const char *diagnosis;

void call();
void escribir(byte x_pos, byte y_pos, const char *text, byte text_size, uint16_t color);
void accepted();
void rejected();
void reminder();
void finish();
IRAM_ATTR void interruptAccepted();
IRAM_ATTR void interruptRejected();

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

////////////////////////////////////////// MQTT //////////////////////////////////////////

void connectToWifi()
{
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onWifiConnect(const WiFiEventStationModeGotIP &event)
{
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected &event)
{
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void onMqttConnect(bool sessionPresent)
{
  Serial.println("Connected to MQTT.");
  mqttClient.subscribe("SIGR/TIG", 2);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected())
  {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
  Serial.println("Subscribe acknowledged.");
}

void onMqttUnsubscribe(uint16_t packetId)
{
  Serial.println("Unsubscribe acknowledged.");
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  char idtig[3] = "02";
  Serial.println("Publish received.");
  Serial.println(topic);
  Serial.println(payload);
  if (payload[7] == idtig[0] && payload[8] == idtig[1])
  {
    int clean = strcspn(payload,"}");
    char content[100]; 
    strncpy(content, payload, clean + 1);
    Serial.println(content);
    File file = LittleFS.open("messsge.txt", "w");
    if (!file)
    {
      Serial.println("file open failed");
    }
    file.println(content);
    Serial.println(content);
    file.close();
  }
  File file = LittleFS.open("messsge.txt", "r");
    if (!file)
    {
      Serial.println("file open failed");
    }
    String line = file.readStringUntil('\n');
    Serial.println("Abriste el archivo y hay esto...");
    Serial.println(line);
    file.close();
  // DynamicJsonDocument data(200);
  // char content[len] = "";

  // Serial.println("########## COINCIDE ID ##########");
  // strcat(content, payload);
  // DeserializationError error = deserializeJson(data, content);
  // if (error)
  // {
  //   Serial.print(F("deserializeJson() failed: "));
  //   Serial.println(error.f_str());
  //   return;
  // }

  // strcpy(bedArray, data["bed"]);
  // bed = data["bed"];
  // room = data["room"];
  // patient = data["patient"];
  // diagnosis = data["diagnosis"];
  // flagMessage = 1;

  // Serial.println(bed);
  // data.clear();
  // Serial.println(bed);
}

void onMqttPublish(uint16_t packetId)
{
  Serial.println("Publish acknowledged.");
}

////////////////////////////////////////// DISPLAY //////////////////////////////////////////

void call()
{
  tft.fillRect(0, 32, 128, 26, RED);
  tft.fillRect(0, 58, 128, 54, BLACK);
  tft.fillRect(0, 112, 128, 33, BLUE);
  tft.fillRect(0, 143, 128, 2, BLACK);
  tft.fillRect(63, 60, 2, 50, WHITE);
  tft.fillRect(0, 145, 64, 19, GREEN);
  tft.fillRect(64, 145, 64, 19, RED);
  escribir(25, 37, "C A L L", 2, WHITE);
  escribir(19, 62, "ROOM", 1, WHITE);
  escribir(91, 62, "BED", 1, WHITE);
  escribir(8, 76, room, 4, CYAN);
  escribir(74, 76, bed, 4, CYAN);
  escribir(3, 117, patient, 1, WHITE);
  escribir(3, 131, diagnosis, 1, WHITE);
  escribir(16, 149, "Acept", 1, BLACK);
  escribir(77, 149, "Decline", 1, BLACK);
  digitalWrite(ondisplay, HIGH);
  flagCall = 1;
  flagMessage = 0;
  attachInterrupt(digitalPinToInterrupt(4), interruptAccepted, FALLING);
  attachInterrupt(digitalPinToInterrupt(5), interruptRejected, FALLING);
}

void accepted()
{
  DynamicJsonDocument tig(200);
  String tigJSON = "";
  tft.fillScreen(GREEN);
  escribir(15, 89, "ACCEPTED", 2, BLACK);
  escribir(16, 88, "ACCEPTED", 2, WHITE);
  digitalWrite(ondisplay, HIGH);
  Serial.println("Accepted");
  tig["TIGID"] = TIGID;
  tig["answer"] = "A";
  tig["bed"] = bed;
  serializeJson(tig, tigJSON);
  const char *tigmsg = tigJSON.c_str();
  Serial.println(tigmsg);
  mqttClient.publish("SIGR/TIGAnswer", 2, true, tigmsg);
  timer = millis();
  flag = 1;
  tig.clear();
}

void rejected()
{
  DynamicJsonDocument tig(200);
  String tigJSON = "";
  tft.fillScreen(RED);
  escribir(15, 89, "REJECTED", 2, WHITE);
  escribir(16, 88, "REJECTED", 2, BLACK);
  digitalWrite(ondisplay, HIGH);
  Serial.println("Rejected");
  tig["TIGID"] = TIGID;
  tig["answer"] = "D";
  tig["bed"] = bed;
  serializeJson(tig, tigJSON);
  const char *tigmsg = tigJSON.c_str();
  mqttClient.publish("SIGR/TIGAnswer", 2, true, tigmsg);
  timer = millis();
  flag = 1;
  tig.clear();
}

void escribir(byte x_pos, byte y_pos, const char *text, byte text_size, uint16_t color)
{
  tft.setCursor(x_pos, y_pos);
  tft.setTextSize(text_size);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
  return;
}

void reminder()
{
  tft.fillRect(0, 32, 128, 19, CYAN);
  tft.fillRect(0, 51, 128, 109, BLACK);
  tft.fillRect(0, 110, 128, 33, BLUE);
  tft.fillRect(0, 145, 128, 15, CYAN);
  escribir(22, 38, "ROOM", 1, BLACK);
  escribir(87, 38, "BED", 1, BLACK);
  escribir(9, 67, room, 4, WHITE);
  escribir(8, 66, room, 4, BLUE);
  escribir(75, 67, bed, 4, WHITE);
  escribir(74, 66, bed, 4, BLUE);
  escribir(3, 115, patient, 1, WHITE);
  escribir(3, 130, diagnosis, 1, WHITE);
  if (flagReminder == 1)
  {
    escribir(48, 149, "Finish", 1, BLACK);
  }
  digitalWrite(ondisplay, HIGH);
  flagReminder = 1;
  flag = 1;
  timer = millis();
  attachInterrupt(digitalPinToInterrupt(4), interruptAccepted, FALLING);
  attachInterrupt(digitalPinToInterrupt(5), interruptRejected, FALLING);
}

void finish()
{
  DynamicJsonDocument tig(200);
  // StaticJsonDocument<200> tig;
  String tigJSON = "";
  tft.fillScreen(BLACK);
  escribir(30, 89, "FINISH", 2, WHITE);
  escribir(31, 88, "FINISH", 2, GREEN);
  digitalWrite(ondisplay, HIGH);
  Serial.println("FINISH");
  tig["TIGID"] = TIGID;
  tig["answer"] = "F";
  tig["bed"] = bed;
  serializeJson(tig, tigJSON);
  const char *tigmsg = tigJSON.c_str();
  Serial.println(tigmsg);
  mqttClient.publish("SIGR/TIGAnswer", 2, true, tigmsg);
  timer = millis();
  flagTerminated = 1;
  tig.clear();
}
////////////////////////////////////////// SETUP //////////////////////////////////////////

void setup()
{
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
  pinMode(ondisplay, OUTPUT);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0); // set display orientation
  bool ok = LittleFS.begin();
  if (!ok)
  {
    Serial.println("Error al iniciar LittleFS");
  }
}

////////////////////////////////////////// INTERRUPTIONS //////////////////////////////////////////

IRAM_ATTR void interruptAccepted()
{
  if (millis() - debounce > 150)
  {
    flagAccepted = 1;
    debounce = millis();
  }
}

IRAM_ATTR void interruptRejected()
{
  if (millis() - debounce > 150)
  {
    flagRejected = 1;
    debounce = millis();
  }
}

/////////////////////////////////////////////// LOOP ///////////////////////////////////////////////

void loop()
{
  if (flagMessage == 1)
  {
    dally = millis();
    call();
  }

  if (flagCall == 1)
  {
    if (flagAccepted == 1)
    {
      if (flag == 0)
      {
        detachInterrupt(4);
        detachInterrupt(5);
        Serial.println("Estamos en por llamar a acepted");
        Serial.println(bed);
        accepted();
      }
      if (flag == 1 && millis() - timer >= 2000)
      {
        flagAccepted = 0;
        flagCall = 0;
        flag = 0;
        Serial.println("Estamos en acepted por llamar a reminder");
        Serial.println(bed);
        reminder();
      }
    }

    if (flagRejected == 1)
    {
      if (flag == 0)
      {
        detachInterrupt(4);
        detachInterrupt(5);
        rejected();
      }
      if (millis() - timer >= 2000)
      {
        flagRejected = 0;
        flagCall = 0;
        flag = 0;
        digitalWrite(ondisplay, LOW);
      }
    }

    if (millis() - dally >= 10000)
    {
      DynamicJsonDocument tig(200);
      String tigJSON = "";
      digitalWrite(ondisplay, LOW);
      tig["TIGID"] = TIGID;
      tig["answer"] = "T";
      tig["bed"] = bed;
      serializeJson(tig, tigJSON);
      const char *tigmsg = tigJSON.c_str();
      Serial.println(tigmsg);
      mqttClient.publish("SIGR/TIGAnswer", 2, true, tigmsg);
      flagCall = 0;
      tig.clear();
    }
  }

  if (flagReminder == 1)
  {
    if (flag == 1)
    {
      digitalWrite(ondisplay, HIGH);
      if (flagAccepted == 1 || flagRejected == 1)
      {
        detachInterrupt(4);
        detachInterrupt(5);
        flagReminder = 0;
        flag = 0;
        flagAccepted = 0;
        flagRejected = 0;
        Serial.println("Estamos en reminder");
        Serial.println(bed);
        finish();
      }

      if (millis() - timer >= 5000 && flag == 1)
      {
        digitalWrite(ondisplay, LOW);
        Serial.println("Estamos en reminder por apagar la pantalla");
        Serial.println(bed);
        flag = 0;
      }
    }

    if (flagAccepted == 1 || flagRejected == 1)
    {
      flag = 1;
      flagAccepted = 0;
      flagRejected = 0;
      timer = millis();
    }
  }

  if (flagTerminated == 1)
  {
    if (millis() - timer >= 2000)
    {
      digitalWrite(ondisplay, LOW);
      flagTerminated = 0;
    }
  }
}
