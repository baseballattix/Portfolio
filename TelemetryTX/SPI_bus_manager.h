#pragma once
#include <SPI.h>

extern SPIClass spiSensorBus;

void initSensorBus();
void deselectSpiDevices();