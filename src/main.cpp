////////////////////////////////////////// DECLARATIONS //////////////////////////////////////////

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_I2CDevice.h>
#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <ArduinoJson.h>
#include <string.h>
#include "LittleFS.h"

#define WIFI_SSID "Kituuu"
#define WIFI_PASSWORD "41819096"
#define MQTT_HOST IPAddress(192, 168, 101, 6)
#define MQTT_PORT 1883
#define TFT_CS 2
#define TFT_RST 14
#define TFT_DC 12
#define TFT_MOSI 13 // Data out
#define TFT_SCLK 15 // Clock out
#define ondisplay 0

/////////////////////////////////////// DEFINITION OF COLORS ///////////////////////////////////////

#define BLACK 0x0000
#define RED 0x001F
#define BLUE 0xF800
#define GREEN 0x07E0
#define YELLOW 0x07FF
#define MAGENTA 0xF81F
#define CYAN 0xFFE0
#define WHITE 0xFFFF

/////////////////////////////////////// DEFINITION OF OBJETS ///////////////////////////////////////

AsyncMqttClient mqttClient;
WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker mqttReconnectTimer;
Ticker wifiReconnectTimer;
Ticker TickerTimeOut;
Ticker TickerOFFDisplay;
Ticker TickerReminder;

//////////////////////////////////////// DEFINITION OF FLAGS ///////////////////////////////////////

volatile bool flagAccepted = 0;
volatile bool flagRejected = 0;
bool flagReminder = 0;
bool flagMessage = 0;
bool flag = 0;
bool flagCall = 0;

////////////////////////////////// DEFINITION OF GLOBAL VARIABLES //////////////////////////////////

const char *const TIGID = "02"; // TIG ID
char bed[] = "00";

/////////////////////////////////// DEFINITION OF GLOBAL FUNTIONS //////////////////////////////////

void call();
void escribir(byte, byte, const char *, byte, uint16_t);
void accepted();
void rejected();
void reminder();
void finish();
void timeOut();
void OFFDisplay();
void publish(const char *answer);
IRAM_ATTR void interruptAccepted();
IRAM_ATTR void interruptRejected();

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

/////////////////////////////////////////////// SETUP ///////////////////////////////////////////////

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
    Serial.println("Error starting LittleFS");
  }
}

/////////////////////////////////////////////// MQTT ///////////////////////////////////////////////

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
  char idtig[3] = "02"; // TIG ID
  Serial.println("Publish received.");
  Serial.println(topic);
  Serial.println(payload);
  if (payload[7] == idtig[0] && payload[8] == idtig[1]) // IF ID MATCHES
  {
    int clean = strcspn(payload, "}");
    char content[len];
    strncpy(content, payload, clean + 1);
    File file = LittleFS.open("messsge.txt", "w");
    if (!file)
    {
      Serial.println("file open failed");
    }
    file.println(content);
    file.close();
    flagMessage = 1;
  }
}

void onMqttPublish(uint16_t packetId)
{
  Serial.println("Publish acknowledged.");
}

////////////////////////////////////////////// DISPLAY //////////////////////////////////////////////

void call()
{
  File file = LittleFS.open("messsge.txt", "r");

  String line = file.readStringUntil('\n');
  StaticJsonDocument<200> data;

  DeserializationError error = deserializeJson(data, line);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  strcpy(bed, data["bed"]);
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
  escribir(8, 76, data["room"], 4, CYAN);
  escribir(74, 76, bed, 4, CYAN);
  char patient[21] = "";
  strncpy(patient, data["patient"], 20);
  escribir(3, 117, patient, 1, WHITE);
  char diagnosis[21] = "";
  strncpy(diagnosis, data["diagnosis"], 20);
  escribir(3, 131, diagnosis, 1, WHITE);
  escribir(16, 149, "Acept", 1, BLACK);
  escribir(77, 149, "Decline", 1, BLACK);
  digitalWrite(ondisplay, HIGH);
  flagMessage = 0;
  flagCall = 1;
  attachInterrupt(digitalPinToInterrupt(4), interruptAccepted, FALLING);
  attachInterrupt(digitalPinToInterrupt(5), interruptRejected, FALLING);
  file.close();
  data.clear();
  TickerTimeOut.attach(10, timeOut);
}

void accepted()
{
  TickerTimeOut.detach();
  tft.fillScreen(GREEN);
  escribir(15, 89, "ACCEPTED", 2, BLACK);
  escribir(16, 88, "ACCEPTED", 2, WHITE);
  digitalWrite(ondisplay, HIGH);
  publish("A");
  reminder();
}

