// Copy this file to secrets.h and fill in your own values.
// secrets.h is ignored by git, so your Wi-Fi password never ends up on GitHub.

#pragma once

// Your home Wi-Fi. The ESP32 only works on 2.4 GHz Wi-Fi, not 5 GHz.
#define WIFI_SSID     "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"

// The address of the computer running Mosquitto, on your home network.
// On a Mac, find it with:  ipconfig getifaddr en0
#define MQTT_HOST "192.168.1.50"
#define MQTT_PORT 1883

// A name for this sensor.
#define DEVICE_ID "stove-1"
