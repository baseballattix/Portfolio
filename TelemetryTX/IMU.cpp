#include "IMU.h"
#include "config.h"
#include "SPI_bus_manager.h"

#include <SPI.h>
#include "SensorQMI8658.hpp"

static SensorQMI8658 qmi;

bool ImuDriver::begin() {
  deselectSpiDevices();
  delay(50);

  ok_ = qmi.begin(spiSensorBus, PIN_IMU_CS, PIN_IMU_MOSI, PIN_IMU_MISO, PIN_IMU_SCK);

  if (ok_) {
    qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_8G,
                            SensorQMI8658::ACC_ODR_250Hz,
                            SensorQMI8658::LPF_MODE_0);

    qmi.configGyroscope(SensorQMI8658::GYR_RANGE_512DPS,
                        SensorQMI8658::GYR_ODR_224_2Hz,
                        SensorQMI8658::LPF_MODE_3);

    qmi.enableAccelerometer();
    qmi.enableGyroscope();
  }

  return ok_;
}

ImuData ImuDriver::read() {
  ImuData out{};

  if (!ok_) return out;

  // make sure SD is not selected
  digitalWrite(PIN_SD_CS, HIGH);

  out.accelValid = qmi.getAccelerometer(out.ax, out.ay, out.az);
  out.gyroValid  = qmi.getGyroscope(out.gx, out.gy, out.gz);

  // extra safety
  digitalWrite(PIN_IMU_CS, HIGH);

  return out;
}