void rejected()
{
  TickerTimeOut.detach();
  tft.fillScreen(RED);
  escribir(15, 89, "REJECTED", 2, WHITE);
  escribir(16, 88, "REJECTED", 2, BLACK);
  digitalWrite(ondisplay, HIGH);
  publish("D");
  TickerOFFDisplay.attach(3, OFFDisplay);
}

void reminder()
{
  File file = LittleFS.open("messsge.txt", "r");

  String line = file.readStringUntil('\n');
  StaticJsonDocument<200> data;

  DeserializationError error = deserializeJson(data, line);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  tft.fillRect(0, 32, 128, 19, CYAN);
  tft.fillRect(0, 51, 128, 109, BLACK);
  tft.fillRect(0, 110, 128, 33, BLUE);
  tft.fillRect(0, 145, 128, 15, CYAN);
  escribir(22, 38, "ROOM", 1, BLACK);
  escribir(87, 38, "BED", 1, BLACK);
  escribir(9, 67, data["room"], 4, WHITE);
  escribir(8, 66, data["room"], 4, BLUE);
  escribir(75, 67, bed, 4, WHITE);
  escribir(74, 66, bed, 4, BLUE);
  char patient[21] = "";
  strncpy(patient, data["patient"], 20);
  escribir(3, 117, patient, 1, WHITE);
  char diagnosis[21] = "";
  strncpy(diagnosis, data["diagnosis"], 20);
  escribir(3, 131, diagnosis, 1, WHITE);
  digitalWrite(ondisplay, HIGH);
  flagReminder = 1;

  attachInterrupt(digitalPinToInterrupt(4), interruptAccepted, FALLING);
  TickerOFFDisplay.attach(3, OFFDisplay);
  file.close();
  data.clear();
}

void finish()
{
  tft.fillScreen(BLACK);
  escribir(30, 89, "FINISH", 2, WHITE);
  escribir(31, 88, "FINISH", 2, GREEN);
  digitalWrite(ondisplay, HIGH);
  publish("F");
  TickerOFFDisplay.attach(3, OFFDisplay);
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

//////////////////////////////////////// FUNCTION TO PUBLISH ////////////////////////////////////////

void publish(const char *answer)
{
  StaticJsonDocument<50> tig;
  String tigJSON = "";
  tig["TIGID"] = TIGID;
  tig["answer"] = answer;
  tig["bed"] = bed;
  serializeJson(tig, tigJSON);
  const char *tigmsg = tigJSON.c_str();
  Serial.println(tigmsg);
  mqttClient.publish("SIGR/TIGAnswer", 2, true, tigmsg);
  tig.clear();
}

///////////////////////////////////////// FUNCTIONS TICKER //////////////////////////////////////////

void timeOut()
{
  digitalWrite(ondisplay, LOW);
  publish("T");
  TickerTimeOut.detach();
}

void OFFDisplay()
{
  digitalWrite(ondisplay, LOW);
  TickerOFFDisplay.detach();
}

void TReminder()
{
  digitalWrite(ondisplay, LOW);
  TickerReminder.detach();
  attachInterrupt(digitalPinToInterrupt(4), interruptAccepted, FALLING);
  detachInterrupt(5);
}

/////////////////////////////////////////// INTERRUPTIONS ///////////////////////////////////////////

IRAM_ATTR void interruptAccepted()
{
  flagAccepted = 1;
  detachInterrupt(4);
  detachInterrupt(5);
}

IRAM_ATTR void interruptRejected()
{
  flagRejected = 1;
  detachInterrupt(4);
  detachInterrupt(5);
}

/////////////////////////////////////////////// LOOP ////////////////////////////////////////////////

void loop()
{
  if (flagMessage == 1)
  {
    call();
  }

  if (flagCall == 1 && flagAccepted == 1)
  {
    flagAccepted = 0;
    flagCall = 0;
    accepted();
  }

  if (flagCall == 1 && flagRejected == 1)
  {
    flagRejected = 0;
    flagCall = 0;
    rejected();
  }

  if (flagReminder == 1 && flagAccepted == 1)
  {
    flagAccepted = 0;
    escribir(48, 149, "Finish", 1, BLACK);
    digitalWrite(ondisplay, HIGH);
    attachInterrupt(digitalPinToInterrupt(5), interruptRejected, FALLING);
    TickerReminder.attach(5, OFFDisplay);
  }

  if (flagReminder == 1 && flagRejected == 1)
  {
    flagRejected = 0;
    flagReminder = 0;
    finish();
  }
}
