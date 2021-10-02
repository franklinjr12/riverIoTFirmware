#ifndef MY_ULTRASSONIC
#define MY_ULTRASSONIC

#define PIN_TRIG 13
#define PIN_ECHO 14
#define PIN_ONOFF 27

unsigned long time1 = 0;
unsigned long time2 = 0;
bool b_ready = false;

ICACHE_RAM_ATTR void isr_echo(void)
{
  if (time1 > 0)
  {
    time2 = micros();
    b_ready = true;
  }
  else
    time1 = micros();
}

float distance_mm(void)
{
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  const unsigned long timeout = 1000;
  const unsigned long t1 = millis();
  unsigned long t2 = millis();
  while ((b_ready == false) && (t2 - t1 < timeout))
    t2 = millis();
  b_ready = false;
  float dt = time2 - time1;
  float dmm = dt / 5.8;
  time1 = 0;
  if (t2 - t1 > timeout)
    return 0;
  if (dmm > 7000)
    return 0;
  return dmm;
}

void ultrassonic_setup()
{
  pinMode(PIN_ONOFF, OUTPUT);
  digitalWrite(PIN_ONOFF, LOW);
  pinMode(PIN_TRIG, OUTPUT);
  digitalWrite(PIN_TRIG, LOW);
  pinMode(PIN_ECHO, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_ECHO), isr_echo, CHANGE);
}

void ultrassonic_turn_on()
{
  digitalWrite(PIN_ONOFF, HIGH);
}

void ultrassonic_turn_off()
{
  digitalWrite(PIN_ONOFF, LOW);
}

#endif