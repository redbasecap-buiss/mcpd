# Built-in Tools Reference

mcpd ships with **105 tools** across **44 modules**.

Each module is a header-only include. Register tools with the inline helper or class method.

## Quick Reference

| Module | Helper | Tools | Description |
|--------|--------|------:|-------------|
| `MCPADCTool.h` | — | 4 | Built-in ADC (Analog-to-Digital Converter) Tool |
| `MCPAnalogWatchTool.h` | `addAnalogWatchTools()` | 0 | Analog Watchdog Tool |
| `MCPBatteryTool.h` | `addBatteryTools()` | 0 | Built-in Battery Monitor Tool |
| `MCPBuzzerTool.h` | `addBuzzerTools()` | 3 | Built-in Buzzer/Tone Tool |
| `MCPCANTool.h` | `addCANTools()` | 0 | CAN Bus Tool |
| `MCPCameraTool.h` | `addCameraTools()` | 0 | ESP32-CAM Camera Tool |
| `MCPDACTool.h` | `addDACTools()` | 3 | Built-in DAC (Digital-to-Analog Converter) Tool |
| `MCPDHTTool.h` | — | 1 | Built-in DHT Sensor Tool |
| `MCPEEPROMTool.h` | `addEEPROMTools()` | 8 | EEPROM Tool |
| `MCPESPNowTool.h` | `addESPNowTools()` | 0 | ESP-NOW Peer-to-Peer Communication Tool |
| `MCPEncoderTool.h` | — | 0 | Rotary Encoder Tool |
| `MCPEthernetTool.h` | `addEthernetTools()` | 4 | Built-in Ethernet Tool |
| `MCPFilesystemTool.h` | — | 0 | Filesystem Built-in Tool |
| `MCPGPIOTool.h` | — | 0 | Built-in GPIO Tools |
| `MCPGPSTool.h` | `addGPSTools()` | 5 | Built-in GPS Tool |
| `MCPI2CScannerTool.h` | — | 0 | I2C Bus Scanner Tool |
| `MCPI2CTool.h` | — | 3 | Built-in I2C Tools |
| `MCPI2SAudioTool.h` | `addI2SAudioTools()` | 6 | Built-in I2S Audio Tool |
| `MCPIRTool.h` | `addIRTools()` | 3 | Built-in IR Remote Tool |
| `MCPInterruptTool.h` | `addInterruptTools()` | 0 | Interrupt Monitor Tool |
| `MCPLCDTool.h` | `addLCDTools()` | 6 | Built-in LCD Display Tool |
| `MCPLoRaTool.h` | `addLoRaTools()` | 7 | Built-in LoRa Radio Tool |
| `MCPMQTTTool.h` | — | 5 | Built-in MQTT Tools |
| `MCPModbusTool.h` | — | 11 | Modbus RTU/TCP Tool |
| `MCPNVSTool.h` | `addNVSTools()` | 5 | Built-in NVS (Non-Volatile Storage) Tool |
| `MCPNeoPixelTool.h` | — | 4 | Built-in NeoPixel Tool |
| `MCPOTATool.h` | — | 0 | OTA (Over-The-Air) Update Tool |
| `MCPOneWireTool.h` | `addOneWireTools()` | 0 | OneWire / DS18B20 Temperature Sensor Tool |
| `MCPPWMTool.h` | — | 2 | Built-in PWM Tool |
| `MCPPowerTool.h` | — | 0 | Power Management Tool |
| `MCPPulseCounterTool.h` | — | 0 | Pulse Counter Tool |
| `MCPRTCTool.h` | `addRTCTools()` | 5 | Built-in RTC (Real-Time Clock) Tool |
| `MCPRelayTool.h` | `addRelayTools()` | 6 | Built-in Relay Control Tool |
| `MCPSDCardTool.h` | `addSDCardTools()` | 0 | Built-in SD Card Tool |
| `MCPSPITool.h` | — | 2 | Built-in SPI Tools |
| `MCPServoTool.h` | — | 2 | Built-in Servo Tool |
| `MCPStepperTool.h` | — | 0 | Stepper Motor Tool |
| `MCPSystemTool.h` | — | 1 | Built-in System Info Tool |
| `MCPTimerTool.h` | — | 0 | Hardware Timer Tool |
| `MCPTouchTool.h` | `addTouchTools()` | 0 | Capacitive Touch Sensor Tool (ESP32) |
| `MCPUARTTool.h` | — | 4 | Built-in UART (Serial Communication) Tool |
| `MCPUltrasonicTool.h` | `addUltrasonicTools()` | 3 | Built-in Ultrasonic Distance Sensor Tool (HC-SR04 / JSN-SR04T) |
| `MCPWatchdogTool.h` | — | 0 |  |
| `MCPWiFiTool.h` | — | 2 | Built-in WiFi Tools |

## Tool Details

### ADCTool
**File:** `src/tools/MCPADCTool.h`  
**Description:** Built-in ADC (Analog-to-Digital Converter) Tool  
**Tools:**

- `adc_read`
- `adc_read_voltage`
- `adc_read_multi`
- `adc_config`

