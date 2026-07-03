#include "write_SD.h"
#include "config.h"
#include "SPI_bus_manager.h"

#include <FS.h>
#include <SD.h>
#include <SPI.h>

static File logFile;

bool SdLogger::begin(const char* path, const char* header) {
  path_ = path;

  pinMode(PIN_IMU_CS, OUTPUT);
  pinMode(PIN_SD_CS, OUTPUT);

  digitalWrite(PIN_IMU_CS, HIGH);
  digitalWrite(PIN_SD_CS, HIGH);
  delay(20);

  ok_ = SD.begin(PIN_SD_CS, spiSensorBus, 4000000);
  if (!ok_) {
    Serial.println("[SD] begin failed");
    return false;
  }

  auto type = SD.cardType();
  if (type == CARD_NONE) {
    Serial.println("[SD] no card");
    ok_ = false;
    return false;
  }

  Serial.printf("[SD] card type = %u\n", (unsigned)type);

  if (header && !SD.exists(path_)) {
    File f = SD.open(path_, FILE_WRITE);
    if (!f) {
      Serial.println("[SD] failed creating file");
      ok_ = false;
      return false;
    }
    f.println(header);
    f.close();
  }

  logFile = SD.open(path_, FILE_APPEND);
  if (!logFile) {
    Serial.println("[SD] failed opening append file");
    ok_ = false;
    return false;
  }

  flushCounter_ = 0;
  return true;
}

bool SdLogger::appendLine(const String& line) {
  if (!ok_ || !logFile) return false;

  digitalWrite(PIN_IMU_CS, HIGH);
  digitalWrite(PIN_SD_CS, LOW);

  bool success = logFile.println(line);

  digitalWrite(PIN_SD_CS, HIGH);

  if (success && ++flushCounter_ >= 10) {
    logFile.flush();
    flushCounter_ = 0;
  }

  return success;
}

void SdLogger::flush() {
  if (logFile) logFile.flush();
}

void SdLogger::close() {
  if (logFile) {
    logFile.flush();
    logFile.close();
  }
}