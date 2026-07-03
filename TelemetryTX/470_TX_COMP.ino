#include <Arduino.h>

#include "config.h"
#include "power.h"
#include "GPS.h"
#include "baro.h"
#include "IMU.h"
#include "LORA.h"
#include "packet.h"
#include "SPI_bus_manager.h"
#include "write_SD.h"
#include "AS.h"

static PowerDriver power;
static GpsDriver gps;
static BaroDriver baro;
static ImuDriver imu;
static LoRaDriver radio;
static AirspeedDriver as;
static SdLogger sd;

struct LatestSensorFrame {
  GpsData gps;
  BaroData baro;
  ImuData imu;
  AirspeedData as;
  uint32_t sample_ms = 0;
  bool valid = false;
};

static LatestSensorFrame latest;

String packetToCsv(const TEL_PACKET& pkt) {
  String line;
  line.reserve(240);

  line += String(pkt.seq); line += ",";
  line += String(pkt.t_ms); line += ",";
  line += String(pkt.lat_e7 / 1e7, 7); line += ",";
  line += String(pkt.lon_e7 / 1e7, 7); line += ",";
  line += String(pkt.alt_gnss_cm / 100.0f, 2); line += ",";
  line += String(pkt.pres_Pa / 100.0f, 2); line += ",";
  line += String(pkt.alt_baro_cm / 100.0f, 2); line += ",";
  line += String(pkt.temp_cX100 / 100.0f, 2); line += ",";
  line += String(pkt.sats); line += ",";
  line += String(pkt.ax_mg / 1000.0f, 3); line += ",";
  line += String(pkt.ay_mg / 1000.0f, 3); line += ",";
  line += String(pkt.az_mg / 1000.0f, 3); line += ",";
  line += String(pkt.gx_dpsX100 / 100.0f, 2); line += ",";
  line += String(pkt.gy_dpsX100 / 100.0f, 2); line += ",";
  line += String(pkt.gz_dpsX100 / 100.0f, 2); line += ",";
  line += String(pkt.mphx10 / 10.0f, 1);

  return line;
}

const char* CSV_HEADER =
  "seq,t_ms,lat,lon,alt_gnss_m,pres_hPa,alt_baro_m,temp_C,sats,"
  "ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,mph";

static void sampleSensors(uint32_t now) {
#if ENABLE_AIRSPEED
  as.update();
#endif

  latest.gps = gps.read();
  latest.baro = baro.read();
  latest.imu = imu.read();
#if ENABLE_AIRSPEED
  latest.as = as.read();
#else
  latest.as = {};
#endif
  latest.sample_ms = now;
  latest.valid = true;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nT-Beam S3 Supreme: data logger / telemetry");

  Serial.printf("[MODE] ENABLE_SD=%d ENABLE_LORA=%d SAMPLE_PERIOD=%lu ms TX_PERIOD=%lu ms\n",
                ENABLE_SD, ENABLE_LORA,
                (unsigned long)SENSOR_SAMPLE_PERIOD_MS,
                (unsigned long)TX_PERIOD_MS);

  bool pmu_ok = power.begin();
  Serial.printf("[PMU] power rail %s\n", pmu_ok ? "ON" : "FAIL");
  delay(250);

  gps.begin();
  Serial.println("[GPS] Serial1 started for NMEA.");

  bool bme_ok = baro.begin();
  Serial.printf("[BME] %s\n", bme_ok ? "found" : "not found (will log/TX zeros)");

  initSensorBus();
  delay(20);

  bool imu_ok = imu.begin();
  Serial.printf("[IMU] begin() -> %s\n", imu_ok ? "OK" : "FAIL");

#if ENABLE_SD
  bool sd_ok = sd.begin("/Telemetry.csv", CSV_HEADER);
  Serial.printf("[SD] init %s\n", sd_ok ? "OK" : "FAIL");
#else
  Serial.println("[SD] disabled by config");
#endif

#if ENABLE_LORA
  bool lora_ok = radio.begin();
  Serial.printf("[LoRa] init %s\n", lora_ok ? "OK" : "failed");
#else
  Serial.println("[LoRa] disabled by config");
#endif

#if ENABLE_AIRSPEED
  bool as_ok = as.begin();
  Serial.printf("[AS] init %s\n", as_ok ? "OK" : "failed");
#else
  Serial.println("[AS] disabled by config; mphx10 will be zero");
#endif

  Serial.println("CSV,seq,t_ms,lat,lon,alt_gnss_m,pres_hPa,alt_baro_m,temp_C,sats,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,mph");
}

void loop() {
  static uint32_t lastSample = 0;
  static uint32_t lastTx = 0;
  static uint16_t sdSeq = 0;
  static uint16_t txSeq = 0;

  // Keep consuming GPS bytes as often as possible. This is not limited to the
  // SD or LoRa rate.
  gps.update();

  uint32_t now = millis();

#if ENABLE_SD
  // 10 Hz sensor snapshot + SD logging path.
  if (now - lastSample >= SENSOR_SAMPLE_PERIOD_MS) {
    lastSample = now;

    sampleSensors(now);

    TEL_PACKET sdPkt;
    buildTelemetryPacket(sdPkt, sdSeq++, latest.sample_ms,
                         latest.gps, latest.baro, latest.imu, latest.as);

    sd.appendLine(packetToCsv(sdPkt));

#if PRINT_EACH_SD_SAMPLE_TO_SERIAL
    Serial.print("SD_CSV,");
    Serial.println(packetToCsv(sdPkt));
#endif
  }
#else
  // If SD is disabled but LoRa is enabled, still refresh the latest sensor
  // frame at the normal sample period so LoRa has recent data.
  if (now - lastSample >= SENSOR_SAMPLE_PERIOD_MS) {
    lastSample = now;
    sampleSensors(now);
  }
#endif

#if ENABLE_LORA
  // Slower LoRa path. This uses the latest sampled sensor state, but gets its
  // own TX sequence number so the receiver does not see skipped packet numbers.
  if (now - lastTx >= TX_PERIOD_MS) {
    lastTx = now;

    if (!latest.valid) {
      sampleSensors(now);
    }

    TEL_PACKET txPkt;
    buildTelemetryPacket(txPkt, txSeq++, now,
                         latest.gps, latest.baro, latest.imu, latest.as);

    if (radio.transmit(txPkt)) {
      printCsv(txPkt);
    }
  }
#endif
}
