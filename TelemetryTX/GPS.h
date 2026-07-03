#pragma once
#include <Arduino.h>

struct GpsData {
  bool valid = false;
  int32_t lat_e7 = 0;
  int32_t lon_e7 = 0;
  int32_t alt_gnss_cm = 0;
  uint8_t sats = 0;
};

class GpsDriver {
public:
  bool begin();
  void update();
  GpsData read() const;
  bool ok() const { return ok_; }

private:
  void ubxCk(const uint8_t* data, uint16_t len, uint8_t& a, uint8_t& b);
  void ubxSend(uint8_t cls, uint8_t id, const uint8_t* payload, uint16_t payloadLen);
  void setRateMs(uint16_t ms);
  void cfgMsgUart1(uint8_t msgClass, uint8_t msgId, uint8_t rateUART1);
  void pruneToGgaOnly();

  bool ok_ = false;
};