### MCPAnalogWatchTool
**File:** `src/tools/MCPAnalogWatchTool.h`  
**Description:** Analog Watchdog Tool  
**Helper:** `mcpd::addAnalogWatchTools(server)`

**Tools:**


### BatteryTool
**File:** `src/tools/MCPBatteryTool.h`  
**Description:** Built-in Battery Monitor Tool  
**Helper:** `mcpd::addBatteryTools(server)`

**Tools:**


### BuzzerTool
**File:** `src/tools/MCPBuzzerTool.h`  
**Description:** Built-in Buzzer/Tone Tool  
**Helper:** `mcpd::addBuzzerTools(server)`

**Tools:**

- `buzzer_tone`
- `buzzer_melody`
- `buzzer_stop`

### MCPCANTool
**File:** `src/tools/MCPCANTool.h`  
**Description:** CAN Bus Tool  
**Helper:** `mcpd::addCANTools(server)`

**Tools:**


### MCPCameraTool
**File:** `src/tools/MCPCameraTool.h`  
**Description:** ESP32-CAM Camera Tool  
**Helper:** `mcpd::addCameraTools(server)`

**Tools:**


### DACTool
**File:** `src/tools/MCPDACTool.h`  
**Description:** Built-in DAC (Digital-to-Analog Converter) Tool  
**Helper:** `mcpd::addDACTools(server)`

**Tools:**

- `dac_write`
- `dac_write_voltage`
- `dac_status`

### DHTTool
**File:** `src/tools/MCPDHTTool.h`  
**Description:** Built-in DHT Sensor Tool  
**Tools:**

- `dht_read`

### MCPEEPROMTool
**File:** `src/tools/MCPEEPROMTool.h`  
**Description:** EEPROM Tool  
**Helper:** `mcpd::addEEPROMTools(server)`

**Tools:**

- `eeprom_read`
- `eeprom_write`
- `eeprom_read_int`
- `eeprom_write_int`
- `eeprom_read_string`
- `eeprom_write_string`
- `eeprom_clear`
- `eeprom_info`

### MCPESPNowTool
**File:** `src/tools/MCPESPNowTool.h`  
**Description:** ESP-NOW Peer-to-Peer Communication Tool  
**Helper:** `mcpd::addESPNowTools(server)`

**Tools:**


### MCPEncoderTool
**File:** `src/tools/MCPEncoderTool.h`  
**Description:** Rotary Encoder Tool  
**Tools:**


### MCPEthernetTool
**File:** `src/tools/MCPEthernetTool.h`  
**Description:** Built-in Ethernet Tool  
**Helper:** `mcpd::addEthernetTools(server)`

**Tools:**

- `ethernet_config`
- `ethernet_status`
- `ethernet_ping`
- `ethernet_dns_lookup`

### MCPFilesystemTool
**File:** `src/tools/MCPFilesystemTool.h`  
**Description:** Filesystem Built-in Tool  
**Tools:**


### GPIOTool
**File:** `src/tools/MCPGPIOTool.h`  
**Description:** Built-in GPIO Tools  
**Tools:**


### GPSTool
**File:** `src/tools/MCPGPSTool.h`  
**Description:** Built-in GPS Tool  
**Helper:** `mcpd::addGPSTools(server)`

**Tools:**

- `gps_read`
- `gps_satellites`
- `gps_speed`
- `gps_distance`
- `gps_status`

### MCPI2CScannerTool
**File:** `src/tools/MCPI2CScannerTool.h`  
**Description:** I2C Bus Scanner Tool  
**Tools:**


### I2CTool
**File:** `src/tools/MCPI2CTool.h`  
**Description:** Built-in I2C Tools  
**Tools:**

- `i2c_scan`
- `i2c_read`
- `i2c_write`

### I2SAudioTool
**File:** `src/tools/MCPI2SAudioTool.h`  
**Description:** Built-in I2S Audio Tool  
**Helper:** `mcpd::addI2SAudioTools(server)`

**Tools:**

- `i2s_init`
- `i2s_record`
- `i2s_play`
- `i2s_volume`
- `i2s_status`
- `i2s_stop`

### IRTool
**File:** `src/tools/MCPIRTool.h`  
**Description:** Built-in IR Remote Tool  
**Helper:** `mcpd::addIRTools(server)`

**Tools:**

- `ir_send`
- `ir_send_raw`
- `ir_status`

### MCPInterruptTool
**File:** `src/tools/MCPInterruptTool.h`  
**Description:** Interrupt Monitor Tool  
**Helper:** `mcpd::addInterruptTools(server)`

**Tools:**


### LCDTool
**File:** `src/tools/MCPLCDTool.h`  
**Description:** Built-in LCD Display Tool  
**Helper:** `mcpd::addLCDTools(server)`

**Tools:**

- `lcd_print`
- `lcd_clear`
- `lcd_setCursor`
- `lcd_backlight`
- `lcd_createChar`
- `lcd_status`

### LoRaTool
**File:** `src/tools/MCPLoRaTool.h`  
**Description:** Built-in LoRa Radio Tool  
**Helper:** `mcpd::addLoRaTools(server)`

**Tools:**

