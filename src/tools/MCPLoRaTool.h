/**
 * mcpd — Built-in LoRa Radio Tool
 *
 * Provides: lora_init, lora_send, lora_receive, lora_configure, lora_status, lora_sleep, lora_cad
 * Supports SX1276/SX1278 LoRa transceivers via SPI.
 *
 * NOTE: Requires LoRa library.
 *       Add to lib_deps: sandeepmistry/LoRa@^0.8
 */

#ifndef MCPD_LORA_TOOL_H
#define MCPD_LORA_TOOL_H

#include "../mcpd.h"

#ifdef ARDUINO
#include <SPI.h>
#endif

namespace mcpd {
namespace tools {

struct LoRaPins {
    int ss    = 18;
    int reset = 14;
    int dio0  = 26;
    int sck   = -1;
    int miso  = -1;
    int mosi  = -1;
};

struct LoRaPacket {
    String data;
    int rssi;
    float snr;
    int packetSize;
    unsigned long timestamp;
};

static const int LORA_RX_BUFFER_SIZE = 32;
static LoRaPacket _loraRxBuffer[LORA_RX_BUFFER_SIZE];
static volatile int _loraRxHead = 0;
static volatile int _loraRxTail = 0;
static bool _loraInitialized = false;
static unsigned long _loraTxCount = 0;
static unsigned long _loraRxCount = 0;
static long _loraFrequency = 868E6;
static int _loraTxPower = 17;
static int _loraSF = 7;
static long _loraBW = 125000;
static int _loraCR = 5;

class LoRaTool {
public:
    static void attach(Server& server, LoRaPins pins = LoRaPins()) {

        // lora_init
        server.addTool("lora_init",
            "Initialize LoRa radio transceiver. Frequency in Hz (433E6, 868E6, 915E6).",
            R"j({"type":"object","properties":{"frequency":{"type":"number","description":"Carrier frequency in Hz (default: 868000000)"},"txPower":{"type":"integer","description":"Transmit power 2-20 dBm (default: 17)"},"spreadingFactor":{"type":"integer","description":"Spreading factor 6-12 (default: 7)"},"bandwidth":{"type":"number","description":"Bandwidth in Hz (default: 125000)"},"codingRate":{"type":"integer","description":"Coding rate 5-8 (default: 5)"},"syncWord":{"type":"integer","description":"Sync word 0x00-0xFF (default: 0x12)"},"preamble":{"type":"integer","description":"Preamble length (default: 8)"},"enableCrc":{"type":"boolean","description":"Enable CRC (default: true)"}}})j",
            [pins](const JsonObject& args) -> String {
                JsonDocument doc;
#ifdef ESP32
                _loraFrequency = args["frequency"] | 868000000L;
                _loraTxPower = args["txPower"] | 17;
                _loraSF = args["spreadingFactor"] | 7;
                _loraBW = args["bandwidth"] | 125000L;
                _loraCR = args["codingRate"] | 5;
                int syncWord = args["syncWord"] | 0x12;
                int preamble = args["preamble"] | 8;
                bool enableCrc = args["enableCrc"] | true;

                if (pins.sck >= 0 && pins.miso >= 0 && pins.mosi >= 0) {
                    SPI.begin(pins.sck, pins.miso, pins.mosi, pins.ss);
                }

                LoRa.setPins(pins.ss, pins.reset, pins.dio0);

                if (!LoRa.begin(_loraFrequency)) {
                    doc["error"] = "LoRa init failed";
                } else {
                    LoRa.setTxPower(_loraTxPower);
                    LoRa.setSpreadingFactor(_loraSF);
                    LoRa.setSignalBandwidth(_loraBW);
                    LoRa.setCodingRate4(_loraCR);
                    LoRa.setSyncWord(syncWord);
                    LoRa.setPreambleLength(preamble);
                    if (enableCrc) LoRa.enableCrc(); else LoRa.disableCrc();

                    LoRa.onReceive([](int packetSize) {
                        int next = (_loraRxHead + 1) % LORA_RX_BUFFER_SIZE;
                        if (next != _loraRxTail) {
                            LoRaPacket& pkt = _loraRxBuffer[_loraRxHead];
                            pkt.data = "";
                            pkt.packetSize = packetSize;
                            pkt.rssi = LoRa.packetRssi();
                            pkt.snr = LoRa.packetSnr();
                            pkt.timestamp = millis();
                            for (int i = 0; i < packetSize; i++) {
                                pkt.data += (char)LoRa.read();
                            }
                            _loraRxHead = next;
                            _loraRxCount++;
                        }
                    });
                    LoRa.receive();

                    _loraInitialized = true;
                    doc["ok"] = true;
                    doc["frequency"] = _loraFrequency;
                    doc["txPower"] = _loraTxPower;
                    doc["spreadingFactor"] = _loraSF;
                    doc["bandwidth"] = _loraBW;
                }
#else
                doc["error"] = "LoRa only supported on ESP32/RP2040/STM32";
#endif
                String result;
                serializeJson(doc, result);
                return result;
            });

        // lora_send
        server.addTool("lora_send",
            "Send a LoRa packet. Returns after transmission complete.",
            R"j({"type":"object","properties":{"data":{"type":"string","description":"Data to send (max 255 bytes)"},"hex":{"type":"boolean","description":"Treat data as hex string (default: false)"}},"required":["data"]})j",
            [](const JsonObject& args) -> String {
                JsonDocument doc;
#ifdef ESP32
                if (!_loraInitialized) {
                    doc["error"] = "LoRa not initialized";
                } else {
                    const char* data = args["data"] | "";
                    bool hex = args["hex"] | false;

                    LoRa.idle();
                    LoRa.beginPacket();

                    if (hex) {
                        String hexStr = data;
                        for (size_t i = 0; i + 1 < hexStr.length(); i += 2) {
                            char buf[3] = { hexStr[i], hexStr[i+1], 0 };
                            LoRa.write((uint8_t)strtol(buf, nullptr, 16));
                        }
                        doc["bytesSent"] = (int)(hexStr.length() / 2);
                    } else {
                        LoRa.print(data);
                        doc["bytesSent"] = (int)strlen(data);
                    }

                    int result = LoRa.endPacket();
                    _loraTxCount++;
                    LoRa.receive();

                    doc["ok"] = (result == 1);
                    doc["totalSent"] = _loraTxCount;
                }
#else
                doc["error"] = "LoRa only supported on ESP32/RP2040/STM32";
#endif
                String result;
                serializeJson(doc, result);
                return result;
            });

        // lora_receive
        server.addTool("lora_receive",
            "Read received LoRa packets from the ring buffer.",
            R"j({"type":"object","properties":{"maxPackets":{"type":"integer","description":"Max packets to return (default: all)"}}})j",
            [](const JsonObject& args) -> String {
                JsonDocument doc;
#ifdef ESP32
                int maxPkts = args["maxPackets"] | LORA_RX_BUFFER_SIZE;
                JsonArray packets = doc["packets"].to<JsonArray>();
                int count = 0;

                while (_loraRxTail != _loraRxHead && count < maxPkts) {
                    LoRaPacket& pkt = _loraRxBuffer[_loraRxTail];
                    JsonObject p = packets.add<JsonObject>();
                    p["data"] = pkt.data;
                    p["rssi"] = pkt.rssi;
                    p["snr"] = pkt.snr;
                    p["size"] = pkt.packetSize;
                    p["timestampMs"] = pkt.timestamp;
                    _loraRxTail = (_loraRxTail + 1) % LORA_RX_BUFFER_SIZE;
                    count++;
                }
                doc["count"] = count;
                doc["totalReceived"] = _loraRxCount;
#else
                doc["error"] = "LoRa only supported on ESP32/RP2040/STM32";
#endif
                String result;
                serializeJson(doc, result);
                return result;
            });

        // lora_configure
        server.addTool("lora_configure",
            "Change LoRa radio parameters without re-initialization.",
            R"j({"type":"object","properties":{"txPower":{"type":"integer","description":"Transmit power 2-20 dBm"},"spreadingFactor":{"type":"integer","description":"Spreading factor 6-12"},"bandwidth":{"type":"number","description":"Bandwidth in Hz"},"codingRate":{"type":"integer","description":"Coding rate 5-8"},"enableCrc":{"type":"boolean","description":"Enable/disable CRC"}}})j",
            [](const JsonObject& args) -> String {
                JsonDocument doc;
#ifdef ESP32
                if (!_loraInitialized) {
                    doc["error"] = "LoRa not initialized";
                } else {
                    if (args.containsKey("txPower")) { _loraTxPower = args["txPower"]; LoRa.setTxPower(_loraTxPower); doc["txPower"] = _loraTxPower; }
                    if (args.containsKey("spreadingFactor")) { _loraSF = args["spreadingFactor"]; LoRa.setSpreadingFactor(_loraSF); doc["spreadingFactor"] = _loraSF; }
                    if (args.containsKey("bandwidth")) { _loraBW = args["bandwidth"]; LoRa.setSignalBandwidth(_loraBW); doc["bandwidth"] = _loraBW; }
                    if (args.containsKey("codingRate")) { _loraCR = args["codingRate"]; LoRa.setCodingRate4(_loraCR); doc["codingRate"] = _loraCR; }
                    if (args.containsKey("enableCrc")) { bool crc = args["enableCrc"]; if (crc) LoRa.enableCrc(); else LoRa.disableCrc(); doc["crc"] = crc; }
                    doc["ok"] = true;
                }
#else
                doc["error"] = "LoRa only supported on ESP32/RP2040/STM32";
#endif
                String result;
                serializeJson(doc, result);
                return result;
            });

        // lora_status
        server.addTool("lora_status",
            "Get LoRa radio status, configuration, and packet statistics.",
            R"j({"type":"object","properties":{}})j",
            [](const JsonObject& args) -> String {
                JsonDocument doc;
#ifdef ESP32
                if (!_loraInitialized) {
                    doc["initialized"] = false;
                } else {
                    doc["initialized"] = true;
                    doc["frequency"] = _loraFrequency;
                    doc["txPower"] = _loraTxPower;
                    doc["spreadingFactor"] = _loraSF;
                    doc["bandwidth"] = _loraBW;
                    doc["codingRate"] = _loraCR;
                    doc["totalSent"] = _loraTxCount;
                    doc["totalReceived"] = _loraRxCount;
                    int buffered = (_loraRxHead - _loraRxTail + LORA_RX_BUFFER_SIZE) % LORA_RX_BUFFER_SIZE;
                    doc["bufferedPackets"] = buffered;
                    doc["rssi"] = LoRa.packetRssi();
                    doc["snr"] = LoRa.packetSnr();
                }
#else
                doc["error"] = "LoRa only supported on ESP32/RP2040/STM32";
#endif
                String result;
                serializeJson(doc, result);
                return result;
            });

        // lora_sleep
        server.addTool("lora_sleep",
            "Put LoRa radio into sleep mode or wake it up.",
            R"j({"type":"object","properties":{"sleep":{"type":"boolean","description":"true=sleep, false=wake (default: true)"}}})j",
            [](const JsonObject& args) -> String {
                JsonDocument doc;
#ifdef ESP32
                if (!_loraInitialized) {
                    doc["error"] = "LoRa not initialized";
                } else {
                    bool sleep = args["sleep"] | true;
                    if (sleep) { LoRa.sleep(); doc["mode"] = "sleep"; }
                    else { LoRa.idle(); LoRa.receive(); doc["mode"] = "receive"; }
                    doc["ok"] = true;
                }
#else
                doc["error"] = "LoRa only supported on ESP32/RP2040/STM32";
#endif
                String result;
                serializeJson(doc, result);
                return result;
            });

        // lora_cad
        server.addTool("lora_cad",
            "Perform Channel Activity Detection (CAD) to check if the channel is in use.",
            R"j({"type":"object","properties":{}})j",
            [](const JsonObject& args) -> String {
                JsonDocument doc;
#ifdef ESP32
                if (!_loraInitialized) {
                    doc["error"] = "LoRa not initialized";
                } else {
                    LoRa.idle();
                    int rssi = LoRa.packetRssi();
                    doc["rssi"] = rssi;
                    doc["channelActive"] = (rssi > -100);
                    LoRa.receive();
                    doc["ok"] = true;
                }
#else
                doc["error"] = "LoRa only supported on ESP32/RP2040/STM32";
#endif
                String result;
                serializeJson(doc, result);
                return result;
            });
    }
};

} // namespace tools

inline void addLoRaTools(Server& server, tools::LoRaPins pins = tools::LoRaPins()) {
    tools::LoRaTool::attach(server, pins);
}

} // namespace mcpd

#endif // MCPD_LORA_TOOL_H
