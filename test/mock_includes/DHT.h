// Mock DHT sensor library for native testing
#ifndef DHT_H
#define DHT_H

#define DHT11 11
#define DHT22 22
#define DHT21 21

class DHT {
public:
    DHT(uint8_t pin, uint8_t type, uint8_t count = 6) {
        (void)pin; (void)type; (void)count;
    }
    void begin() {}
    float readTemperature(bool S = false) { (void)S; return 23.5f; }
    float readHumidity() { return 55.0f; }
    float computeHeatIndex(float temperature, float humidity, bool isFahrenheit = true) {
        (void)temperature; (void)humidity; (void)isFahrenheit;
        return 24.0f;
    }
};

#endif // DHT_H
