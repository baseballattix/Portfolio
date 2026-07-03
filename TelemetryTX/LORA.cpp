#include "LORA.h"
#include "config.h"
#include "packet.h"

#include <SPI.h>
#include <RadioLib.h>

static SPIClass spiLoRa(FSPI);
static SPISettings LORA_SPISET(4000000, MSBFIRST, SPI_MODE0);
static SPISettings RAW_SPISET(4000000, MSBFIRST, SPI_MODE0);

static SX1262 lora(new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY, spiLoRa, LORA_SPISET));

static const uint8_t OPC_GET_STATUS     = 0xC0;
static const uint8_t OPC_SET_STANDBY_RC = 0x80;
static const uint8_t NOP                = 0x00;

bool LoRaDriver::waitBusyLow(uint32_t tout_ms) {
  uint32_t t0 = millis();
  while (digitalRead(PIN_LORA_BUSY) == HIGH) {
    if (millis() - t0 > tout_ms) return false;
    delay(1);
  }
  return true;
}

bool LoRaDriver::sx1262TransferCmd(uint8_t cmd, uint8_t* rx, size_t rxLen) {
  if (!waitBusyLow()) return false;

  digitalWrite(PIN_LORA_NSS, LOW);
  spiLoRa.beginTransaction(RAW_SPISET);
  spiLoRa.transfer(cmd);
  for (size_t i = 0; i < rxLen; ++i) rx[i] = spiLoRa.transfer(NOP);
  spiLoRa.endTransaction();
  digitalWrite(PIN_LORA_NSS, HIGH);
  return true;
}

bool LoRaDriver::sx1262GetStatus(uint8_t& status) {
  return sx1262TransferCmd(OPC_GET_STATUS, &status, 1);
}

bool LoRaDriver::sx1262SetStandbyRc() {
  uint8_t dummy = 0;
  return sx1262TransferCmd(OPC_SET_STANDBY_RC, &dummy, 1);
}

void LoRaDriver::hardReset() {
  pinMode(PIN_LORA_RST, OUTPUT);
  digitalWrite(PIN_LORA_RST, LOW);
  delay(10);
  digitalWrite(PIN_LORA_RST, HIGH);
  delay(10);
}

bool LoRaDriver::bringupAndConfig() {
  spiLoRa.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);
  delay(5);

  int st = lora.begin(RF_FREQ_MHZ, RF_BW_KHZ, RF_SF, RF_CR, RF_SYNCWORD, RF_POWER_DBM);
  if (st != RADIOLIB_ERR_NONE) return false;

  lora.setTCXO(1.8, 5000);
  lora.setCurrentLimit(100);
  lora.setPreambleLength(12);
  lora.setCRC(true);

  #if defined(RADIOLIB_SX126X_PACKET_EXPLICIT)
    lora.setPacketMode(RADIOLIB_SX126X_PACKET_EXPLICIT);
  #else
    lora.explicitHeader();
  #endif

  lora.setDio2AsRfSwitch(true);
  return true;
}

bool LoRaDriver::begin() {
  pinMode(PIN_LORA_NSS, OUTPUT);
  pinMode(PIN_LORA_DIO1, INPUT);
  pinMode(PIN_LORA_BUSY, INPUT);
  pinMode(PIN_LORA_RST, OUTPUT);
  digitalWrite(PIN_LORA_NSS, HIGH);

  hardReset();
  waitBusyLow(500);

  uint8_t stat = 0xFF;
  sx1262GetStatus(stat);
  sx1262SetStandbyRc();

  LORA_SPISET = SPISettings(4000000, MSBFIRST, SPI_MODE0);
  if (!bringupAndConfig()) {
    LORA_SPISET = SPISettings(1000000, MSBFIRST, SPI_MODE0);
    hardReset();
    waitBusyLow(500);
    ok_ = bringupAndConfig();
  } else {
    ok_ = true;
  }

  return ok_;
}

bool LoRaDriver::transmit(const TEL_PACKET& pkt) {
  if (!ok_) return false;
  int st = lora.transmit((uint8_t*)&pkt, sizeof(pkt));
  return st == RADIOLIB_ERR_NONE;
}