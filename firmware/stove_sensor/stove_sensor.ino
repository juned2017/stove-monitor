// Stove monitor - ESP32 firmware
//
// Reads the MLX90614 heat sensor every few seconds and publishes the reading
// to Mosquitto over Wi-Fi, in exactly the same format as the fake ESP32:
//
//   home/kitchen/stove/<DEVICE_ID>/reading
//     {"device":"stove-1","celsius":180.2,"ambient":24.1,"ts":1790649513}
//
//   home/kitchen/stove/<DEVICE_ID>/status
//     "online" | "offline"   (retained)
//
// "celsius" is what the sensor is pointed at (the cooktop).
// "ambient" is the temperature of the sensor itself, roughly the room.
// "ts" is the time in seconds (0 if the clock hasn't synced yet).
//
// If the ESP32 loses power or Wi-Fi, Mosquitto publishes "offline" for it
// automatically (its "last will"), so the alert service can tell.
//
// Settings live in secrets.h.

#include <WiFi.h>
#include <Wire.h>
#include <time.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_MLX90614.h>
#include "secrets.h"

// ---- Settings you might tweak ---------------------------------------------
const int I2C_SDA = 21;                      // sensor data wire
const int I2C_SCL = 22;                      // sensor clock wire
const unsigned long READ_EVERY_MS = 5000;    // how often to send a reading
const unsigned long RETRY_EVERY_MS = 5000;   // how often to retry Mosquitto
const uint16_t MQTT_KEEPALIVE_SEC = 15;      // offline is detected ~1.5x this

// ---- Topics ----------------------------------------------------------------
const String BASE_TOPIC = String("home/kitchen/stove/") + DEVICE_ID;
const String READING_TOPIC = BASE_TOPIC + "/reading";
const String STATUS_TOPIC = BASE_TOPIC + "/status";

Adafruit_MLX90614 mlx;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long lastReadMs = 0;
unsigned long lastRetryMs = 0;
bool sensorOk = false;

void connectWifi() {
  Serial.printf("Connecting to Wi-Fi \"%s\"", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Wi-Fi connected. ESP32 address: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("Wi-Fi not connected yet. Check the name/password, and that it's 2.4 GHz. Will keep trying.");
  }
}

// Ask the internet for the current time, so readings get a timestamp.
void startClock() {
  configTime(0, 0, "pool.ntp.org", "time.google.com");
}

unsigned long nowSeconds() {
  time_t now = time(nullptr);
  return now > 1700000000 ? (unsigned long)now : 0;  // 0 = not synced yet
}

bool connectMqtt() {
  String clientId = String("esp32-") + DEVICE_ID;
  Serial.printf("Connecting to Mosquitto at %s:%d... ", MQTT_HOST, MQTT_PORT);

  // The last 3 arguments set the "last will": if we vanish, Mosquitto
  // publishes "offline" on our status topic for us (retained).
  bool ok = mqtt.connect(clientId.c_str(), STATUS_TOPIC.c_str(), 1, true, "offline");

  if (ok) {
    Serial.println("connected.");
    mqtt.publish(STATUS_TOPIC.c_str(), "online", true);
  } else {
    Serial.printf("failed (code %d). Is Docker running and the laptop address right?\n", mqtt.state());
  }
  return ok;
}

void publishReading() {
  double pointed = mlx.readObjectTempC();
  double room = mlx.readAmbientTempC();

  if (isnan(pointed) || isnan(room)) {
    // Don't send a bad number. If this keeps happening, readings stop and
    // the alert service will notice the silence.
    Serial.println("Sensor read failed. A wire may be loose.");
    return;
  }

  JsonDocument doc;
  doc["device"] = DEVICE_ID;
  doc["celsius"] = round(pointed * 10) / 10.0;
  doc["ambient"] = round(room * 10) / 10.0;
  doc["ts"] = nowSeconds();

  char payload[128];
  serializeJson(doc, payload, sizeof(payload));

  bool sent = mqtt.publish(READING_TOPIC.c_str(), payload);
  Serial.printf("%s %s\n", sent ? "Sent" : "NOT sent", payload);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.printf("Stove monitor: device %s\n", DEVICE_ID);

  Wire.begin(I2C_SDA, I2C_SCL);
  sensorOk = mlx.begin();
  if (!sensorOk) {
    Serial.println("Heat sensor NOT found. Run the sensor_test program to check the wiring.");
  }

  connectWifi();
  startClock();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(MQTT_KEEPALIVE_SEC);
}

void loop() {
  unsigned long now = millis();

  // Wi-Fi reconnects on its own; just wait for it.
  if (WiFi.status() != WL_CONNECTED) {
    delay(500);
    return;
  }

  // (Re)connect to Mosquitto, without hammering it.
  if (!mqtt.connected()) {
    if (now - lastRetryMs >= RETRY_EVERY_MS) {
      lastRetryMs = now;
      connectMqtt();
    }
    return;
  }
  mqtt.loop();  // keeps the connection alive

  if (sensorOk && now - lastReadMs >= READ_EVERY_MS) {
    lastReadMs = now;
    publishReading();
  }
}
