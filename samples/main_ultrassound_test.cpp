#include <Arduino.h>
#include "my_ultrassonic.h"

void setup()
{
    // Serial.begin(115200);
    Serial.println("Setup begin");

    ultrassonic_setup();
    ultrassonic_turn_on();
    Serial.println("ultrassonic initiated");

    delay(1000);
}

void loop()
{
    float sum = 0;
    float samples = 5;
    for (int i = 0; i < samples; i++)
    {
        float d = distance_mm();
        while (d == 0)
        {
            delay(100);
            d = distance_mm();
        }
        sum += d;
    }
    sum = sum / samples;
    const int resp_size = 100;
    char response[resp_size];
    memset(response, 0, resp_size);
    sprintf(response, "distance= %2.2f\n", sum);
    Serial.println(response);
}
