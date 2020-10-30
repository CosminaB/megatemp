#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
// Update these with values suitable for your network.
int pltoint(byte* payload, unsigned int length);

struct tsensorgroup {
  OneWire *onewire;
  DallasTemperature *sensors;
  byte pin;
  byte level;
  char zone[20];
};

struct htsensor {
  DHT *sensor;
  byte level;
  char zone[20];
};

struct valve {
  byte pin;
  byte ambient_sensor;
  byte local_sensor_group;
  byte local_sensor_index;
};

struct Conf {
  byte nrtsens;
  byte nrhtsens;
  byte nrvalves;
  bool heatcoolmode;
  tsensorgroup tsens[2];
  htsensor htsens[10];
  valve valves[20];
};

struct reading {
  char zone[20];
  byte level;
  float temperature;
  float humidity;
  float dewpoint;
};


//extern PubSubClient mclient;

extern Conf conf;
extern long configTime;
extern bool pinsset;
extern byte nrlocalsensors;
extern reading local_readings[30];
extern reading ambient_readings[10];
