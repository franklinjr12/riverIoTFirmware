#ifndef SIM800INTERFACE_HPP
#define SIM800INTERFACE_HPP

#include <Arduino.h>

#define TINY_GSM_MODEM_SIM800
#include <TinyGsmClient.h>

#define PIN_DTR 25
#define PIN_RST 26
#define PIN_RX2 16
#define PIN_TX2 17
#define PWR 4

#define APN "timbrasil.br"
#define APNUSER "tim"
#define APNPASS "tim"

typedef struct sim800InterfaceData
{
    TinyGsm *modem;
    TinyGsmClient *gsm_cli;
};

int sim800InterfaceSetup(sim800InterfaceData *sim800);
int sim800InterfaceSetCert(sim800InterfaceData *sim800);
int sim800InterfaceConnect(sim800InterfaceData *sim800);
#endif