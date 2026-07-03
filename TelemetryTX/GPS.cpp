#include "GPS.h"
#include "config.h"

#include <Arduino.h>
#include <MicroNMEA.h>

static char nmeaBuf[160];
static MicroNMEA nmea(nmeaBuf, sizeof(nmeaBuf));

bool GpsDriver::begin() {
  Serial1.begin(GPS_BAUD_RATE, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  delay(500);
  pruneToGgaOnly();
  setRateMs(200);
  ok_ = true;
  return true;
}

void GpsDriver::update() {
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();
    nmea.process(c);
  }
}

GpsData GpsDriver::read() const {
  GpsData out{};
  out.sats = nmea.getNumSatellites();

  if (nmea.isValid()) {
    out.valid = true;

    int32_t lat_e6 = (int32_t)nmea.getLatitude();
    int32_t lon_e6 = (int32_t)nmea.getLongitude();

    out.lat_e7 = lat_e6 * 10;
    out.lon_e7 = lon_e6 * 10;

    long alt_mm = 0;
    if (nmea.getAltitude(alt_mm)) {
      out.alt_gnss_cm = (int32_t)(alt_mm / 10);
    }
  }

  return out;
}

void GpsDriver::ubxCk(const uint8_t* data, uint16_t len, uint8_t& a, uint8_t& b) {
  a = 0;
  b = 0;
  for (uint16_t i = 0; i < len; i++) {
    a = (uint8_t)(a + data[i]);
    b = (uint8_t)(b + a);
  }
}

void GpsDriver::ubxSend(uint8_t cls, uint8_t id, const uint8_t* payload, uint16_t payloadLen) {
  uint8_t hdr[4] = { cls, id, (uint8_t)(payloadLen & 0xFF), (uint8_t)(payloadLen >> 8) };

  uint8_t ckA, ckB;
  ubxCk(hdr, 4, ckA, ckB);
  for (uint16_t i = 0; i < payloadLen; i++) {
    ckA = (uint8_t)(ckA + payload[i]);
    ckB = (uint8_t)(ckB + ckA);
  }

  Serial1.write(0xB5);
  Serial1.write(0x62);
  Serial1.write(hdr, 4);
  if (payloadLen) Serial1.write(payload, payloadLen);
  Serial1.write(ckA);
  Serial1.write(ckB);
  Serial1.flush();
}

void GpsDriver::setRateMs(uint16_t ms) {
  uint8_t p[6] = {
    (uint8_t)(ms & 0xFF), (uint8_t)(ms >> 8),
    0x01, 0x00,
    0x01, 0x00
  };
  ubxSend(0x06, 0x08, p, sizeof(p));
}

void GpsDriver::cfgMsgUart1(uint8_t msgClass, uint8_t msgId, uint8_t rateUART1) {
  uint8_t p[8] = { msgClass, msgId, 0x00, rateUART1, 0x00, 0x00, 0x00, 0x00 };
  ubxSend(0x06, 0x01, p, sizeof(p));
}

void GpsDriver::pruneToGgaOnly() {
  const uint8_t NMEA = 0xF0;
  uint8_t disable_ids[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x08, 0x0D };
  for (uint8_t id : disable_ids) cfgMsgUart1(NMEA, id, 0);
  cfgMsgUart1(NMEA, 0x00, 1);
}