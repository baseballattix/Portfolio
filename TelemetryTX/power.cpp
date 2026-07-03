#include "power.h"
#include "config.h"

#include <Wire.h>
#include <XPowersLib.h>

static XPowersAXP2101 pmu;

bool PowerDriver::begin() {
  pinMode(PIN_GPS_EN, OUTPUT);
  digitalWrite(PIN_GPS_EN, HIGH);
  delay(10);

  Wire1.begin(PIN_PMU_SDA, PIN_PMU_SCL);
  delay(20);

  pmu_ok_ = pmu.begin(Wire1, AXP2101_SLAVE_ADDRESS, PIN_PMU_SDA, PIN_PMU_SCL);
  if (pmu_ok_) {
    // GPS rail
    pmu.setALDO4Voltage(3300);
    pmu.enableALDO4();

    // SD card rail
    pmu.setBLDO1Voltage(3300);
    pmu.enableBLDO1();

    delay(100);
  }

  return pmu_ok_;
}