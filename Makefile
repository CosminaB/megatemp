BOARD_TAG    = mega
BOARD_SUB    = atmega2560
MONITOR_PORT = /dev/ttyUSB0

ARDUINO_LIBS = SPI Ethernet OneWire DallasTemperature-3.9.0 pubsubclient ArduinoJson Arduino-MemoryFree




include $(ARDMK_DIR)/Arduino.mk
