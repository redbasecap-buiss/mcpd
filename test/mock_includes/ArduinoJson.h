// Wrapper that sets up ArduinoJson for our mock String
#pragma once
#include "../arduino_mock.h"

// Enable Arduino String support (our mock provides a compatible String class)
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#define ARDUINOJSON_ENABLE_STD_STRING 1
#define ARDUINOJSON_ENABLE_STD_STREAM 0

// Include real ArduinoJson
#include "../ArduinoJson.h"
