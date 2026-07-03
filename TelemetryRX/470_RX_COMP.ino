#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <WiFi.h>
#include <WiFiUdp.h>

extern "C" {
  #include "esp_wifi.h"
}
// ---------- Wifi Config -------------
const char* SSID = "AttixPhone";
const char* PASS = "REDACTED";

IPAddress laptopIP(REDACTED); //change for your laptop IP
const uint16_t laptopPort = REDACTED;

WiFiUDP udp;
uint32_t counter = 0;

// ---------- Wifi Connect Function ----------
static bool connectWiFi(uint32_t timeout_ms = 20000) {
  Serial.println("[WiFi] connecting...");

  WiFi.disconnect(true, true);
  delay(300);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setHostname("tbeamRX");

  WiFi.begin(SSID, PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeout_ms) {
    delay(250);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    udp.begin(0);                 // open a local UDP port
    return true;
  } else {
    Serial.println("\n[WiFi] connect TIMEOUT");
    return false;
  }
}
// ---------- LoRa params (must match TX) ----------
#define RF_FREQ_MHZ   915.0
#define RF_BW_KHZ     125.0
#define RF_SF         8
#define RF_CR         5
#define RF_SYNCWORD   0x12
#define RF_POWER_DBM  14   // TX-side; RX ignores power

// ---------- T-Beam Supreme (ESP32-S3) SX1262 pins ----------
#define LORA_SCK   12
#define LORA_MISO  13
#define LORA_MOSI  11
#define LORA_NSS   10     // CS
#define LORA_RST   5
#define LORA_DIO1  1
#define LORA_BUSY  4

// --------- Button For Event Markers ------------
#define EVENT_BTN 3
bool lastPressed = false;

// ---------- Telemetry Packet ----------
#pragma pack(push,1)
struct TelemPacket {
  uint16_t seq;
  uint32_t t_ms;
  int32_t  lat_e7;      // deg * 1e7
  int32_t  lon_e7;      // deg * 1e7
  int32_t  alt_gnss_cm; // m * 100
  uint8_t  sats;
  uint32_t pres_Pa;     // Pa
  int32_t  alt_baro_cm; // m * 100
  int16_t  temp_cX100;  // C * 100

  int16_t  ax_mg;       // g * 1000
  int16_t  ay_mg;
  int16_t  az_mg;
  int16_t  gx_dpsX100;  // dps * 100
  int16_t  gy_dpsX100;
  int16_t  gz_dpsX100;

  uint16_t mphx10;       //mph * 10
  uint16_t crc16;       // CRC-16/IBM
};
#pragma pack(pop)

// ---------- CRC-16/IBM ----------
static uint16_t crc16_ibm(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
    }
  }
  return crc;
}

// ---------- LoRa: explicit SPI settings (mirror TX) ----------
SPIClass spiLoRa(FSPI);
static SPISettings LORA_SPISET(4000000, MSBFIRST, SPI_MODE0);
SX1262 lora = SX1262(new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, spiLoRa, LORA_SPISET));

// Optional: same reset/BUSY helpers as TX
static void hardResetLoRa() {
  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, LOW);
  delay(10);
  digitalWrite(LORA_RST, HIGH);
  delay(10);
}
static bool waitBusyLow(uint32_t tout_ms = 500) {
  uint32_t t0 = millis();
  while (digitalRead(LORA_BUSY) == HIGH) {
    if (millis() - t0 > tout_ms) return false;
    delay(1);
  }
  return true;
}
// Helpers for Status Monitor
TelemPacket latestPkt;
bool haveLatestPkt = false;
uint32_t lastPacketMillis = 0;

void printStatusLine() {
  if (!haveLatestPkt) {
    Serial.println("STATUS,sats=0,nose=UNKNOWN,radio=LOST,temp=nan");
    return;
  }
  uint32_t age = millis() - lastPacketMillis;
  const char* radioHealth =
    age > 2000 ? "LOST" :
    age > 750  ? "DEGRADED" :
                 "OK";
  // --- nose detection (simple version using accel) ---
  double ax_y = (double)latestPkt.ay_mg / 1000.0;
  const char* noseStatus =
    ax_y < -0.60 ? "CRITICAL" : "OK";   // flip sign if wrong direction
  // --- temp formatting (NO FLOAT VERSION) ---
  int tempWhole = latestPkt.temp_cX100 / 100;
  int tempFrac  = abs(latestPkt.temp_cX100 % 100);
  //Print Status Report
  Serial.printf(
    "STATUS,sats=%u,nose=%s,radio=%s,temp=%d.%02d\n",
    latestPkt.sats,
    noseStatus,
    radioHealth,
    tempWhole,
    tempFrac
  );
}

