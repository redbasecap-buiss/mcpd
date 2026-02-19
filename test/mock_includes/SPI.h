/**
 * Mock SPI.h for native testing
 */
#ifndef MOCK_SPI_H
#define MOCK_SPI_H

#include <cstdint>

#define MSBFIRST 1
#define LSBFIRST 0
#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3

class SPISettings {
public:
    SPISettings() : _clock(1000000), _bitOrder(MSBFIRST), _dataMode(SPI_MODE0) {}
    SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode)
        : _clock(clock), _bitOrder(bitOrder), _dataMode(dataMode) {}
    uint32_t _clock;
    uint8_t _bitOrder;
    uint8_t _dataMode;
};

class SPIClass {
public:
    SPIClass() : _lastTransferByte(0), _transferCount(0) {}

    void begin() {}
    void end() {}
    void beginTransaction(SPISettings settings) { _settings = settings; }
    void endTransaction() {}

    uint8_t transfer(uint8_t data) {
        _lastTransferByte = data;
        _transferCount++;
        // Echo back XOR 0xFF for testing
        return data ^ 0xFF;
    }

    uint16_t transfer16(uint16_t data) {
        return ((uint16_t)transfer(data >> 8) << 8) | transfer(data & 0xFF);
    }

    // Test inspection
    uint8_t _lastTransferByte;
    uint32_t _transferCount;
    SPISettings _settings;
};

extern SPIClass SPI;

#ifndef MOCK_SPI_IMPL
SPIClass SPI;
#endif

#endif // MOCK_SPI_H
