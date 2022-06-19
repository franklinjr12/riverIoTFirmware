
#define TINY_GSM_MODEM_SIM800
#define SerialMon Serial
#define SerialAT Serial2
#define TINY_GSM_DEBUG SerialMon
// set GSM PIN, if any
#define GSM_PIN ""

// Your GPRS credentials, if any
const char apn[] = "timbrasil.br";
const char gprsUser[] = "tim";
const char gprsPass[] = "tim";

// MQTT details
const char *broker = "broker.hivemq.com";

const char *topicLed = "GsmClientTest/led";
const char *topicInit = "GsmClientTest/init";
const char *topicLedStatus = "GsmClientTest/ledStatus";

#include <Arduino.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <Preferences.h>

#include "my_ultrassonic.h"
#include "my_bmp280.h"

#define PIN_DTR 25
#define PIN_RST 26
#define PIN_RX2 16
#define PIN_TX2 17
#define PWR 4

#define TIME_TO_SLEEP (5 * 60 * 1000000)

#define LED_PIN 13
int ledStatus = LOW;

TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

Preferences preferences;

uint32_t lastReconnectAttempt = 0;

unsigned int bootCounter;
unsigned int retriesCounter;
unsigned long onTimeCounter;
unsigned long connectionTimeCounter;

const int pin_battery = 39;

float vbat = 0;

// float read_battery() { return (((float)analogRead(pin_battery)) * 1.1736334 * 3.3 * 2.0) / 4096.0; }
float read_battery() { return (((float)analogRead(pin_battery)) * 3.3 * 2.0) / 4096.0; }

void mqttCallback(char *topic, byte *payload, unsigned int len)
{
    SerialMon.print("Message arrived [");
    SerialMon.print(topic);
    SerialMon.print("]: ");
    SerialMon.write(payload, len);
    SerialMon.println();

    // Only proceed if incoming message's topic matches
    if (String(topic) == topicLed)
    {
        ledStatus = !ledStatus;
        digitalWrite(LED_PIN, ledStatus);
        mqtt.publish(topicLedStatus, ledStatus ? "1" : "0");
    }
}

boolean mqttConnect()
{
    SerialMon.print("Connecting to ");
    SerialMon.print(broker);

    // Connect to MQTT Broker
    boolean status = mqtt.connect("GsmClientTest");

    // Or, if you want to authenticate MQTT:
    // boolean status = mqtt.connect("GsmClientName", "mqtt_user", "mqtt_pass");

    if (status == false)
    {
        SerialMon.println(" fail");
        return false;
    }
    SerialMon.println(" success");
    mqtt.publish(topicInit, "GsmClientTest started");
    mqtt.subscribe(topicLed);
    return mqtt.connected();
}

void setup()
{
    onTimeCounter = millis();

    preferences.begin("my-app", false);
    bootCounter = preferences.getUInt("bootCounter", 0);
    retriesCounter = preferences.getUInt("retriesCounter", 0);
    bootCounter++;
    preferences.putUInt("bootCounter", bootCounter);

    // Set console baud rate
    // SerialMon.begin(115200);
    delay(10);
    Serial.printf("boot %u tries %u\n", bootCounter, retriesCounter);
    SerialMon.println("\nWait...");

    bmp280_setup();
    Serial.println("bmp280 initiated");
    ultrassonic_setup();
    Serial.println("ultrassonic initiated");
    pinMode(pin_battery, INPUT);
    const float samples = 4;
    vbat = 0;
    for (int i = 0; i < samples; i++)
    {
        vbat += read_battery() / samples;
    }
    Serial.println("battery read");

    ultrassonic_turn_on();
    delay(1000);
    float d = distance_mm();
    while (d == 0)
        d = distance_mm();
    ultrassonic_turn_off();
    Bmp_data data;
    data.A = 0;
    data.P = 0;
    data.T = 0;
    int resp_size = 50;
    char response[resp_size];
    bmp280_read(&data);
    sprintf(response, "{\"d\":%f,\"v\":%.2f,\"p\":%.2f,\"T\":%.2f,\"a\":%.2f}", d, vbat, data.P, data.T, data.A);
    Serial.println(response);

    pinMode(PWR, OUTPUT);
    digitalWrite(PWR, HIGH);
    connectionTimeCounter = millis();
    delay(3000);
    Serial2.begin(115200, SERIAL_8N1, PIN_RX2, PIN_TX2);
    Serial.println("serial2 initiated");

    // Restart takes quite some time
    // To skip it, call init() instead of restart()
    SerialMon.println("Initializing modem...");
    // modem.restart();
    modem.init();

    // String modemInfo = modem.getModemInfo();
    // SerialMon.print("Modem Info: ");
    // SerialMon.println(modemInfo);

    // #if TINY_GSM_USE_GPRS
    //     // Unlock your SIM card with a PIN if needed
    //     if (GSM_PIN && modem.getSimStatus() != 3)
    //     {
    //         modem.simUnlock(GSM_PIN);
    //     }
    // #endif

    SerialMon.print("Waiting for network...");
    if (!modem.waitForNetwork())
    {
        SerialMon.println(" fail");
        retriesCounter++;
        preferences.putUInt("retriesCounter", retriesCounter);
        preferences.end();
        digitalWrite(PWR, LOW);
        onTimeCounter = millis() - onTimeCounter;
        connectionTimeCounter = millis() - connectionTimeCounter;
        Serial.printf("onTime %lums connectionTime %lums\n", onTimeCounter, connectionTimeCounter);
        Serial.println("Entering deep sleep");
        esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP);
        esp_deep_sleep_start();
    }
    SerialMon.println(" success");

    if (modem.isNetworkConnected())
    {
        SerialMon.println("Network connected");
    }

    // GPRS connection parameters are usually set after network registration
    SerialMon.print(F("Connecting to "));
    SerialMon.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass))
    {
        SerialMon.println(" fail");
        retriesCounter++;
        preferences.putUInt("retriesCounter", retriesCounter);
        preferences.end();
        digitalWrite(PWR, LOW);
        onTimeCounter = millis() - onTimeCounter;
        connectionTimeCounter = millis() - connectionTimeCounter;
        Serial.printf("onTime %lums connectionTime %lums\n", onTimeCounter, connectionTimeCounter);
        Serial.println("Entering deep sleep");
        esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP);
        esp_deep_sleep_start();
    }
    SerialMon.println(" success");

    if (modem.isGprsConnected())
    {
        SerialMon.println("GPRS connected");
    }

    // MQTT Broker setup
    mqtt.setServer(broker, 1883);
    mqtt.setCallback(mqttCallback);

    if (mqttConnect())
    {
        retriesCounter = 0;
    }
    else
    {
        retriesCounter++;
    }
    preferences.end();
    digitalWrite(PWR, LOW);
    onTimeCounter = millis() - onTimeCounter;
    connectionTimeCounter = millis() - connectionTimeCounter;
    Serial.printf("onTime %lums connectionTime %lums\n", onTimeCounter, connectionTimeCounter);
    Serial.println("Entering deep sleep");
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP);
    esp_deep_sleep_start();
}

void loop()
{
}