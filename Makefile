BOARD_TAG    = mega
BOARD_SUB    = atmega2560
MONITOR_PORT = /dev/ttyACM0

ARDUINO_LIBS = SPI Ethernet onewire dallastemperature DHT Adafruit_Sensor pubsubclient ArduinoJson




include $(ARDMK_DIR)/Arduino.mk
