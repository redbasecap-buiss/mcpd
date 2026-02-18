// Mock Adafruit NeoPixel library for native testing
#ifndef ADAFRUIT_NEOPIXEL_H
#define ADAFRUIT_NEOPIXEL_H

#include <cstdint>

#define NEO_GRB  0x01
#define NEO_KHZ800 0x02

class Adafruit_NeoPixel {
public:
    Adafruit_NeoPixel(uint16_t n = 0, int16_t pin = -1, uint8_t type = NEO_GRB + NEO_KHZ800)
        : _numPixels(n), _pin(pin) { (void)type; }
    void begin() {}
    void show() {}
    void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
        (void)n; (void)r; (void)g; (void)b;
    }
    void setPixelColor(uint16_t n, uint32_t c) { (void)n; (void)c; }
    void setBrightness(uint8_t b) { _brightness = b; }
    uint8_t getBrightness() const { return _brightness; }
    void fill(uint32_t c = 0, uint16_t first = 0, uint16_t count = 0) {
        (void)c; (void)first; (void)count;
    }
    void clear() {}
    uint16_t numPixels() const { return _numPixels; }
    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
    void updateLength(uint16_t n) { _numPixels = n; }
    void setPin(int16_t p) { _pin = p; }
private:
    uint16_t _numPixels = 0;
    int16_t _pin = -1;
    uint8_t _brightness = 255;
};

#endif // ADAFRUIT_NEOPIXEL_H
