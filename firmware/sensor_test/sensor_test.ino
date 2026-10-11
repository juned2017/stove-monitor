// Stove monitor - sensor test
//
// First program to run on the real ESP32. No Wi-Fi, no Mosquitto.
// It just checks the heat sensor is wired correctly and prints what it sees
// to the Serial Monitor once a second.
//
// Expected output:
//   Room 23.4 C   |   Pointed at 24.1 C
// Hold it over a hot mug and the "Pointed at" number should climb.

#include <Wire.h>
#include <Adafruit_MLX90614.h>

// Which ESP32 pins the sensor's two data wires go to.
// 21 and 22 are right for the common "ESP32 Dev Module" boards.
const int I2C_SDA = 21;
const int I2C_SCL = 22;

Adafruit_MLX90614 mlx;

// If the sensor isn't found, list everything that IS connected, to help
// spot a wiring mistake.
void scanForDevices() {
  Serial.println("Scanning for connected sensors...");
  int found = 0;
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Found a device at address 0x%02X\n", address);
      found++;
    }
  }
  if (found == 0) {
    Serial.println("  Nothing found. Check the 4 wires: VIN->3V3, GND->GND, SDA->21, SCL->22.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // give the Serial Monitor a moment to connect
  Serial.println();
  Serial.println("Stove monitor: sensor test");

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!mlx.begin()) {
    Serial.println("Heat sensor NOT found (expected at address 0x5A).");
    scanForDevices();
    while (true) delay(1000);  // stop here until the wiring is fixed
  }
  Serial.println("Heat sensor found. Reading once a second...");
}

void loop() {
  double room = mlx.readAmbientTempC();     // temperature of the sensor itself
  double pointed = mlx.readObjectTempC();   // temperature of what it's aimed at

  if (isnan(room) || isnan(pointed)) {
    Serial.println("Read failed. A wire may be loose.");
  } else {
    Serial.printf("Room %5.1f C   |   Pointed at %6.1f C\n", room, pointed);
  }
  delay(1000);
}
