#include <Arduino.h>

#include "sim800Interface.hpp"

#include "ca.h"

#define CERT_FILE "C:\\USER\\CERT.PEM"

int sim800InterfaceSetup(sim800InterfaceData *sim800)
{
    pinMode(PWR, OUTPUT);
    digitalWrite(PWR, HIGH);
    delay(3000);
    Serial2.begin(115200, SERIAL_8N1, PIN_RX2, PIN_TX2);
    Serial.println("serial2 initiated");
    sim800->modem = new TinyGsm(Serial2);
    if (!sim800->modem->init())
    {
        return -1;
    }
    sim800->gsm_cli = new TinyGsmClientSecure(*sim800->modem);
    return 0;
}

int sim800InterfaceSetCert(sim800InterfaceData *sim800)
{
    sim800->modem->sendAT(GF("+FSCREATE=" CERT_FILE));
    if (sim800->modem->waitResponse() != 1)
        return -1;

    const int cert_size = ca_pem_len;

    sim800->modem->sendAT(GF("+FSWRITE=" CERT_FILE ",0,"), cert_size, GF(",10"));
    if (sim800->modem->waitResponse(GF(">")) != 1)
    {
        return -1;
    }

    for (int i = 0; i < cert_size; i++)
    {
        char c = pgm_read_byte(&ca_pem[i]);
        sim800->modem->stream.write(c);
    }

    sim800->modem->stream.write(GSM_NL);
    sim800->modem->stream.flush();

    if (sim800->modem->waitResponse(2000) != 1)
        return -1;

    sim800->modem->sendAT(GF("+SSLSETCERT=\"" CERT_FILE "\""));
    if (sim800->modem->waitResponse() != 1)
        return -1;
    if (sim800->modem->waitResponse(5000L, GF(GSM_NL "+SSLSETCERT:")) != 1)
        return -1;
    const int retCode = sim800->modem->stream.readStringUntil('\n').toInt();

    Serial2.println();
    Serial2.println();
    Serial2.println(F("****************************"));
    Serial2.print(F("Setting Certificate: "));
    Serial2.println((0 == retCode) ? "OK" : "FAILED");
    Serial2.println(F("****************************"));

    return 0;
}

int sim800InterfaceConnect(sim800InterfaceData *sim800)
{
    if (!sim800->modem->gprsConnect(APN, APNUSER, APNPASS))
    {
        return -1;
    }
    if (sim800->modem->isGprsConnected())
    {
        Serial.println("GPRS connected");
    }
    return 0;
}