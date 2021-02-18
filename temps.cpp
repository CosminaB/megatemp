#include "Arduino.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "declarations.h"


#define DHTTYPE DHT22


void powersensors(bool on) {

  for (byte i=0; i < conf.nrtsens; i++) {
    if (on) {
      digitalWrite(conf.tsens[i].pwrpin,HIGH);
    } else {
      digitalWrite(conf.tsens[i].pwrpin,LOW);
    }
  }
  senspwr = on;

}

void readtemps() {

  //we read all local sensors because this is needed anyway
  for (byte i=0; i < conf.nrtsens; i++) {

    conf.tsens[i].sensors->begin();
    byte count = conf.tsens[i].sensors->getDeviceCount();
    Serial.print(F("detected "));
    Serial.println(count);
    if (count == 0) {
      //alert on no sensors detected
    }
    conf.tsens[i].sensors->requestTemperatures();
    for (byte j=0; j < count; j++) {
      localtemps[i][j] = conf.tsens[i].sensors->getTempCByIndex(j);
    }

  }
  powersensors(0);

  // populate tempsources with local readings
  for (byte i=0; i < conf.nrtsrcs; i++) {
    if (conf.tempsources[i].type == 0) {
      float t;
      t = localtemps[conf.tempsources[i].localgroup][conf.tempsources[i].localindex];
      //only consider plausible results
      if (t > -30 and t < 70) {
        temperatures[i] = t;
      }
      else {
        // alert on implausible result
      }
    }
  }

  // populate tempsources with averages
  for (byte i=0; i < conf.nrtsrcs; i++) {
    if (conf.tempsources[i].type == 2) {
      float sum = 0;
      for (byte j=0; j < conf.tempsources[i].nravgs; j++) {
        sum += temperatures[j];
      }
      temperatures[i] = sum / conf.tempsources[i].nravgs;
    }
  }
  // populate tempsources with constant test value
  for (byte i=0; i < conf.nrtsrcs; i++) {
    if (conf.tempsources[i].type == 3) {
      temperatures[i] = 18.2;
    }
  }


}

void populate_external_temps(String topic_str, String payload_str) {
  for (byte i=0; i < conf.nrtsrcs; i++) {
    if (conf.tempsources[i].type == 1) {
      if (topic_str == conf.tempsources[i].topic) {
        temperatures[i] = atof(payload_str.c_str());
      }
    }
  }
}

void thermoregulate() {

  // iterate over all mappings
  for (byte i=0; i < conf.nrmaps; i++) {
    
    //only act for automatic maps
    if (conf.mappings[i].automatic == 1) {

      //check if an override is not in effect
      if (lastcommand[i] == 0 or (millis() - lastcommand[i] > conf.mappings[i].ovrsecs * 1000)) {

        float mintemp = conf.mappings[i].tsetpoint - (conf.mappings[i].hysteresis / 2);
        float maxtemp = conf.mappings[i].tsetpoint + (conf.mappings[i].hysteresis / 2);
        float ctemp = temperatures[conf.mappings[i].tsource];
        byte mode = conf.mappings[i].mode;
        //Serial.print(conf.mappings[i].mname);
        //Serial.print(F(" min, max, current: "));
        //Serial.print(mintemp);
        //Serial.print(F(" "));
        //Serial.print(maxtemp);
        //Serial.print(F(" "));
        //Serial.println(ctemp);
        for (byte j=0; j < conf.mappings[i].nrpumps; j++) {
          byte p = conf.mappings[i].pumps[j];

          //Serial.print(conf.pumps[p].pname);
          //Serial.print(F(" millis - lastchange "));
          //Serial.print(millis() - laststatechange[p]);
          //Serial.print(F(" > "));
          //Serial.println(conf.pumps[p].minsecsflap * 1000);


          //if current temp is more than maxtemp and mode is heating, or 
          //if current temp is less than mintemp and mode is cooling
          //stop pump
          if ((mode == 1 and ctemp > maxtemp) or (mode == 0 and ctemp < mintemp)) {
            digitalWrite(conf.pumps[p].pin, LOW);
            if (pumpstate[p] == 1) {
              laststatechange[p] = millis();
            }
            pumpstate[p] = 0;
          }
            
          if ((mode == 1 and ctemp < mintemp) or (mode == 0 and ctemp > maxtemp)) {
            // check if pump isn't flapping state too fast
            if (millis() - laststatechange[p] > conf.pumps[p].minsecsflap * 1000) {
              digitalWrite(conf.pumps[p].pin, HIGH);
              if (pumpstate[p] == 0) {
                laststatechange[p] = millis();
              }
              pumpstate[p] = 1;
            }
          } 
        }
      }
    }
  }
}
