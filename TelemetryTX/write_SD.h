#pragma once
#include <Arduino.h>

class SdLogger {
public:
  bool begin(const char* path, const char* header = nullptr);
  bool appendLine(const String& line);
  void flush();
  void close();
  bool ok() const { return ok_; }

private:
  const char* path_ = nullptr;
  bool ok_ = false;
  uint8_t flushCounter_ = 0;
};