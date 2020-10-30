#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include "declarations.h"
#include "MemoryFree.h"
#include "readtemps.h"


#define DHTTYPE DHT22

byte id = 0x10;
char idString[] = "10";

byte mac[] = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, id };
IPAddress server(192,168,91,215);

char* metricsTopic = "metrics/";


EthernetClient ethClient;
PubSubClient mclient(ethClient);

Conf conf;

byte nrlocalsensors;
long configTime;
long lastReport;
long lastRead;
long lastReconnectAttempt;
bool pinsset = false;
reading ambient_readings[10];
reading local_readings[30];



void callback(char* topic, byte* payload, unsigned int length);

void setup(void) {
  for (byte pin = 0; pin <= 54; pin++) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  Serial.begin(9600); //Begin serial communication
  delay(1000);
  Serial.println(F("Arduino mega Heating/Cooling controller")); //Print a message


  strcat(metricsTopic, idString);
  Serial.println(F("Setup done"));
  //request config on startup
}

void configure(byte* payload) {

  Serial.println(F("Got reconfiguration request"));
  StaticJsonDocument<1024> jconf;
  DeserializationError error = deserializeJson(jconf, payload);
  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    //Serial.println(error.c_str());
    return;
  }

  conf.nrtsens = jconf["t"].size();
  conf.nrhtsens = jconf["ht"].size();
  conf.nrvalves = jconf["v"].size();
  conf.heatcoolmode = jconf["hcmode"];

  for (byte i = 0; i < conf.nrtsens; i++) {
    byte pin = jconf["t"][i]["p"];
    conf.tsens[i].onewire = new OneWire(pin);
    conf.tsens[i].sensors = new DallasTemperature(conf.tsens[i].onewire);
    conf.tsens[i].level = jconf["t"][i]["l"];
    strcpy(conf.tsens[i].zone, jconf["t"][i]["z"]);
  }

  for (byte i = 0; i < conf.nrhtsens; i++) {
    conf.htsens[i].sensor = new DHT(jconf["ht"][i]["p"],DHTTYPE);
    conf.htsens[i].sensor->begin();
    conf.htsens[i].level = jconf["ht"][i]["l"];
    strcpy(conf.htsens[i].zone, jconf["ht"][i]["z"]);
  }

  for (byte i = 0; i < conf.nrvalves; i++) {
    conf.valves[i].pin = jconf["v"][i]["p"];
    conf.valves[i].ambient_sensor = jconf["v"][i]["a"];
    conf.valves[i].local_sensor_group = jconf["v"][i]["l"];
    conf.valves[i].local_sensor_index = jconf["v"][i]["i"];
  }

  pinsset = false;

}

boolean reconnect() {
  Ethernet.init();
  delay(100);
  if (Ethernet.begin(mac) != 0) {
    //Serial.println(Ethernet.localIP());
    mclient.setServer(server, 1883);
    mclient.setCallback(callback);
  }
  if (mclient.connect("arduinoClient2")) {
    // ... and resubscribe

    char topic[20];
    //snprintf(topic, sizeof topic, "%s/lights/#", idString );
    //mclient.subscribe(topic);
    //snprintf(topic, sizeof topic, "%s/shutters/#", idString );
    //mclient.subscribe(topic);
    //snprintf(topic, sizeof topic, "%s/fans/#", idString );
    //mclient.subscribe(topic);
    snprintf(topic, sizeof topic, "%s/setconfig", idString );
    mclient.subscribe(topic);
    Serial.println(F("Connected to mqtt"));
  }
  return mclient.connected();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print(F("got smth on "));
  //Serial.print("Message arrived [");
  Serial.println(topic);
  //Serial.print("] ");
  //Serial.println(length);
  //for (unsigned int i=0;i<length;i++) {
  //  Serial.print((char)payload[i]);
  //}
  //if (String(topic).substring(3,10) == "lights/") {
  //  //Serial.print(F("got light "));

  //  String lightattr = String(topic).substring(10,String(topic).length());
  //  setlight(lightattr,payload,length);
  //}
  //if (String(topic).substring(3,12) == "shutters/") {
  //  String shutterattr = String(topic).substring(12,String(topic).length());
  //  //Serial.print("Shutter command via MQTT ");
  //  //Serial.println(shutter.toInt());
  //  setshutter(shutterattr,payload,length);
  //}
  //if (String(topic).substring(3,8) == "fans/") {
  //  String fanattr = String(topic).substring(8,String(topic).length());
  //  //Serial.print("Fan command via MQTT ");
  //  //Serial.println(shutter.toInt());
  //  setfan(fanattr,payload,length);
  //}

  //char* cfgtopic;
  //snprintf(cfgtopic, sizeof topic, "%s/setconfig", idString );
  if (String(topic) == "10/setconfig") {
    Serial.println(F("got config"));

    configure(payload);
  }

  //Serial.println();
}

