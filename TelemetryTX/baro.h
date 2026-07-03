#pragma once
#include <Arduino.h>

struct BaroData {
  bool valid = false;
  float tempC = NAN;
  float pres_hPa = NAN;
  float alt_baro_m = NAN;
};

class BaroDriver {
public:
  bool begin();
  BaroData read() const;
  bool ok() const { return ok_; }

private:
  bool ok_ = false;
};