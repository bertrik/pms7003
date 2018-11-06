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
#define PIN_RX  D1
#define PIN_TX  D2

// BME280 pins
#define PIN_SDA D3
#define PIN_SCL D4

#define MEASURE_INTERVAL_MS 30000

#define MQTT_HOST   "aliensdetected.com"
#define MQTT_PORT   1883
#define MQTT_TOPIC  "bertrik/dust"

static SoftwareSerial sensor(PIN_RX, PIN_TX);
static WiFiClient wifiClient;
static WiFiManager wifiManager;
static PubSubClient mqttClient(wifiClient);

static char esp_id[16];
static char device_name[20];
static char mqtt_topic[32];
static boolean have_bme280;
static BME280I2C bme280;

typedef struct {
    float temp;
    float hum;
    float pres;
} bme_meas_t;

typedef struct {
    float pm10;
    float pm2_5;
    float pm1_0;
} pms_dust_t;

void setup(void)
{
    uint8_t txbuf[8];
    int txlen;

    // welcome message
    Serial.begin(115200);
    Serial.println("PMS7003 ESP reader");

    // get ESP id
    sprintf(esp_id, "%06X", ESP.getChipId());
    sprintf(device_name, "PMS7003-%s", esp_id);
    Serial.print("Device name: ");
    Serial.println(device_name);
    sprintf(mqtt_topic, "%s/%s", MQTT_TOPIC, esp_id);

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
    
    Wire.begin(PIN_SDA, PIN_SCL);
    
    have_bme280 = bme280.begin();
    if (have_bme280) {
        Serial.println("Found BME280 sensor.");
    }
    
    Serial.println("setup() done");
}

static void mqtt_send_string(const char *topic, const char *string)
{
    if (!mqttClient.connected()) {
        Serial.println("Connecting to MQTT server " MQTT_HOST);
        mqttClient.setServer(MQTT_HOST, MQTT_PORT);
        mqttClient.connect(device_name, topic, 0, true, "{\"alive\":0}");
    }
    if (mqttClient.connected()) {
        Serial.print("Publishing ");
        Serial.print(string);
        Serial.print(" to ");
        Serial.print(topic);
        Serial.print("...");
        bool result = mqttClient.publish(topic, string);
        Serial.println(result ? "OK" : "FAIL");
    }
}

static void mqtt_send_json(const char *topic, int alive, const pms_dust_t *pms, const bme_meas_t *bme)
{
    static char json[128];
    char tmp[128];
    
    // header
    strcpy(json, "{");

    // always send alive
    sprintf(tmp, "\"alive\":%d", alive);
    strcat(json, tmp);

    // PMS7003
    if (pms != NULL) {
        // AMB, "standard atmosphere" particle
        sprintf(tmp, ",\"pms7003\":{\"pm10\":%.1f,\"pm2_5\":%.1f,\"pm1_0\":%.1f}",
                pms->pm10, pms->pm2_5, pms->pm1_0);
        strcat(json, tmp);
    }

    // BME280, other meteorological data
    if (bme != NULL) {
        sprintf(tmp, ",\"bme280\":{\"t\":%.1f,\"rh\":%.1f,\"p\":%.1f}",
                bme->temp, bme->hum, bme->pres / 100.0);
        strcat(json, tmp);
    }

    // footer
    strcat(json, "}");

    mqtt_send_string(topic, json);
}

void loop(void)
{
    static pms_dust_t pms_meas_sum = {0.0, 0.0, 0.0};
    static int pms_meas_count = 0;
    static unsigned long last_sent = 0;
    static unsigned long alive_count = 0;

    // keep MQTT alive
    mqttClient.loop();

    // check measurement interval
    unsigned long ms = millis();
    if ((ms - last_sent) > MEASURE_INTERVAL_MS) {
        if (pms_meas_count > 0) {
            // average dust measurement
            pms_meas_sum.pm10 /= pms_meas_count;
            pms_meas_sum.pm2_5 /= pms_meas_count;
            pms_meas_sum.pm1_0 /= pms_meas_count;

            // read BME sensor
            bme_meas_t *bme280_p;
            if (have_bme280) {
                bme_meas_t bme_meas;
                bme280.read(bme_meas.pres, bme_meas.temp, bme_meas.hum);
                bme280_p = &bme_meas;
            } else {
                bme280_p = NULL;
            }

            // publish it
            alive_count++;
            mqtt_send_json(mqtt_topic, alive_count, &pms_meas_sum, bme280_p);

            // reset sum
            pms_meas_sum.pm10 = 0.0;
            pms_meas_sum.pm2_5 = 0.0;
            pms_meas_sum.pm1_0 = 0.0;
            pms_meas_count = 0;
        } else {
            Serial.println("Not publishing, no measurement received from PMS7003!");
            
            // publish only the alive counter
            mqtt_send_json(mqtt_topic, alive_count, NULL, NULL);
        }
        last_sent = ms;
    }

    // check for incoming measurement data
    while (sensor.available()) {
        uint8_t c = sensor.read();
        if (PmsProcess(c)) {
            // parse it
            pms_meas_t pms_meas;
            PmsParse(&pms_meas);
            // sum it
            pms_meas_sum.pm10 += pms_meas.concPM10_0_amb;
            pms_meas_sum.pm2_5 += pms_meas.concPM2_5_amb;
            pms_meas_sum.pm1_0 += pms_meas.concPM1_0_amb;
            pms_meas_count++;
        }
    }
}