void setup() {
  Serial.begin(115200);
  delay(300);
   //connectWiFi(); //---comment out when using serial

  const char* csvHeader = "seq,t_ms,lat,lon,alt_gnss_m,pres_hPa,alt_baro_m,temp_C,sats,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,mph,event";

  if (WiFi.status() == WL_CONNECTED) {
  udp.beginPacket(laptopIP, laptopPort);
  udp.write((const uint8_t*)csvHeader, strlen(csvHeader));
  udp.endPacket();
  }

  Serial.println("\n[RX] LoRa init (V2 only)...");

  pinMode(EVENT_BTN, INPUT_PULLUP);
  pinMode(LORA_NSS,  OUTPUT);
  pinMode(LORA_DIO1, INPUT);
  pinMode(LORA_BUSY, INPUT);
  pinMode(LORA_RST,  OUTPUT);
  digitalWrite(LORA_NSS, HIGH);

  hardResetLoRa();
  waitBusyLow(500);

  spiLoRa.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  delay(5);

  int st = lora.begin(RF_FREQ_MHZ, RF_BW_KHZ, RF_SF, RF_CR, RF_SYNCWORD, RF_POWER_DBM);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.print("[RX] begin failed: "); Serial.println(st);
    LORA_SPISET = SPISettings(1000000, MSBFIRST, SPI_MODE0);
    hardResetLoRa();
    waitBusyLow(500);
    st = lora.begin(RF_FREQ_MHZ, RF_BW_KHZ, RF_SF, RF_CR, RF_SYNCWORD, RF_POWER_DBM);
  }
  if (st != RADIOLIB_ERR_NONE) {
    Serial.print("[RX] begin failed again: "); Serial.println(st);
    while (1) delay(1000);
  }

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

  Serial.println("CSV_HEADER,seq,t_ms,lat,lon,alt_gnss_m,pres_hPa,alt_baro_m,temp_C,sats,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,mph,event");
}

void loop() {
  // update status checker
  static uint32_t lastStatusPrint = 0;
  if (millis() - lastStatusPrint >= 500) {
    lastStatusPrint = millis();
    printStatusLine();
  }

  int event = 0;
  bool pressed = (digitalRead(EVENT_BTN) == LOW);

  if (pressed && !lastPressed) {
    event = 1;
  }

  lastPressed = pressed;

  uint8_t buf[sizeof(TelemPacket)];
  int st = lora.receive(buf, sizeof(buf));
  if (st != RADIOLIB_ERR_NONE) return;

  int len = lora.getPacketLength();
  if (len < (int)sizeof(TelemPacket)) {
    // Serial.printf("[RX] short packet (%d bytes)\n", len);
    return;
  }

  TelemPacket p;
  memcpy(&p, buf, sizeof(p));


  uint16_t calc = crc16_ibm(reinterpret_cast<uint8_t*>(&p), sizeof(p) - 2);
  if (calc != p.crc16) {
    // Serial.printf("[RX] CRC FAIL calc=0x%04X pkt=0x%04X\n", calc, p.crc16);
    return;
  }

  // Update for Status 
  latestPkt = p;
  haveLatestPkt = true;
  lastPacketMillis = millis();

  // Unpack using double to avoid float quantization
  double lat    = (double)p.lat_e7 / 1e7;
  double lon    = (double)p.lon_e7 / 1e7;
  double alt_m  = (double)p.alt_gnss_cm / 100.0;
  double pres   = (double)p.pres_Pa / 100.0;
  double baro   = (double)p.alt_baro_cm / 100.0;
  double tempC  = (double)p.temp_cX100 / 100.0;

  double ax_g   = (double)p.ax_mg / 1000.0;
  double ay_g   = (double)p.ay_mg / 1000.0;
  double az_g   = (double)p.az_mg / 1000.0;

  double gx_dps = (double)p.gx_dpsX100 / 100.0;
  double gy_dps = (double)p.gy_dpsX100 / 100.0;
  double gz_dps = (double)p.gz_dpsX100 / 100.0;

  double mph    = (double)p.mphx10 / 10.0;

  float rssi = lora.getRSSI();
  float snr  = lora.getSNR();

// ---- Serial Transmit ------- comment out for WIfi

  Serial.printf(
  "CSV,%u,%lu,%.8lf,%.8lf,%.2lf,%.2lf,%.2lf,%.2lf,%u,%.3lf,%.3lf,%.3lf,%.2lf,%.2lf,%.2lf,%.1lf,%d\n",
  p.seq, (unsigned long)p.t_ms, lat, lon, alt_m, pres, baro, tempC, p.sats,
  ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps, mph, event
);


//---- WIFI Transmit ------ comment out for serial 

  char out[240];
snprintf(out, sizeof(out),
  "%u,%lu,%.8lf,%.8lf,%.2lf,%.2lf,%.2lf,%.2lf,%u,%.3lf,%.3lf,%.3lf,%.2lf,%.2lf,%.2lf,%.1lf,%d\n",
  p.seq, (unsigned long)p.t_ms, lat, lon, alt_m, pres, baro, tempC, p.sats,
  ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps, mph, event
);

  if (WiFi.status() == WL_CONNECTED) {
    udp.beginPacket(laptopIP, laptopPort);
    udp.write((const uint8_t*)out, strlen(out));
    udp.endPacket();
  }

}

