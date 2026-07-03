#include "SPI_bus_manager.h"
#include "config.h"
#include <Arduino.h>

SPIClass spiSensorBus(HSPI);

void deselectSpiDevices() {
  digitalWrite(PIN_IMU_CS, HIGH);
  digitalWrite(PIN_SD_CS, HIGH);
}

void initSensorBus() {
  pinMode(PIN_IMU_CS, OUTPUT);
  pinMode(PIN_SD_CS, OUTPUT);

  deselectSpiDevices();
  delay(20);

  spiSensorBus.begin(PIN_IMU_SCK, PIN_IMU_MISO, PIN_IMU_MOSI, PIN_SD_CS);
}