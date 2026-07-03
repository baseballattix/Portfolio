#pragma once
#include <Arduino.h>

struct TEL_PACKET;

class LoRaDriver {
public:
  bool begin();
  bool transmit(const TEL_PACKET& pkt);
  bool ok() const { return ok_; }

private:
  bool waitBusyLow(uint32_t tout_ms = 500);
  bool sx1262TransferCmd(uint8_t cmd, uint8_t* rx, size_t rxLen);
  bool sx1262GetStatus(uint8_t& status);
  bool sx1262SetStandbyRc();
  void hardReset();
  bool bringupAndConfig();

  bool ok_ = false;
};