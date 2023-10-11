#include "Arduino.h"

#include "Nicla_System.h"
//https://docs.arduino.cc/tutorials/nicla-sense-me/cheat-sheet#sensors
#include "Arduino_BHY2.h"
Sensor temperature(128);  // declare temperature sensor object
Sensor humidity(130);  // declare humidity sensor object
bool col_green = false;


void setup() {

  // init nicla board
  nicla::begin();
  nicla::leds.begin();

  // init temperature measurement
  Serial.begin(9600);
  BHY2.begin();
  temperature.begin();
  humidity.begin();
}

void loop() {

  // internal LED blink
  if (col_green) {
    nicla::leds.setColor(off);
    col_green = false;
  } else {
    nicla::leds.setColor(green);
    col_green = true;
  }

  delay(1000);
  BHY2.update();

  // Check sensor values
  Serial.print(String("temperature: ") + String(int(temperature.value())));
  Serial.println(String(" | humidity: ") + String(int(humidity.value())));
}