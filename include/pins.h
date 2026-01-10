#pragma once

#if defined(ESP8266)
  #include <Arduino.h>
  #define PIN_I2C_SDA D2
  #define PIN_I2C_SCL D1
#elif defined(ESP32)
  #include <Arduino.h>
  // ESP32-C3 SuperMini Empfehlung
  #define PIN_I2C_SDA 8
  #define PIN_I2C_SCL 9
#else
  #error "Unknown platform"
#endif
