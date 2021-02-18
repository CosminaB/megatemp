#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <avr/wdt.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "declarations.h"
//#include "MemoryFree.h"
#include "temps.h"


byte id = 0x11;
String idString = "11";

byte mac[] = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, id };
IPAddress server(192,168,91,215);

String metricsTopic = "metrics/";


EthernetClient ethClient;
PubSubClient mclient(ethClient);

Conf conf;

byte nrlocalsensors;
long configTime;
long lastReport;
long lastRead;
long lastReconnectAttempt;
bool pinsset = false;
long laststatechange[10];
long lastcommand[10];
byte pumpstate[10];
float temperatures[20];
float localtemps[2][10];
bool senspwr;


void callback(char* topic, byte* payload, unsigned int length);

void setup(void) {
  // set everything low to avoid random things starting
  for (byte pin = 0; pin <= 54; pin++) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  //enable the hardware watchdog at 2s
  wdt_enable(WDTO_4S);

  Serial.begin(115200); //Begin serial communication
  Serial.println(F("Arduino mega Heating/Cooling controller")); //Print a message


  metricsTopic += idString;
  Serial.println(F("Setup done"));
  //request config on startup
}

void publish_metric (String metric, String tag, String value) {
  //Serial.print(metric);
  //Serial.print(F("/"));
  //Serial.print(tag);
  //Serial.print(F(" "));
  //Serial.println(value);
  char pubtopic[70];
  //char val[10];
 
  //value.toCharArray(val,value.length()+1);
  snprintf(pubtopic, sizeof pubtopic, "%s/%s/%s", metricsTopic.c_str(), metric.c_str(), tag.c_str());
  mclient.publish(pubtopic, value.c_str(), true);

}
void configure(byte* payload) {

  Serial.println(F("Got reconfiguration request"));

  //publish_metric("freeram", "mem", String(freeMemory()));
  StaticJsonDocument<1536> jconf;
  DeserializationError error = deserializeJson(jconf, payload);
  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }

  conf.nrtsens = jconf["lt"].size();
  conf.nrpumps = jconf["p"].size();
  conf.nrmaps = jconf["m"].size();
  conf.nrtsrcs = jconf["ts"].size();
  Serial.print(F("nrtsrcs "));
  Serial.println(conf.nrtsrcs);

  for (byte i = 0; i < conf.nrtsens; i++) {
    conf.tsens[i].pin = jconf["lt"][i]["p"];
    conf.tsens[i].pwrpin = jconf["lt"][i]["pw"];
    conf.tsens[i].onewire = new OneWire(conf.tsens[i].pin);
    conf.tsens[i].sensors = new DallasTemperature(conf.tsens[i].onewire);
  }
  Serial.println(F("localsensors configured"));


  for (byte i = 0; i < conf.nrpumps; i++) {
    conf.pumps[i].pin = jconf["p"][i]["p"];
    conf.pumps[i].minsecsflap = jconf["p"][i]["m"];
    const char* pn = jconf["p"][i]["n"];
    conf.pumps[i].pname = String(pn);
  }
  Serial.println(F("pumps configured"));

   for (byte i = 0; i < conf.nrmaps; i++) {
    conf.mappings[i].nrpumps = jconf["m"][i]["p"].size();
    for (byte j=0; j < conf.mappings[i].nrpumps; j++) {
      conf.mappings[i].pumps[j] = jconf["m"][i]["p"][j];
    }
    conf.mappings[i].tsource = jconf["m"][i]["t"];
    conf.mappings[i].tsetpoint = jconf["m"][i]["s"];
    conf.mappings[i].hysteresis = jconf["m"][i]["h"];
    conf.mappings[i].automatic = jconf["m"][i]["a"];
    conf.mappings[i].ovrsecs = jconf["m"][i]["o"];
    conf.mappings[i].mode = jconf["m"][i]["m"];
    const char* mn = jconf["m"][i]["n"];
    conf.mappings[i].mname = String(mn);

  }
  Serial.println(F("maps configured"));

  for (byte i = 0; i < conf.nrtsrcs; i++) {
    conf.tempsources[i].type = jconf["ts"][i]["t"];
    conf.tempsources[i].localgroup = jconf["ts"][i]["lg"];
    conf.tempsources[i].localindex = jconf["ts"][i]["li"];
    const char* t = jconf["ts"][i]["tp"];
    conf.tempsources[i].topic = String(t);
    conf.tempsources[i].nravgs = jconf["ts"][i]["a"].size();
    for (byte j=0; j < conf.tempsources[i].nravgs; j++) {
      conf.tempsources[i].avgsrcs[j] = jconf["ts"][i]["a"][j];
    }
    const char* tn = jconf["ts"][i]["n"];
    conf.tempsources[i].tsname = String(tn);

    //subscribe to relevant topics for mqtt read temperatures
    if (conf.tempsources[i].type == 1) {
      const char* tpc = conf.tempsources[i].topic.c_str();
      mclient.subscribe(tpc);
    }


  }

  Serial.println(F("tempsources configured"));
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
  String client = "arduinoClient_" + idString;
  if (mclient.connect(client.c_str())) {
    // ... and resubscribe

    char topic[30];
    snprintf(topic, sizeof topic, "commands/%s/setpoint/#", idString.c_str());

    mclient.subscribe(topic);
    snprintf(topic, sizeof topic, "commands/%s/hysteresis/#", idString.c_str());
    mclient.subscribe(topic);
    snprintf(topic, sizeof topic, "commands/%s/control/#", idString.c_str());
    mclient.subscribe(topic);
    snprintf(topic, sizeof topic, "%s/setconfig", idString.c_str());
    mclient.subscribe(topic);
    Serial.println(F("Connected to mqtt"));
  }

  return mclient.connected();
}

