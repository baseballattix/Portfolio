#include "baro.h"
#include "config.h"

#include <Wire.h>
#include <Adafruit_BME280.h>
#include <math.h>

static Adafruit_BME280 bme;

bool BaroDriver::begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    ok_ = bme.begin(0x76) || bme.begin(0x77);
    return ok_;
}

BaroData BaroDriver::read() const {
    BaroData out;
    if (bme.sensorID() != 0xFF) {
    out.valid = true;
    out.tempC = bme.readTemperature();
    out.pres_hPa = bme.readPressure() / 100.0f;
    out.alt_baro_m = 44330.0f * (1.0f - powf(out.pres_hPa / SEALEVEL_hPa, 0.1903f));
  }

  return out;
}