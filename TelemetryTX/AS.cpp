#include "AS.h"
#include <Wire.h>
#include <math.h>
#include "config.h"

//static TwoWire asWire = TwoWire(0);   // or TwoWire(1) depending on board support

static const float PRESSURE_RANGE_PSI = 1.0f;
static const float PSI_TO_PA = 6894.75729f;
static const float AIR_DENSITY = 1.225f;
static const float MPS_TO_MPH = 2.23694f;

static const float COUNTS_MIN = 1638.0f;
static const float COUNTS_MAX = 14745.0f;
static const float COUNTS_SPAN = COUNTS_MAX - COUNTS_MIN;

bool AirspeedDriver::begin() {
  //asWire.begin(PIN_AS_SDA, PIN_AS_SCL);
  //asWire.setClock(100000);

  data_.valid = false;
  data_.airspeed_mph = .0f;

  uint16_t pressureCounts = 0;
  uint16_t tempCounts = 0;
  uint8_t status = 0;

  ok_ = readSensor(pressureCounts, tempCounts, status);
  return ok_;
}

void AirspeedDriver::update() {
  uint16_t pressureCounts = 0;
  uint16_t tempCounts = 0;
  uint8_t status = 0;

  if (!readSensor(pressureCounts, tempCounts, status)) {
    data_.valid = false;
    ok_ = false;
    return;
  }

  if (status != 0) {
    data_.valid = false;
    ok_ = false;
    return;
  }

  float pa = countsToPressurePa(pressureCounts);
  float mph = pressurePaToMph(pa);

  data_.airspeed_mph = mph;
  data_.valid = true;
  ok_ = true;
}

AirspeedData AirspeedDriver::read() const {
  return data_;
}

bool AirspeedDriver::readSensor(uint16_t& pressureCounts, uint16_t& tempCounts, uint8_t& status) {
  int n = Wire.requestFrom((uint8_t)AS_addr, (uint8_t)4);
  if (n != 4) {
    return false;
  }

  uint8_t b0 = Wire.read();
  uint8_t b1 = Wire.read();
  uint8_t b2 = Wire.read();
  uint8_t b3 = Wire.read();

  status = (b0 >> 6) & 0x03;
  pressureCounts = (((uint16_t)(b0 & 0x3F)) << 8) | b1;
  tempCounts = (((uint16_t)b2) << 3) | (b3 >> 5);

  return true;
}

float AirspeedDriver::countsToNormalizedDiff(uint16_t counts) const {
  float mid = (COUNTS_MIN + COUNTS_MAX) * 0.5f;
  float halfSpan = COUNTS_SPAN * 0.5f;
  return ((float)counts - mid) / halfSpan;
}

float AirspeedDriver::countsToPressurePa(uint16_t counts) const {
  float norm = countsToNormalizedDiff(counts);
  float psi = norm * PRESSURE_RANGE_PSI;
  return psi * PSI_TO_PA;
}

float AirspeedDriver::pressurePaToMph(float pa) const {
  if (pa <= 0.0f) return 0.0f;
  float mps = sqrtf((2.0f * pa) / AIR_DENSITY);
  return mps * MPS_TO_MPH;
}