//get mapping index by name
byte getbyname(String name) {
  //can't have 200 mappings, 200 means fail
  byte notfound = 200;

  for (byte i=0; i < conf.nrmaps; i++) {
    if (String(conf.mappings[i].mname) == name) {
      return i;
    }
  }

  return notfound;

}

void callback(char* topic, byte* payload, unsigned int length) {

  String topic_str = String(topic);
  Serial.print(F("got smth on "));
  Serial.println(topic);
  Serial.print(F("length "));
  Serial.println(length);
  String payload_str;
  for (unsigned int i=0;i<length;i++) {
    payload_str += (char)payload[i];
  }
  Serial.println(payload_str);
  Serial.println();

  //config topic
  if (topic_str.substring(3,12) == "setconfig") {
    configure(payload);
    return;
  }

  //topic format: commands/{controller_id}/{property}/{mapping_name}

  //temperature setpoint for mapping
  else if (topic_str.substring(12,21) == "setpoint/") {
    String mname = topic_str.substring(21,topic_str.length());
    id = getbyname(mname);
    if (id < 200) {
      conf.mappings[id].tsetpoint = atof(payload_str.c_str());
    }
  }

  //hysteresis for mapping
  else if (topic_str.substring(12,23) == "hysteresis/") {
   String mname = topic_str.substring(23,topic_str.length());
    id = getbyname(mname);
    if (id < 200) {
      Serial.print("convertig payload");
      Serial.println((char *)payload);
      conf.mappings[id].hysteresis = atof(payload_str.c_str());
      Serial.print("resulted in ");
      Serial.println(conf.mappings[id].hysteresis);
    }
  }

  else if (topic_str.substring(12,22) == "automatic/") {
    String mname = topic_str.substring(22,topic_str.length());
    id = getbyname(mname);
    if (id < 200) {
      conf.mappings[id].automatic = (byte)atoi(payload_str.c_str());
    }
  }

  //direct control of mapping
  else if (topic_str.substring(12,20) == "control/") {
    String mname = topic_str.substring(20,topic_str.length());
    id = getbyname(mname);
    if (id < 200) {
      //tbd
      //conf.mappings[id].auto = (byte)atoi((char *)payload);
    }
  }
  //if none of the above, it must be an external mqtt temperature source
  else {

    populate_external_temps(topic_str, payload_str);

  }

}

void setpins() {

  for (byte i = 0; i < conf.nrpumps; i++) {
    pinMode(conf.pumps[i].pin, OUTPUT);
  }

  pinsset = true;

}

//int pltoint(byte* payload, unsigned int length) {
//  char temp[length+1];
//
//  strncpy(temp, payload, length);
//  //temp[length] = '\0';
//  return atoi(temp);
//}

//float pltofloat(byte* payload, unsigned int length) {
//  char temp[length+1];
//
//  strncpy(temp, payload, length);
//  //temp[length] = '\0';
//  return atof(temp);
//}




void loop() {

  wdt_reset();

  //Network relalted maintenance and reconnect
  Ethernet.maintain();
  if (!mclient.connected()) {
    //attempt to reconnect to mqtt every 20s
    if (lastReconnectAttempt == 0 or millis() - lastReconnectAttempt > 20000) {
      Serial.println(F("MQTT not connected"));
      lastReconnectAttempt = millis();
      if (reconnect()) {
        lastReconnectAttempt = 0;
        mclient.publish("configreq",idString.c_str());
        configTime = millis();
      }
    }
  }
  else {
    mclient.loop();
  }


  //alow 5s to receive config, else use the locally stored one
  if (conf.nrtsens == 0 and conf.nrpumps == 0 and conf.nrmaps == 0)  {
    if (configTime == 0 or millis() - configTime > 5000) {
      mclient.publish("configreq",idString.c_str());
      configTime = millis();
    }
  } else {

    //We have a valid config, proceed

    //set pins if not already done
    if (not pinsset) {
      setpins();
    }


    //power temp sensors 4 sec after reading

    if (millis() - lastRead > 4000 and senspwr == 0) {
      powersensors(1);
    }
 
    //read and regulate temperatures every 5 seconds
    if (millis() - lastRead > 5000) {
      readtemps();
      thermoregulate();
      lastRead = millis();
    }
    

    //report current state every 10s
    if (millis() - lastReport > 10000) {

      //publish_metric("freeram", "mem", String(freeMemory()));

      for (byte i=0; i < conf.nrtsrcs; i++) {
        publish_metric("temperature", conf.tempsources[i].tsname, String(temperatures[i]));
      }

      for (byte i=0; i < conf.nrpumps; i++) {
        publish_metric("pumpstate", conf.pumps[i].pname, String(pumpstate[i]));
      }

      for (byte i=0; i < conf.nrmaps; i++) {
        publish_metric("setpoint", conf.mappings[i].mname, String(conf.mappings[i].tsetpoint));
        publish_metric("hysteresis", conf.mappings[i].mname, String(conf.mappings[i].hysteresis));
        publish_metric("automatic", conf.mappings[i].mname, String(conf.mappings[i].automatic));
      }


      lastReport = millis();

    }

  }

}
