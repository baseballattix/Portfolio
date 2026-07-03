#pragma once
#include <Arduino.h>

struct AirspeedData {
  bool valid = false;
  float airspeed_mph = 0.0f;
};

class AirspeedDriver {
public:
  bool begin();
  void update();
  AirspeedData read() const;
  bool ok() const { return ok_; }

private:
  bool readSensor(uint16_t& pressureCounts, uint16_t& tempCounts, uint8_t& status);
  float countsToNormalizedDiff(uint16_t counts) const;
  float countsToPressurePa(uint16_t counts) const;
  float pressurePaToMph(float pa) const;

  AirspeedData data_;
  bool ok_ = false;
};