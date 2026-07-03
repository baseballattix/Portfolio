#pragma once
#include <Arduino.h>

struct ImuData {
  bool accelValid = false;
  bool gyroValid = false;
  float ax = 0, ay = 0, az = 0;
  float gx = 0, gy = 0, gz = 0;
};

class ImuDriver {
public:
  bool begin();
  ImuData read();
  bool ok() const { return ok_; }

private:
  bool ok_ = false;
};