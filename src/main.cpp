// Copyright(c) Microsoft Corporation.All rights reserved.SPDX - License - Identifier : MIT

#include <cstdlib>
#include <string.h>
#include <time.h>

#include <WiFi.h>
#include <mqtt_client.h>

#include <PubSubClient.h>

#include <az_iot_hub_client.h>
#include <az_result.h>
#include <az_span.h>

#include "AzIoTSasToken.h"
#include "SerialLogger.h"
#include "ca.h"
#include "iot_configs.h"

#include "sim800Interface.hpp"

#include "my_bmp280.h"
#include "my_ultrassonic.h"

#define sizeofarray(a) (sizeof(a) / sizeof(a[0]))
#define NTP_SERVERS "pool.ntp.org", "time.nist.gov"
#define MQTT_QOS1 1
#define DO_NOT_RETAIN_MSG 0
#define SAS_TOKEN_DURATION_IN_MINUTES 60
#define UNIX_TIME_NOV_13_2017 1510592825

// #define TIME_TO_SLEEP (30 * 1000000) // each 30s
#define TIME_TO_SLEEP (5 * 60 * 1000000) // each 30s

// #define PST_TIME_ZONE -8
// #define PST_TIME_ZONE_DST_DIFF 1
// #define GMT_OFFSET_SECS (PST_TIME_ZONE * 3600)
// #define GMT_OFFSET_SECS_DST ((PST_TIME_ZONE + PST_TIME_ZONE_DST_DIFF) * 3600)
#define GMT_OFFSET_SECS (5 * 3600)
#define GMT_OFFSET_SECS_DST (0 * 3600)

static const char *ssid = IOT_CONFIG_WIFI_SSID;
static const char *password = IOT_CONFIG_WIFI_PASSWORD;
static const char *host = IOT_CONFIG_IOTHUB_FQDN;
static const char *mqtt_broker_uri = "mqtts://" IOT_CONFIG_IOTHUB_FQDN;
static const char *device_id = IOT_CONFIG_DEVICE_ID;
static const int mqtt_port = 8883;

const int pin_battery = 39;

static esp_mqtt_client_handle_t mqtt_client;
static az_iot_hub_client client;

static char mqtt_client_id[128];
static char mqtt_username[128];
static char mqtt_password[200];
static uint8_t sas_signature_buffer[256];
static unsigned long next_telemetry_send_time_ms = 0;
static char telemetry_topic[128];
static uint8_t telemetry_payload[200];
static uint32_t telemetry_send_count = 0;

const int len = 200;
char buf[len];
float vbat = 0;

bool mqtt_connected = false;

sim800InterfaceData sim800;
PubSubClient *mqttClient;

float read_battery() { return (((float)analogRead(pin_battery)) * 1.1736334 * 3.3 * 2.0) / 4096.0; }

static AzIoTSasToken sasToken(
    &client,
    AZ_SPAN_FROM_STR(IOT_CONFIG_DEVICE_KEY),
    AZ_SPAN_FROM_BUFFER(sas_signature_buffer),
    AZ_SPAN_FROM_BUFFER(mqtt_password));

int connectInternet()
{
  int err = 0;

  return err;
}

static void connectToWiFi()
{
  Logger.Info("Connecting to WIFI SSID " + String(ssid));

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long t1 = millis();
  unsigned long t2 = millis();
  unsigned long timeout = 10000;
  while (WiFi.status() != WL_CONNECTED && (t2 - t1 < timeout))
  {
    delay(500);
    Serial.print(".");
    t2 = millis();
  }
  if (t2 - t1 >= timeout)
  {
    Serial.println("Error connecting to wifi");
    ESP.restart();
  }

  Serial.println("");

  Logger.Info("WiFi connected, IP address: " + WiFi.localIP().toString());
}

static void initializeTime()
{
  Logger.Info("Setting time using SNTP");

  configTime(GMT_OFFSET_SECS, GMT_OFFSET_SECS_DST, NTP_SERVERS);
  time_t now = time(NULL);
  const unsigned long timeout = 10000;
  unsigned long t1 = millis();
  unsigned long t2 = millis();
  while (now < UNIX_TIME_NOV_13_2017 && (t2 - t1 < timeout))
  {
    delay(500);
    t2 = millis();
    Serial.print(".");
    now = time(nullptr);
  }
  if (t2 - t1 >= timeout)
  {
    Serial.println("Error setting SNTP timeout");
    ESP.restart();
  }
  Serial.println("");
  Logger.Info("Time initialized!");
}