- `lora_init`
- `lora_send`
- `lora_receive`
- `lora_configure`
- `lora_status`
- `lora_sleep`
- `lora_cad`

### MQTTTool
**File:** `src/tools/MCPMQTTTool.h`  
**Description:** Built-in MQTT Tools  
**Tools:**

- `mqtt_connect`
- `mqtt_publish`
- `mqtt_subscribe`
- `mqtt_messages`
- `mqtt_status`

### ModbusTool
**File:** `src/tools/MCPModbusTool.h`  
**Description:** Modbus RTU/TCP Tool  
**Tools:**

- `modbus_init`
- `modbus_read_coils`
- `modbus_read_discrete`
- `modbus_read_holding`
- `modbus_read_input`
- `modbus_write_coil`
- `modbus_write_register`
- `modbus_write_coils`
- `modbus_write_registers`
- `modbus_scan`
- `modbus_status`

### NVSTool
**File:** `src/tools/MCPNVSTool.h`  
**Description:** Built-in NVS (Non-Volatile Storage) Tool  
**Helper:** `mcpd::addNVSTools(server)`

**Tools:**

- `nvs_set`
- `nvs_get`
- `nvs_delete`
- `nvs_list`
- `nvs_status`

### NeoPixelTool
**File:** `src/tools/MCPNeoPixelTool.h`  
**Description:** Built-in NeoPixel Tool  
**Tools:**

- `neopixel_set`
- `neopixel_fill`
- `neopixel_clear`
- `neopixel_brightness`

### MCPOTATool
**File:** `src/tools/MCPOTATool.h`  
**Description:** OTA (Over-The-Air) Update Tool  
**Tools:**


### MCPOneWireTool
**File:** `src/tools/MCPOneWireTool.h`  
**Description:** OneWire / DS18B20 Temperature Sensor Tool  
**Helper:** `mcpd::addOneWireTools(server)`

**Tools:**


### PWMTool
**File:** `src/tools/MCPPWMTool.h`  
**Description:** Built-in PWM Tool  
**Tools:**

- `pwm_write`
- `pwm_stop`

### MCPPowerTool
**File:** `src/tools/MCPPowerTool.h`  
**Description:** Power Management Tool  
**Tools:**


### MCPPulseCounterTool
**File:** `src/tools/MCPPulseCounterTool.h`  
**Description:** Pulse Counter Tool  
**Tools:**


### RTCTool
**File:** `src/tools/MCPRTCTool.h`  
**Description:** Built-in RTC (Real-Time Clock) Tool  
**Helper:** `mcpd::addRTCTools(server)`

**Tools:**

- `rtc_get`
- `rtc_set`
- `rtc_alarm`
- `rtc_temperature`
- `rtc_status`

### RelayTool
**File:** `src/tools/MCPRelayTool.h`  
**Description:** Built-in Relay Control Tool  
**Helper:** `mcpd::addRelayTools(server)`

**Tools:**

- `relay_set`
- `relay_get`
- `relay_toggle`
- `relay_pulse`
- `relay_all_off`
- `relay_status`

### SDCardTool
**File:** `src/tools/MCPSDCardTool.h`  
**Description:** Built-in SD Card Tool  
**Helper:** `mcpd::addSDCardTools(server)`

**Tools:**


### SPITool
**File:** `src/tools/MCPSPITool.h`  
**Description:** Built-in SPI Tools  
**Tools:**

- `spi_transfer`
- `spi_config`

### ServoTool
**File:** `src/tools/MCPServoTool.h`  
**Description:** Built-in Servo Tool  
**Tools:**

- `servo_write`
- `servo_detach`

### MCPStepperTool
**File:** `src/tools/MCPStepperTool.h`  
**Description:** Stepper Motor Tool  
**Tools:**


### SystemTool
**File:** `src/tools/MCPSystemTool.h`  
**Description:** Built-in System Info Tool  
**Tools:**

- `system_info`

### MCPTimerTool
**File:** `src/tools/MCPTimerTool.h`  
**Description:** Hardware Timer Tool  
**Tools:**


### MCPTouchTool
**File:** `src/tools/MCPTouchTool.h`  
**Description:** Capacitive Touch Sensor Tool (ESP32)  
**Helper:** `mcpd::addTouchTools(server)`

**Tools:**


### UARTTool
**File:** `src/tools/MCPUARTTool.h`  
**Description:** Built-in UART (Serial Communication) Tool  
**Tools:**

- `uart_config`
- `uart_write`
- `uart_read`
- `uart_available`

### UltrasonicTool
**File:** `src/tools/MCPUltrasonicTool.h`  
**Description:** Built-in Ultrasonic Distance Sensor Tool (HC-SR04 / JSN-SR04T)  
**Helper:** `mcpd::addUltrasonicTools(server)`

**Tools:**

- `distance_read`
- `distance_read_multi`
- `distance_config`

### WatchdogTool
**File:** `src/tools/MCPWatchdogTool.h`  
**Tools:**


### WiFiTool
**File:** `src/tools/MCPWiFiTool.h`  
**Description:** Built-in WiFi Tools  
**Tools:**

- `wifi_status`
- `wifi_scan`
