"""
Fake ESP32 for the stove monitor.

Pretends to be the real ESP32 + heat sensor: every few seconds it publishes
a stove temperature reading to Mosquitto, using the exact same topics and
message format the real board will use. Lets you build and test the alert
service before any hardware arrives.

Scenarios:
  idle     Stove off. Room temperature, forever.
  cook     Normal cooking: heats up, stays hot for a while, then cools down.
  forgot   Stove turned on and never turned off (should trigger an alert).
  offline  Sends a few readings, then "loses power" without saying goodbye
           (should trigger a "sensor offline" alert).

Usage:
  python fake_esp32.py <scenario>

Settings (environment variables):
  MQTT_HOST       Broker address (default: localhost)
  MQTT_PORT       Broker port (default: 1883)
  DEVICE_ID       Name of this fake device (default: stove-1)
  INTERVAL_SEC    Seconds between readings (default: 5)
  HOT_MINUTES     How long "cook" stays hot (default: 2)
"""

import json
import os
import random
import sys
import time

import paho.mqtt.client as mqtt

# ---- Message contract (the real ESP32 firmware will use the same) ----------
# home/kitchen/stove/<device>/reading  {"device":..,"celsius":..,"ts":..}
# home/kitchen/stove/<device>/status   "online" | "offline"   (retained)
DEVICE_ID = os.getenv("DEVICE_ID", "stove-1")
BASE_TOPIC = f"home/kitchen/stove/{DEVICE_ID}"
READING_TOPIC = f"{BASE_TOPIC}/reading"
STATUS_TOPIC = f"{BASE_TOPIC}/status"

MQTT_HOST = os.getenv("MQTT_HOST", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
INTERVAL_SEC = float(os.getenv("INTERVAL_SEC", "5"))
HOT_MINUTES = float(os.getenv("HOT_MINUTES", "2"))

ROOM_C = 22.0
COOKING_C = 180.0
HEAT_UP_SEC = 60.0
COOL_DOWN_SEC = 120.0


def temperature_for(scenario: str, elapsed: float) -> float:
    """Pretend sensor: what the heat sensor would read `elapsed` seconds in."""
    if scenario == "idle":
        target = ROOM_C
    else:
        hot_until = HEAT_UP_SEC + HOT_MINUTES * 60
        if elapsed < HEAT_UP_SEC:
            # Heating up
            target = ROOM_C + (COOKING_C - ROOM_C) * (elapsed / HEAT_UP_SEC)
        elif scenario != "cook" or elapsed < hot_until:
            # Hot ("forgot" and "offline" stay hot)
            target = COOKING_C
        else:
            # Cooling down (cook only)
            cooled = min((elapsed - hot_until) / COOL_DOWN_SEC, 1.0)
            target = COOKING_C - (COOKING_C - ROOM_C) * cooled
    # Real sensors wobble a little
    return round(target + random.uniform(-1.5, 1.5), 1)


def main() -> None:
    scenarios = ("idle", "cook", "forgot", "offline")
    if len(sys.argv) != 2 or sys.argv[1] not in scenarios:
        print(f"Usage: python fake_esp32.py <{'|'.join(scenarios)}>")
        sys.exit(1)
    scenario = sys.argv[1]

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"fake-{DEVICE_ID}")
    # "Last will": if we vanish without a clean goodbye, the broker announces
    # "offline" for us. The real ESP32 does the same thing.
    client.will_set(STATUS_TOPIC, "offline", qos=1, retain=True)
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=15)
    client.loop_start()
    client.publish(STATUS_TOPIC, "online", qos=1, retain=True)

    print(f"Fake ESP32 '{DEVICE_ID}' connected to {MQTT_HOST}:{MQTT_PORT}")
    print(f"Scenario: {scenario}. Publishing to {READING_TOPIC} every {INTERVAL_SEC:g}s. Ctrl+C to stop.")

    start = time.monotonic()
    sent = 0
    try:
        while True:
            elapsed = time.monotonic() - start
            celsius = temperature_for(scenario, elapsed)
            message = {"device": DEVICE_ID, "celsius": celsius, "ts": int(time.time())}
            client.publish(READING_TOPIC, json.dumps(message), qos=1)
            sent += 1
            print(f"[{int(elapsed):>5}s] {celsius:6.1f} °C")

            if scenario == "offline" and sent >= 6:
                print("Simulating power loss (no goodbye). The broker will mark this device offline.")
                time.sleep(0.5)  # let the last reading go out
                os._exit(0)  # die abruptly so the "last will" fires

            time.sleep(INTERVAL_SEC)
    except KeyboardInterrupt:
        # Clean shutdown: say we're going offline ourselves.
        client.publish(STATUS_TOPIC, "offline", qos=1, retain=True).wait_for_publish(2)
        client.loop_stop()
        client.disconnect()
        print("\nStopped.")


if __name__ == "__main__":
    main()
