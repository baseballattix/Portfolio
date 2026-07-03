#pragma once
#include <Arduino.h>

struct GpsData;
struct BaroData;
struct ImuData;
struct AirspeedData;

#pragma pack(push,1)
struct TEL_PACKET {
  uint16_t seq;
  uint32_t t_ms;
  int32_t  lat_e7;
  int32_t  lon_e7;
  int32_t  alt_gnss_cm;
  uint8_t  sats;
  uint32_t pres_Pa;
  int32_t  alt_baro_cm;
  int16_t  temp_cX100;
  int16_t  ax_mg;
  int16_t  ay_mg;
  int16_t  az_mg;
  int16_t  gx_dpsX100;
  int16_t  gy_dpsX100;
  int16_t  gz_dpsX100;
  uint16_t mphx10;
  uint16_t crc16;
};
#pragma pack(pop)

uint16_t crc16_ibm(const uint8_t* data, size_t len);
int16_t clamp_i16(long v);

void buildTelemetryPacket(
  TEL_PACKET& p,
  uint16_t seq,
  uint32_t now,
  const GpsData& gps,
  const BaroData& baro,
  const ImuData& imu,
  const AirspeedData& as 
);

void printCsv(const TEL_PACKET& p);