#pragma once
#include <Arduino.h>

class PowerDriver {
public:
  bool begin();
  bool ok() const { return pmu_ok_; }

private:
  bool pmu_ok_ = false;
};