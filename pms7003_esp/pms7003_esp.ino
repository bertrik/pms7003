#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "Arduino.h"

#include "pms7003.h"

#include "SoftwareSerial.h"

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>

#include <Wire.h>
#include <BME280I2C.h>

// PMS7003 pins
#define PIN_RX  D7
#define PIN_TX  D8
#define PIN_RST D3
#define PIN_SET D4

// BME280 pins
#define PIN_SDA D6
#define PIN_SCL D5

#define MEASURE_INTERVAL_MS 10000

#define MQTT_HOST   "aliensdetected.com"
#define MQTT_PORT   1883
#define MQTT_TOPIC  "bertrik/pms7003"

static SoftwareSerial sensor(PIN_RX, PIN_TX);
static WiFiClient wifiClient;
static WiFiManager wifiManager;
static PubSubClient mqttClient(wifiClient);

static char device_name[20];

typedef struct {
    float temp;
    float hum;
    float pres;
} bme_meas_t;

static BME280I2C bme280;

void setup(void)
{
    uint8_t txbuf[8];
    int txlen;

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

    Wire.begin(PIN_SDA, PIN_SCL);
    
    while(!bme280.begin())
    {
        Serial.println("Could not find BME280 sensor!");
        delay(1000);
    }
    
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
        result = mqttClient.publish(topic, string);
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

static bool mqtt_send_json(const char *topic, const pms_meas_t *pms, const bme_meas_t *bme)
{
    static char json[128];
    char tmp[128];
    
    // header
    strcpy(json, "{");
    
    // AMB, "standard atmosphere" particle
    sprintf(tmp, "\"pms7003\":{\"pm1_0\":%u,\"pm2_5\":%u,\"pm10\":%u},",
            pms->concPM1_0_amb, pms->concPM2_5_amb, pms->concPM10_0_amb);
    strcat(json, tmp);

    // BME280, other meteorological data
    sprintf(tmp, "\"bme280\":{\"t\":%.1f,\"rh\":%.1f,\"p\":%.1f}",
            bme->temp, bme->hum, bme->pres / 100.0);
    strcat(json, tmp);

    // footer
    strcat(json, "}");

    return mqtt_send_string(topic, json);
}


void loop(void)
{
    static pms_meas_t pms_meas;
    static unsigned long last_sent;
    bme_meas_t bme_meas;

    unsigned long ms = millis();
    
    // check measurement interval
    if ((ms - last_sent) > MEASURE_INTERVAL_MS) {
        // read BME sensor
        bme280.read(bme_meas.pres, bme_meas.temp, bme_meas.hum);

        // publish it
        mqtt_send_json(MQTT_TOPIC "/json", &pms_meas, &bme_meas);
        last_sent = ms;
    }

    // check for incoming measurement data
    while (sensor.available()) {
        uint8_t c = sensor.read();
        if (PmsProcess(c)) {
            // parse it
            PmsParse(&pms_meas);
        }
    }
}

