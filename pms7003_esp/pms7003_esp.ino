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

static bool mqtt_send_string(const char *topic, const char *string)
{
    bool result = false;
    if (!mqttClient.connected()) {
        mqttClient.setServer(MQTT_HOST, MQTT_PORT);
        mqttClient.connect(device_name);
    }
    if (mqttClient.connected()) {
        Serial.print("Publishing ");
        Serial.print(string);
        Serial.print(" to ");
        Serial.print(topic);
        Serial.print("...");
        result = mqttClient.publish(topic, string, true);
        Serial.println(result ? "OK" : "FAIL");
    }
    return result;
}

static bool mqtt_send_value(const char *topic, int value)
{
    char string[16];
    snprintf(string, sizeof(string), "%d", value);
    return mqtt_send_string(topic, string);
}

static bool mqtt_send_json(const char *topic, const pms_meas_t *m)
{
    static char json[128];
    char tmp[128];
    
    // header
    strcpy(json, "{");
    
    // CF1, "standard particle"
    sprintf(tmp, "\"cf1\":{\"pm1_0\":%u,\"pm2_5\":%u,\"pm10\":%u},",
            m->concPM1_0_CF1, m->concPM2_5_CF1, m->concPM10_0_CF1);
    strcat(json, tmp);

    // AMB, "standard atmosphere"
    sprintf(tmp, "\"amb\":{\"pm1_0\":%u,\"pm2_5\":%u,\"pm10\":%u}",
            m->concPM1_0_amb, m->concPM2_5_amb, m->concPM10_0_amb);
    strcat(json, tmp);

#if 0 // currently this makes the message too big, PubSubClient allows maximum 128 bytes (including internal header)
    // raw particle counts
    sprintf(tmp, "\"raw\":{\"gt0_3\":%u,\"gt0_5\":%u,\"gt1_0\":%u,\"gt2_5\":%u,\"gt5_0\":%u,\"gt10_0\":%u},",
            m->rawGt0_3um, m->rawGt0_5um, m->rawGt1_0um, m->rawGt2_5um, m->rawGt5_0um, m->rawGt10_0um);
    strcat(json, tmp);

    // version
    sprintf(tmp, "\"ver\":%u,", m->version);
    strcat(json, tmp);
    
    // error code
    sprintf(tmp, "\"err\":%u", m->errorCode);
    strcat(json, tmp);
#endif
    
    // footer
    strcat(json, "}");

    return mqtt_send_string(topic, json);
}

void loop(void)
{
    unsigned long ms = millis();
    
    // check measurement interval
    if ((ms - last_sent) > MEASURE_INTERVAL_MS) {
        // publish it
        mqtt_send_json(MQTT_TOPIC "/json", &meas);
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

