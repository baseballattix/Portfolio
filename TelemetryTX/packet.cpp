#include "packet.h"
#include "GPS.h"
#include "baro.h"
#include "IMU.h"
#include "AS.h"

#include <math.h>

uint16_t crc16_ibm(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
    }
  }
  return crc;
}

int16_t clamp_i16(long v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return (int16_t)v;
}

void buildTelemetryPacket(
  TEL_PACKET& p,
  uint16_t seq,
  uint32_t now,
  const GpsData& gps,
  const BaroData& baro,
  const ImuData& imu,
  const AirspeedData& as
) {
  p = {};

  p.seq     = seq;
  p.t_ms    = now;

  p.lat_e7       = gps.lat_e7;
  p.lon_e7       = gps.lon_e7;
  p.alt_gnss_cm  = gps.alt_gnss_cm;
  p.sats         = gps.sats;

  p.pres_Pa     = isnan(baro.pres_hPa)   ? 0 : (uint32_t)lround(baro.pres_hPa * 100.0f);
  p.alt_baro_cm = isnan(baro.alt_baro_m) ? 0 : (int32_t)lround(baro.alt_baro_m * 100.0f);
  p.temp_cX100  = isnan(baro.tempC)      ? 0 : (int16_t)lround(baro.tempC * 100.0f);

  p.ax_mg      = imu.accelValid ? clamp_i16(lroundf(imu.ax * 1000.0f)) : 0;
  p.ay_mg      = imu.accelValid ? clamp_i16(lroundf(imu.ay * 1000.0f)) : 0;
  p.az_mg      = imu.accelValid ? clamp_i16(lroundf(imu.az * 1000.0f)) : 0;

  p.gx_dpsX100 = imu.gyroValid ? clamp_i16(lroundf(imu.gx * 100.0f)) : 0;
  p.gy_dpsX100 = imu.gyroValid ? clamp_i16(lroundf(imu.gy * 100.0f)) : 0;
  p.gz_dpsX100 = imu.gyroValid ? clamp_i16(lroundf(imu.gz * 100.0f)) : 0;

  p.mphx10 = as.valid ? clamp_i16(lroundf(as.airspeed_mph * 10.0f)) : 0;

  p.crc16 = crc16_ibm(reinterpret_cast<const uint8_t*>(&p), sizeof(p) - 2);
 
}
void printCsv(const TEL_PACKET& p) {
  Serial.printf(
    "TX_CSV,%u,%lu,%.7f,%.7f,%.2f,%.2f,%.2f,%.2f,%u,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%.1f\n",
    p.seq, (unsigned long)p.t_ms,
    (float)p.lat_e7 / 1e7f, (float)p.lon_e7 / 1e7f,
    (float)p.alt_gnss_cm / 100.0f, p.pres_Pa / 100.0f,
    (float)p.alt_baro_cm / 100.0f, (float)p.temp_cX100 / 100.0f,
    p.sats,
    p.ax_mg / 1000.0f, p.ay_mg / 1000.0f, p.az_mg / 1000.0f,
    p.gx_dpsX100 / 100.0f, p.gy_dpsX100 / 100.0f, p.gz_dpsX100 / 100.0f,
    p.mphx10 / 10.0f
  );
}
