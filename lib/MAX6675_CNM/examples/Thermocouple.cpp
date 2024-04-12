/* 
 * Project MAX6675 Thermocouple example file
 * Author: Brian Rashap
 * Date: 29-NOV-2023
 */

#include "Particle.h"
#include "MAX6675.h"

MAX6675 thermocouple;
byte status;
float tempC, tempF;

SYSTEM_MODE(SEMI_AUTOMATIC);

void setup () {
  Serial.begin(9600);
  waitFor(Serial.isConnected,10000);
  thermocouple.begin(SCK, SS, MISO);
}

void loop () {
  status = thermocouple.read();
  if (status != STATUS_OK) {
    Serial.printf("ERROR!\n");
  }

  tempC = thermocouple.getTemperature();
  tempF = (9.0/5.0)* tempC + 32;
  Serial.printf("Temperature:%0.2f or %0.2f\n",tempC,tempF);
  delay(1000);
}