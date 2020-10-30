#include "Arduino.h"
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include "declarations.h"


#define DHTTYPE DHT22

void readtemps() {

  for (byte i=0; i < conf.nrhtsens; i++) {

    float h = conf.htsens[i].sensor->readHumidity();
    float t = conf.htsens[i].sensor->readTemperature();
    float d = t - ( (100 - h) / 5);
    ambient_readings[i].temperature = t;
    ambient_readings[i].humidity = h;
    ambient_readings[i].dewpoint = d;
    strcpy(ambient_readings[i].zone, conf.htsens[i].zone);
    ambient_readings[i].level = conf.htsens[i].level;

  }

  byte k=0;
  for (byte i=0; i < conf.nrtsens; i++) {

    conf.tsens[i].sensors->begin();
    byte count = conf.tsens[i].sensors->getDeviceCount();
    Serial.print(F("detected "));
    Serial.println(count);
    conf.tsens[i].sensors->requestTemperatures();
    for (byte j=0; j < count; j++) {
      Serial.print(k);
      Serial.print(F(": "));
      local_readings[k].temperature = conf.tsens[i].sensors->getTempCByIndex(j);
      Serial.println(local_readings[k].temperature);
      char buffer[20];
      snprintf(buffer, sizeof(buffer), "%s_%i", conf.tsens[i].zone, j);
      Serial.println(conf.tsens[i].zone);

      strcpy(local_readings[k].zone, buffer);

      local_readings[k].level = conf.tsens[i].level;
      k++;
    }
    nrlocalsensors = k;
  }
  
}
