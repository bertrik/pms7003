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

#define MEASURE_INTERVAL_MS 10000

#define MQTT_HOST   "mosquitto.space.revspace.nl"
#define MQTT_PORT   1883
#define MQTT_TOPIC  "revspace/sensors/dust/pms7003"

static SoftwareSerial sensor(PIN_RX, PIN_TX);
static WiFiClient wifiClient;
static WiFiManager wifiManager;
static PubSubClient mqttClient(wifiClient);

static char device_name[20];

static uint8_t txbuf[8];
static int txlen;

static pms_meas_t meas;
static unsigned long last_sent;

void setup(void)
{
    // welcome message
    Serial.begin(115200);
    Serial.println("PMS7003 ESP reader");

    // get ESP id
    sprintf(device_name, "PMS7003-%08X", ESP.getChipId());
    Serial.print("Device name: ");
    Serial.println(device_name);

    // connect to wifi or set up captive portal
    Serial.println("Starting WIFI manager ...");
    wifiManager.autoConnect(device_name);

    // initialize the sensor, put it in manual mode
    sensor.begin(9600);
    txlen = PmsCreateCmd(txbuf, sizeof(txbuf), PMS_CMD_ON_STANDBY, 1);
    sensor.write(txbuf, txlen);
    txlen = PmsCreateCmd(txbuf, sizeof(txbuf), PMS_CMD_AUTO_MANUAL, 0);
    sensor.write(txbuf, txlen);
    PmsInit();
}

static void mqtt_send(const char *topic, int value, const char *unit)
{
    if (!mqttClient.connected()) {
        mqttClient.setServer(MQTT_HOST, MQTT_PORT);
        mqttClient.connect(device_name);
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
    unsigned long ms = millis();
    
    // check measurement interval
    if ((ms - last_sent) > MEASURE_INTERVAL_MS) {
        txlen = PmsCreateCmd(txbuf, sizeof(txbuf), PMS_CMD_TRIG_MANUAL, 0);
        sensor.write(txbuf, txlen);
        last_sent = ms;
    }

    // check for incoming measurement data
    while (sensor.available()) {
        uint8_t c = sensor.read();
        if (PmsProcess(c)) {
            // parse it
            PmsParse(&meas);
            // publish it
            mqtt_send(MQTT_TOPIC "/0.3",  meas.rawGt0_3um, "ug/m3");
            mqtt_send(MQTT_TOPIC "/0.5",  meas.rawGt0_5um, "ug/m3");
            mqtt_send(MQTT_TOPIC "/1.0",  meas.rawGt1_0um, "ug/m3");
            mqtt_send(MQTT_TOPIC "/2.5",  meas.rawGt2_5um, "ug/m3");
            mqtt_send(MQTT_TOPIC "/5.0",  meas.rawGt5_0um, "ug/m3");
            mqtt_send(MQTT_TOPIC "/10.0", meas.rawGt10_0um,"ug/m3");
        }
    }
}

