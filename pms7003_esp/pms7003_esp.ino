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
#define PIN_RST D3
#define PIN_SET D4

#define MEASURE_INTERVAL_MS 10000

#define MQTT_HOST   "aliensdetected.com"
#define MQTT_PORT   1883
#define MQTT_TOPIC  "bertrik/pms7003"

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
    sprintf(device_name, "PMS7003-%06X", ESP.getChipId());
    Serial.print("Device name: ");
    Serial.println(device_name);

    // connect to wifi or set up captive portal
    Serial.println("Starting WIFI manager ...");
    wifiManager.autoConnect(device_name);

    // initialize the sensor
    sensor.begin(9600);
    txlen = PmsCreateCmd(txbuf, sizeof(txbuf), PMS_CMD_ON_STANDBY, 1);
    sensor.write(txbuf, txlen);
    txlen = PmsCreateCmd(txbuf, sizeof(txbuf), PMS_CMD_AUTO_MANUAL, 1);
    sensor.write(txbuf, txlen);
    PmsInit();
    
    pinMode(PIN_RST, INPUT_PULLUP);
    pinMode(PIN_SET, INPUT_PULLUP);
    
    Serial.println("setup() done");
}

static void mqtt_send(const char *topic, int value)
{
    if (!mqttClient.connected()) {
        mqttClient.setServer(MQTT_HOST, MQTT_PORT);
        mqttClient.connect(device_name);
    }
    if (mqttClient.connected()) {
        char string[64];
        snprintf(string, sizeof(string), "%d", value);
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
        // publish it
        mqtt_send(MQTT_TOPIC "/PM1_0", meas.concPM1_0_amb);
        mqtt_send(MQTT_TOPIC "/PM2_5", meas.concPM2_5_amb);
        mqtt_send(MQTT_TOPIC "/PM10",  meas.concPM10_0_amb);
        last_sent = ms;
    }

    // check for incoming measurement data
    while (sensor.available()) {
        uint8_t c = sensor.read();
        if (PmsProcess(c)) {
            // parse it
            PmsParse(&meas);
        }
    }
}

