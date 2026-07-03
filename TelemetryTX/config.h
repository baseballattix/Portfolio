#pragma once
#include <Arduino.h>

// ---------------- Build mode ----------------
// Expo/live telemetry mode:
//   ENABLE_LORA 1
//   ENABLE_SD   1
//
// Clean data collection mode:
//   ENABLE_LORA 0
//   ENABLE_SD   1
//
// Radio-only debug mode:
//   ENABLE_LORA 1
//   ENABLE_SD   0
#define ENABLE_LORA 0
#define ENABLE_SD   1
#define ENABLE_AIRSPEED 0  // 1 = read airspeed sensor, 0 = put zeroes in packet

// Printing every 10 Hz sample over Serial can add timing noise. Leave this off
// for clean SD-only data collection. Set to 1 only for debugging.
#define PRINT_EACH_SD_SAMPLE_TO_SERIAL 1

// ---------------- Pin map: T-Beam S3 Supreme (SX1262) ----------------
static const int PIN_I2C_SDA   = 17;
static const int PIN_I2C_SCL   = 18;

static const int PIN_GPS_RX    = 9;
static const int PIN_GPS_TX    = 8;
static const int PIN_GPS_PPS   = 6;
static const int PIN_GPS_EN    = 7;
static const uint32_t GPS_BAUD_RATE = 9600;

// PMU on Wire1
static const int PIN_PMU_SDA   = 42;
static const int PIN_PMU_SCL   = 41;

// LoRa
static const int PIN_LORA_SCK  = 12;
static const int PIN_LORA_MISO = 13;
static const int PIN_LORA_MOSI = 11;
static const int PIN_LORA_NSS  = 10;
static const int PIN_LORA_RST  = 5;
static const int PIN_LORA_DIO1 = 1;
static const int PIN_LORA_BUSY = 4;

// IMU
static const int PIN_IMU_CS    = 34;
static const int PIN_IMU_MOSI  = 35;
static const int PIN_IMU_SCK   = 36;
static const int PIN_IMU_MISO  = 37;

// card pins
static const int PIN_SD_CS  = 47;

// airspeed 
//static const int PIN_AS_SDA = 3;
//static const int PIN_AS_SCL = 21;
static const uint8_t AS_addr = 0x28;

// Radio params
#define RF_FREQ_MHZ   915.0
#define RF_BW_KHZ     125.0
#define RF_SF         8
#define RF_CR         5
#define RF_SYNCWORD   0x12
#define RF_POWER_DBM  14

// Rates
static const uint32_t TX_PERIOD_MS = 400;             // 2.5 Hz LoRa transmit rate
static const uint32_t SENSOR_SAMPLE_PERIOD_MS = 100;  // 10 Hz sensor snapshot + SD log rate

// BME initialization
static constexpr float SEALEVEL_hPa = 1023.0f;
