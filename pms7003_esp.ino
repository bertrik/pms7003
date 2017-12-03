#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "Arduino.h"

#include "pms7003.h"

#include "SoftwareSerial.h"

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>

#define PIN_RX  D1
#define PIN_TX  D2

#define MQTT_HOST   "mosquitto.space.revspace.nl"
#define MQTT_PORT   1883
#define MQTT_TOPIC  "revspace/sensors/dust/pms7003"

static SoftwareSerial sensor(PIN_RX, PIN_TX);
static WiFiClient wifiClient;
static WiFiManager wifiManager;
static PubSubClient mqttClient(wifiClient);

static char esp_id[16];
static uint8_t buf[32];

void setup(void)
{
    Serial.begin(115200);
    Serial.println("PMS7003 ESP reader\n");

    sprintf(esp_id, "%08X", ESP.getChipId());
    Serial.print("ESP ID: ");
    Serial.println(esp_id);

    sensor.begin(9600);

    Serial.println("Starting WIFI manager ...");
    wifiManager.autoConnect("ESP-PMS7003");

    PmsInit(buf, sizeof(buf));
}

static void mqtt_send(const char *topic, int value, const char *unit)
{
    if (!mqttClient.connected()) {
        mqttClient.setServer(MQTT_HOST, MQTT_PORT);
        mqttClient.connect(esp_id);
    }
    if (mqttClient.connected()) {
        char string[64];
        snprintf(string, sizeof(string), "%d %s", value, unit);
        Serial.print("Publishing ");
        Serial.print(string);
        Serial.print(" to ");
        Serial.print(topic);
        Serial.print("...");
        int result = mqttClient.publish(topic, string, true);
        Serial.println(result ? "OK" : "FAIL");
    }
}

void loop(void)
{
    while (sensor.available()) {
        uint8_t c = sensor.read();
        if (PmsProcess(c)) {
            // got a new frame
            // TODO parse it, etc.
        }
    }
}

