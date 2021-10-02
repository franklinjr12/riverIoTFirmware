#ifndef MY_BMP280
#define MY_BMP280

// #include "BMP280.h"
#include "BMP280_DEV.h"
#include "Wire.h"

// #define P0 1013.25

typedef struct
{
  float T;
  float P;
  float A;
} Bmp_data;

// const int pin_sda = 23;
// const int pin_scl = 18;
const int pin_barcs = 22;

// BMP280 bmp;
Bmp_data bmp_data;

SPIClass SPI1(VSPI);                      // Create (instantiate) the SPI1 object for HSPI operation
BMP280_DEV bmp280(pin_barcs, VSPI, SPI1); // Create BMP280_DEV object and set-up for HSPI operation, SCK 14, MOSI 13, MISO 27, SS 21

void bmp280_setup()
{
  // pinMode(pin_barcs, OUTPUT);
  // digitalWrite(pin_barcs, HIGH);
  // if (!bmp.begin(pin_sda, pin_scl))
  // {
  //   Serial.println("BMP init failed!");
  //   while (1)
  //     ;
  // }
  // else
  //   Serial.println("BMP init success!");
  // bmp.setOversampling(4);

  bmp280.begin();                             // Default initialisation, place the BMP280 into SLEEP_MODE
  bmp280.setTimeStandby(TIME_STANDBY_1000MS); // Set the standby time to 1 second (1000ms)
  bmp280.startNormalConversion();             // Start NORMAL continuous conversion

  bmp_data.A = 0;
  bmp_data.T = 0;
  bmp_data.P = 0;
}

void bmp280_read(Bmp_data *data)
{
  // uint8_t result = bmp.startMeasurment();
  // if (result != 0)
  // {
  //   delay(result);
  //   result = bmp.getTemperatureAndPressure(data->T, data->P);
  //   if (result != 0)
  //   {
  //     data->A = bmp.altitude(data->P, P0);
  //   }
  //   else
  //     Serial.println("Error.");
  // }
  // else
  //   Serial.println("Error.");

  while (bmp280.getMeasurements(data->T, data->P, data->A) == false)
    delay(100);
}

#endif