void receivedCallback(char *topic, byte *payload, unsigned int length)
{
  Logger.Info("Received [");
  Logger.Info(topic);
  Logger.Info("]: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
  switch (event->event_id)
  {
  case MQTT_EVENT_ERROR:
    Logger.Info("MQTT event MQTT_EVENT_ERROR");
    break;
  case MQTT_EVENT_CONNECTED:
    mqtt_connected = true;
    Logger.Info("MQTT event MQTT_EVENT_CONNECTED");
    break;
  case MQTT_EVENT_DISCONNECTED:
    Logger.Info("MQTT event MQTT_EVENT_DISCONNECTED");
    break;
  case MQTT_EVENT_SUBSCRIBED:
    Logger.Info("MQTT event MQTT_EVENT_SUBSCRIBED");
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    Logger.Info("MQTT event MQTT_EVENT_UNSUBSCRIBED");
    break;
  case MQTT_EVENT_PUBLISHED:
    Logger.Info("MQTT event MQTT_EVENT_PUBLISHED");
    break;
  case MQTT_EVENT_DATA:
    Logger.Info("MQTT event MQTT_EVENT_DATA");
    break;
  case MQTT_EVENT_BEFORE_CONNECT:
    Logger.Info("MQTT event MQTT_EVENT_BEFORE_CONNECT");
    break;
  default:
    Logger.Error("MQTT event UNKNOWN");
    break;
  }
}

static void initializeIoTHubClient()
{
  if (az_result_failed(az_iot_hub_client_init(
          &client,
          az_span_create((uint8_t *)host, strlen(host)),
          az_span_create((uint8_t *)device_id, strlen(device_id)),
          NULL)))
  {
    Logger.Error("Failed initializing Azure IoT Hub client");
    return;
  }

  size_t client_id_length;
  if (az_result_failed(az_iot_hub_client_get_client_id(
          &client, mqtt_client_id, sizeof(mqtt_client_id) - 1, &client_id_length)))
  {
    Logger.Error("Failed getting client id");
    return;
  }

  // Get the MQTT user name used to connect to IoT Hub
  if (az_result_failed(az_iot_hub_client_get_user_name(
          &client, mqtt_username, sizeofarray(mqtt_username), NULL)))
  {
    Logger.Error("Failed to get MQTT clientId, return code");
    return;
  }

  Logger.Info("Client ID: " + String(mqtt_client_id));
  Logger.Info("Username: " + String(mqtt_username));
}

static int initializeMqttClient()
{
  if (sasToken.Generate(SAS_TOKEN_DURATION_IN_MINUTES) != 0)
  {
    Logger.Error("Failed generating SAS token");
    return 1;
  }
  esp_mqtt_client_config_t mqtt_config;
  memset(&mqtt_config, 0, sizeof(mqtt_config));
  mqtt_config.uri = mqtt_broker_uri;
  mqtt_config.port = mqtt_port;
  mqtt_config.client_id = mqtt_client_id;
  mqtt_config.username = mqtt_username;
  mqtt_config.password = (const char *)az_span_ptr(sasToken.Get());
  mqtt_config.keepalive = 30;
  mqtt_config.disable_clean_session = 0;
  mqtt_config.disable_auto_reconnect = false;
  mqtt_config.event_handle = mqtt_event_handler;
  mqtt_config.user_context = NULL;
  mqtt_config.cert_pem = (const char *)ca_pem;

  mqtt_client = esp_mqtt_client_init(&mqtt_config);

  if (mqtt_client == NULL)
  {
    Logger.Error("Failed creating mqtt client");
    return 1;
  }

  esp_err_t start_result = esp_mqtt_client_start(mqtt_client);

  if (start_result != ESP_OK)
  {
    Logger.Error("Could not start mqtt client; error code:" + start_result);
    return 1;
  }
  else
  {
    Logger.Info("MQTT client started");
    return 0;
  }
}

static uint32_t getEpochTimeInSecs() { return (uint32_t)time(NULL); }

static int establishConnection()
{
  int err = 0;
  err = connectInternet();
  connectToWiFi();
  initializeTime();
  initializeIoTHubClient();
  (void)initializeMqttClient();
  return err;
}

static void getTelemetryPayload(az_span payload, az_span *out_payload)
{
  az_span original_payload = payload;
  ultrassonic_turn_on();
  delay(1000);
  float d = distance_mm();
  ultrassonic_turn_off();
  Bmp_data data;
  bmp280_read(&data);
  memset(buf, 0, len);
  sprintf(
      buf,
      "{\"deviceData\":{\"id\":\"%s\",\"d\":%f,\"v\":%.2f,\"p\":%.2f,\"t\":%.2f,\"a\":%.2f,\"time\":%ld}}",
      IOT_CONFIG_DEVICE_ID,
      d,
      vbat,
      data.P,
      data.T,
      data.A,
      time(NULL));
  Serial.printf("buf len %d buf content: %s\n", strlen(buf), buf);
  payload = az_span_copy(payload, az_span_create_from_str(buf));
  *out_payload = az_span_slice(original_payload, 0, az_span_size(original_payload) - az_span_size(payload));
}

static void sendTelemetry()
{
  az_span telemetry = AZ_SPAN_FROM_BUFFER(telemetry_payload);

  Logger.Info("Sending telemetry ...");

  // The topic could be obtained just once during setup,
  // however if properties are used the topic need to be generated again to reflect the
  // current values of the properties.
  if (az_result_failed(az_iot_hub_client_telemetry_get_publish_topic(
          &client, NULL, telemetry_topic, sizeof(telemetry_topic), NULL)))
  {
    Logger.Error("Failed az_iot_hub_client_telemetry_get_publish_topic");
    return;
  }
  getTelemetryPayload(telemetry, &telemetry);
  if (esp_mqtt_client_publish(
          mqtt_client,
          telemetry_topic,
          (const char *)az_span_ptr(telemetry),
          az_span_size(telemetry),
          MQTT_QOS1,
          DO_NOT_RETAIN_MSG) == 0)
  {
    Logger.Error("Failed publishing");
  }
  else
  {
    Logger.Info("Message published successfully");
    char b[100];
    sprintf(b, "topic: %s\n", telemetry_topic);
    Logger.Info(b);
    Serial.print("payload: ");
    for (size_t i = 0; i < az_span_size(telemetry); i++)
      Serial.print((char)az_span_ptr(telemetry)[i]);
    Serial.println("");
  }
}

void setup()
{
  Serial.println("Setup begin");
  if (sim800InterfaceSetup(&sim800) == 0)
  {
    Serial.println("sim800 initiated");
  }
  else
  {
    Serial.println("sim800 fail to initiate");
    while (1)
      ;
  }
  if (sim800InterfaceSetCert(&sim800) == 0)
  {
    Serial.println("sim800 cert setted");
  }
  else
  {
    Serial.println("sim800 fail to set cert");
    while (1)
      ;
  }
  delay(10000); // time for sim800 network connection
  // digitalWrite(PWR, LOW);

  bmp280_setup();
  Serial.println("bmp280 initiated");
  ultrassonic_setup();
  Serial.println("ultrassonic initiated");
  pinMode(pin_battery, INPUT);
  const int samples = 4;
  vbat = 0;
  for (int i = 0; i < samples; i++)
  {
    vbat += read_battery() / samples;
  }
  Serial.println("battery read");

  mqttClient = new PubSubClient(*sim800.gsm_cli);
  mqttClient->setServer(host, mqtt_port);
  establishConnection();
  Serial.println("connection initiated");
}

void loop()
{
  // Serial.println("Hello");
  // while (1)
  //   ;
  if (WiFi.status() != WL_CONNECTED)
  {
    connectToWiFi();
  }
  else if (sasToken.IsExpired())
  {
    Logger.Info("SAS token expired; reconnecting with a new one.");
    (void)esp_mqtt_client_destroy(mqtt_client);
    initializeMqttClient();
  }
  else if (millis() > next_telemetry_send_time_ms)
  {
    sendTelemetry();
    digitalWrite(PWR, LOW);
    Serial.println("Entering deep sleep");
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP);
    esp_deep_sleep_start();
    next_telemetry_send_time_ms = millis() + TELEMETRY_FREQUENCY_MILLISECS;
  }
}