void setpins() {
  // apparently dallas crap already sets what is needed
  //for (byte i = 0; i < conf.nrtsens; i++) {
  //  pinMode(conf.tsens[i].pin, INPUT);
  //}

  // apparently dht.begin() already sets what is needed
  //for (byte i = 0; i < conf.nrhtsens; i++) {
  //  pinMode(conf.htsens[i].pin, INPUT);
  //}

  for (byte i = 0; i < conf.nrvalves; i++) {
    pinMode(conf.valves[i].pin, OUTPUT);
  }

  pinsset = true;

}

int pltoint(byte* payload, unsigned int length) {
  char temp[length+1];

  strncpy(temp, payload, length);
  //temp[length] = '\0';
  return atoi(temp);
}


void loop() {
  Ethernet.maintain();
  if (!mclient.connected()) {
    //attempt to reconnect to mqtt every 20s
    if (millis() - lastReconnectAttempt > 20000) {
      Serial.println(F("MQTT not connected"));
      lastReconnectAttempt = millis();
      if (reconnect()) {
        lastReconnectAttempt = 0;
        mclient.publish("configreq",idString);
        configTime = millis();
      }
    }
  }
  else {
    mclient.loop();
  }


  //alow 5s to receive config, else use the locally stored one
  if (conf.nrtsens == 0 and conf.nrhtsens == 0 and conf.nrvalves == 0)  {
    if (millis() - configTime > 5000) {
      //applystoredconfig();
      //if (conf.nrlights == 0 and conf.nrshutters == 0 and conf.nrfans == 0) {
        //Serial.println("publishing configreq");
        mclient.publish("configreq",idString);
        configTime = millis();
      }
  }
  else {
    if (not pinsset) {
      setpins();
    }

    //read temperatures every 5 seconds
    if (millis() - lastRead > 5000) {
      readtemps();
      lastRead = millis();
    }
    //applyintensities();
    //controlshutters();

    // Print debug info evrey 10s
    //if (millis() - lastVarDump > 10000) {
    //  Serial.print("shutter 0 current state ");
    //  Serial.println(shutcurstate[0]);
    //  Serial.print("target state ");
    //  Serial.println(shuttgtstate[0]);
    //  Serial.print("pin last used ");
    //  Serial.println(debugpin);
    //  lastVarDump = millis();
    //}
  }

  //report current state every 10s
  if (millis() - lastReport > 10000) {
    //char topic[10] = "metrics/" + idString;
    //char msg[12] = "freeram " + String(FreeMemory());
    //mclient.publish(strcat("metrics/", idString), "test!");
    //Serial.println(metricsTopic);
    char topic[20];
    snprintf(topic, sizeof topic, "%s/freeram", metricsTopic );
    char value[5];
    snprintf(value, sizeof value, "%i", freeMemory());
    

    //Serial.print(topic);
    //Serial.println(freeMemory());
    mclient.publish(topic, value, true);

    for (byte i=0; i < conf.nrhtsens; i++) {
      char pubtopic[100];
      String tmp;
      char val[10];

      snprintf(pubtopic, sizeof pubtopic, "%s/humidity/%i/%s", metricsTopic, ambient_readings[i].level, ambient_readings[i].zone );
      tmp = String(ambient_readings[i].humidity);
      tmp.toCharArray(val,tmp.length()+1);
      mclient.publish(pubtopic, val, true);

      snprintf(pubtopic, sizeof pubtopic, "%s/temperature/%i/%s", metricsTopic, ambient_readings[i].level, ambient_readings[i].zone );
      tmp = String(ambient_readings[i].temperature);
      tmp.toCharArray(val,tmp.length()+1);
      mclient.publish(pubtopic, val, true);


      snprintf(pubtopic, sizeof pubtopic, "%s/dewpoint/%i/%s", metricsTopic, ambient_readings[i].level, ambient_readings[i].zone );
      tmp = String(ambient_readings[i].dewpoint);
      tmp.toCharArray(val,tmp.length()+1);
      mclient.publish(pubtopic, val, true);

    }

    for (byte i=0; i < nrlocalsensors; i++) {
      char pubtopic[100];
      String tmp;
      char val[10];
      snprintf(pubtopic, sizeof pubtopic, "%s/temperature/%i/%s", metricsTopic, local_readings[i].level, local_readings[i].zone);
      tmp = String(local_readings[i].temperature);
      //snprintf(val, sizeof val, "%.2f", local_readings[i].temperature);
      tmp.toCharArray(val,tmp.length()+1);
      Serial.print(pubtopic);
      Serial.print(F(": "));
      Serial.println(val);
      Serial.println(local_readings[i].zone);
      mclient.publish(pubtopic, val, true);
    }
     
    lastReport = millis();

  }

}
