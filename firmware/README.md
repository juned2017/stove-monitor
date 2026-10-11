# Step 3: the real ESP32

Two programs, run in this order:

| Program | What it does | Needs Wi-Fi? |
|---|---|---|
| `sensor_test` | Checks the heat sensor is wired right, prints temperatures | No |
| `stove_sensor` | The real thing: sends readings to Mosquitto every 5 seconds | Yes |

---

## A. Wire the sensor (4 wires)

Unplug the ESP32 from USB while wiring.

| Heat sensor pin | ESP32 pin | What it is |
|---|---|---|
| VIN (or VCC) | **3V3** | Power |
| GND | **GND** | Ground |
| SDA | **G21** (or D21) | Data |
| SCL | **G22** (or D22) | Clock |

- Pin names are printed on the boards, sometimes tiny. A phone camera zoomed in helps.
- Use **3V3**, not 5V or VIN, on the ESP32 side.
- If your sensor has a small STEMMA QT / Qwiic plug, you can use that cable instead. The wire colors are: red = power, black = ground, blue = SDA, yellow = SCL.

## B. Set up the Arduino software (once)

1. Download **Arduino IDE 2** from arduino.cc/en/software and install it.
2. Add ESP32 support:
   - **Arduino IDE > Settings**, in "Additional boards manager URLs" paste:
     `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   - Left sidebar, **Boards Manager** icon, search **esp32**, install **"esp32 by Espressif Systems"**. It's a big download.
3. Install 3 libraries. Left sidebar, **Library Manager** icon, search and install each:
   - **Adafruit MLX90614** (say yes if it asks to install "Adafruit BusIO" too)
   - **PubSubClient** by Nick O'Leary
   - **ArduinoJson** by Benoit Blanchon

## C. Run the sensor test

1. Plug the ESP32 into the Mac with the USB **data** cable.
2. **File > Open**, pick `firmware/sensor_test/sensor_test.ino`.
3. Top of the window, the board dropdown: **Select other board and port**:
   - Board: **ESP32 Dev Module**
   - Port: the one that appeared when you plugged in, usually named like `/dev/cu.usbserial-...` or `/dev/cu.SLAB_USBtoUART` or `/dev/cu.wchusbserial...`
4. Click **Upload** (the right-arrow button). Wait for "Done uploading."
5. **Tools > Serial Monitor**, set the speed dropdown (bottom right) to **115200 baud**.

You should see a line every second:

```
Room  23.4 C   |   Pointed at   24.1 C
```

Now the real test: hold the sensor about **2 feet** above a pot of boiling water (roughly hood height) and watch "Pointed at". Write down:

- the number with nothing hot below it,
- the number over the boiling pot from 2 feet.

That tells us how to set the alert threshold, and whether the sensor sees well enough from the hood.

## D. Run the real program

1. Find your Mac's address on the home network. In Terminal:
   ```
   ipconfig getifaddr en0
   ```
   You'll get something like `192.168.1.23`.
2. **File > Open**, pick `firmware/stove_sensor/stove_sensor.ino`. A second tab named `secrets.h` opens with it.
3. In `secrets.h`, fill in your Wi-Fi name, Wi-Fi password, and the Mac address from step 1.
4. Make sure Mosquitto is running: `docker compose up -d` in the `stove_monitor` folder.
5. Upload, then open the Serial Monitor (115200). You should see:
   ```
   Wi-Fi connected. ESP32 address: 192.168.1.40
   Connecting to Mosquitto at 192.168.1.23:1883... connected.
   Sent {"device":"stove-1","celsius":24.1,"ambient":23.4,"ts":1790690000}
   ```
6. Confirm the readings reach your laptop. In Terminal:
   ```
   docker exec -it mosquitto mosquitto_sub -t "home/kitchen/stove/#" -v
   ```
   Same readings appear there every 5 seconds. Unplug the ESP32 and about 20 seconds later you'll see `status offline`.

Once this works, the ESP32 no longer needs the laptop's USB. Plug it into any USB phone charger and it starts sending by itself.

---

## If something goes wrong

| What you see | Likely fix |
|---|---|
| No port shows up when plugged in | The USB cable is charge-only. Try another cable. If still nothing, your board may need a USB driver: search for the chip name printed near its USB port ("CP2102" or "CH340") + "Mac driver". |
| Upload stuck on `Connecting......` | Hold the **BOOT** button on the ESP32 while it says Connecting, release when it starts writing. |
| Serial Monitor shows gibberish | Speed isn't 115200. |
| `Heat sensor NOT found` | Check the 4 wires. The test prints what it does find, which helps. SDA and SCL swapped is the most common mistake. |
| Wi-Fi never connects | ESP32 only works on **2.4 GHz** Wi-Fi. Double-check name and password (both case-sensitive). |
| `Connecting to Mosquitto ... failed` | Is Docker running? Is the Mac address right (it can change after a restart)? macOS may block incoming connections: **System Settings > Network > Firewall**, allow Docker, or turn the firewall off briefly to test. |
| Readings say `"ts":0` | The clock hasn't synced from the internet yet. Harmless, fixes itself. |

## Keep secrets out of GitHub

When this goes on GitHub for your portfolio, don't upload your real `secrets.h`. Add it to `.gitignore` and commit a copy named `secrets.example.h` with placeholder values instead.
