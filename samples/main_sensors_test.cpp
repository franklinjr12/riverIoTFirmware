#include <Arduino.h>
#include "my_ultrassonic.h"
#include "my_bmp280.h"

#define PIN_DTR 25
#define PIN_RST 26
#define PIN_RX2 16
#define PIN_TX2 17
#define PWR 4

String data = "dados";

const int pin_battery = 39;

float vbat = 0;

// float read_battery() { return (((float)analogRead(pin_battery)) * 1.1736334 * 3.3 * 2.0) / 4096.0; }
float read_battery() { return (((float)analogRead(pin_battery)) * 3.3 * 2.0) / 4096.0; }

// typedef struct
// {
//   float T;
//   float P;
//   float A;
// } Bmp_data;

void setup()
{
  // Serial.begin(115200);
  Serial.println("Setup begin");

  ultrassonic_setup();
  ultrassonic_turn_on();

  pinMode(PWR, OUTPUT);
  digitalWrite(PWR, HIGH);
  delay(3000);
  Serial2.begin(115200, SERIAL_8N1, PIN_RX2, PIN_TX2);
  Serial.println("serial2 initiated");

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

  // vbat = read_battery();
  ultrassonic_turn_on();
  delay(1000);
  float d = distance_mm();
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
}

void loop()
{
  // vbat = read_battery();
  ultrassonic_turn_on();
  delay(1000);
  float d = distance_mm();
  ultrassonic_turn_off();
  Bmp_data data;
  data.A = 0;
  data.P = 0;
  data.T = 0;
  const int resp_size = 100;
  char response[resp_size];
  memset(response, 0, resp_size);
  bmp280_read(&data);
  sprintf(response, "{\"d\":%f,\"v\":%.2f,\"p\":%.2f,\"T\":%.2f,\"a\":%.2f}", d, vbat, data.P, data.T, data.A);
  Serial.println(response);
}
