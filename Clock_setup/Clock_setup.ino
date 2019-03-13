#include <Wire.h>
#include <DS3231.h>

#include <time.h>

DS3231 RTCclock;

void setup() {
  Serial.begin(9600);
  RTCclock.begin();
  RTCclock.setDateTime(__DATE__,__TIME__);
}

void loop() {

}
