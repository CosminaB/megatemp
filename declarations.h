#include <OneWire.h>
#include <DallasTemperature.h>
// Update these with values suitable for your network.
//int pltoint(byte* payload, unsigned int length);
//float pltofloat(byte* payload, unsigned int length);

struct tsensorgroup {
  OneWire *onewire;
  DallasTemperature *sensors;
  byte pin;
  byte pwrpin;
};

struct pump {
  byte pin;
  long minsecsflap;
  String pname;
};

struct mapping {
  // pumps
  byte pumps[20];
  byte nrpumps;

  // temperature source
  byte tsource;
  
  // temperature setpoint
  float tsetpoint;

  // difference between on and off
  float hysteresis;

  // automatic or manual control, 
  //  if not automatic only external commands will be executed
  //  only pump(s) will be operated on external commands
  byte automatic;

  //mode; 1=heating, 0=cooling
  byte mode;

  // if control is automatic, external commands will override automation for this number of seconds
  long ovrsecs;
  
  // name of the mapping
  String mname;
};

struct tempsource {
  //0 local, 1 mqtt, 2 average of more sources, 3 constant 18.2 for testing
  byte type;
  // local
  byte localgroup;
  byte localindex;
  // mqtt
  String topic;

	//sources for average
  byte avgsrcs[5];
  byte nravgs;

  // source name
  String tsname;
};

struct Conf {
  // local temp sensors groups
  byte nrtsens;
  tsensorgroup tsens[2];

  // pumps
  byte nrpumps;
  pump pumps[10];

  // pump(s) and temperature mappings
  byte nrmaps;
  mapping mappings[10];

  // temperature sources
  byte nrtsrcs;
  tempsource tempsources[15];
};

//extern PubSubClient mclient;

extern Conf conf;
extern long configTime;
extern bool pinsset;
extern float temperatures[20];
extern float localtemps[2][10];
extern long laststatechange[10];
extern byte pumpstate[10];
extern long lastcommand[10];
extern bool senspwr;
