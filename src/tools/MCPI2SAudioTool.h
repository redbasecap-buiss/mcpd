/**
 * mcpd — Built-in I2S Audio Tool
 *
 * Provides: i2s_init, i2s_record, i2s_play, i2s_status, i2s_volume, i2s_stop
 * Supports I2S microphones (INMP441, SPH0645) and speakers (MAX98357A, PCM5102).
 *
 * NOTE: Uses ESP-IDF I2S driver on ESP32.
 */

#ifndef MCPD_I2S_AUDIO_TOOL_H
#define MCPD_I2S_AUDIO_TOOL_H

#include "../mcpd.h"

namespace mcpd {
namespace tools {

struct I2SPins {
    int bck     = 26;
    int ws      = 25;
    int dataIn  = 22;
    int dataOut = -1;
    int i2sPort = 0;
};

static bool _i2sInitialized = false;
static int _i2sSampleRate = 16000;
static int _i2sBitsPerSample = 16;
static int _i2sVolume = 100;
static unsigned long _i2sSamplesRecorded = 0;
static unsigned long _i2sSamplesPlayed = 0;

static const char _i2sB64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static String _i2sBase64Encode(const uint8_t* data, size_t len) {
    String result;
    if (!data || len == 0) return result;
    result.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = ((uint32_t)data[i]) << 16;
        if (i + 1 < len) n |= ((uint32_t)data[i + 1]) << 8;
        if (i + 2 < len) n |= data[i + 2];
        result += _i2sB64Chars[(n >> 18) & 0x3F];
        result += _i2sB64Chars[(n >> 12) & 0x3F];
        result += (i + 1 < len) ? _i2sB64Chars[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < len) ? _i2sB64Chars[n & 0x3F] : '=';
    }
    return result;
}

class I2SAudioTool {
public:
    static void attach(Server& server, I2SPins pins = I2SPins()) {

        // i2s_init
        server.addTool("i2s_init",
            "Initialize I2S audio interface for microphone and/or speaker.",
            R"j({"type":"object","properties":{"sampleRate":{"type":"integer","description":"Sample rate in Hz (default: 16000)","enum":[8000,11025,16000,22050,32000,44100,48000]},"bitsPerSample":{"type":"integer","description":"Bits per sample (default: 16)","enum":[16,24,32]},"mode":{"type":"string","description":"Audio mode: mic, speaker, or duplex","enum":["mic","speaker","duplex"],"default":"mic"},"channels":{"type":"integer","description":"1=mono, 2=stereo (default: 1)","enum":[1,2]}}})j",
            [pins](const JsonObject& args) -> String {
                JsonDocument doc;
#ifdef ESP32
                _i2sSampleRate = args["sampleRate"] | 16000;
                _i2sBitsPerSample = args["bitsPerSample"] | 16;
                const char* mode = args["mode"] | "mic";
                int channels = args["channels"] | 1;

                i2s_config_t i2s_config = {};
                i2s_config.sample_rate = _i2sSampleRate;
                i2s_config.bits_per_sample = (i2s_bits_per_sample_t)_i2sBitsPerSample;
                i2s_config.channel_format = (channels == 2) ? I2S_CHANNEL_FMT_RIGHT_LEFT : I2S_CHANNEL_FMT_ONLY_LEFT;
                i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
                i2s_config.dma_buf_count = 8;
                i2s_config.dma_buf_len = 1024;
                i2s_config.use_apll = false;
                i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;

                if (strcmp(mode, "mic") == 0) i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
                else if (strcmp(mode, "speaker") == 0) i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
                else i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);

                i2s_port_t port = (i2s_port_t)pins.i2sPort;
                esp_err_t err = i2s_driver_install(port, &i2s_config, 0, NULL);

                if (err != ESP_OK) {
                    doc["error"] = "I2S driver install failed";
                } else {
                    i2s_pin_config_t pin_config = {};
                    pin_config.bck_io_num = pins.bck;
                    pin_config.ws_io_num = pins.ws;
                    pin_config.data_in_num = pins.dataIn;
                    pin_config.data_out_num = pins.dataOut;

                    err = i2s_set_pin(port, &pin_config);
                    if (err != ESP_OK) {
                        doc["error"] = "I2S pin config failed";
                        i2s_driver_uninstall(port);
                    } else {
                        i2s_zero_dma_buffer(port);
                        _i2sInitialized = true;
                        doc["ok"] = true;
                        doc["sampleRate"] = _i2sSampleRate;
                        doc["bitsPerSample"] = _i2sBitsPerSample;
                        doc["mode"] = mode;
                        doc["channels"] = channels;
                    }
                }
#else
                doc["error"] = "I2S audio only supported on ESP32";
#endif
                String result;
                serializeJson(doc, result);
                return result;
            });

        // i2s_record
        server.addTool("i2s_record",
            "Record audio from I2S microphone. Returns base64-encoded WAV or raw PCM.",
            R"j({"type":"object","properties":{"durationMs":{"type":"integer","description":"Recording duration in ms (default: 1000, max: 5000)"},"format":{"type":"string","description":"Output format","enum":["raw","wav"],"default":"wav"}}})j",
            [pins](const JsonObject& args) -> String {
                JsonDocument doc;
#ifdef ESP32
                if (!_i2sInitialized) {
                    doc["error"] = "I2S not initialized";
                } else {
                    int durationMs = args["durationMs"] | 1000;
                    if (durationMs > 5000) durationMs = 5000;
                    const char* format = args["format"] | "wav";

                    int bytesPerSample = _i2sBitsPerSample / 8;
                    size_t totalBytes = (_i2sSampleRate * bytesPerSample * durationMs) / 1000;
                    uint8_t* buffer = (uint8_t*)malloc(totalBytes + 44);
                    if (!buffer) {
                        doc["error"] = "Out of memory";
                    } else {
                        uint8_t* pcmStart = (strcmp(format, "wav") == 0) ? buffer + 44 : buffer;
                        size_t bytesRead = 0;
                        i2s_port_t port = (i2s_port_t)pins.i2sPort;
                        size_t remaining = totalBytes;

                        while (remaining > 0) {
                            size_t chunk = (remaining > 4096) ? 4096 : remaining;
                            size_t read = 0;
                            i2s_read(port, pcmStart + bytesRead, chunk, &read, portMAX_DELAY);
                            bytesRead += read;
                            remaining -= read;
                        }
                        _i2sSamplesRecorded += bytesRead / bytesPerSample;

                        uint8_t* encodeStart = pcmStart;
                        size_t encodeLen = bytesRead;

                        if (strcmp(format, "wav") == 0) {
                            uint32_t dataSize = bytesRead;
                            uint32_t fileSize = dataSize + 36;
                            uint8_t* h = buffer;
                            memcpy(h, "RIFF", 4); h += 4;
                            memcpy(h, &fileSize, 4); h += 4;
                            memcpy(h, "WAVE", 4); h += 4;
                            memcpy(h, "fmt ", 4); h += 4;
                            uint32_t fmtSize = 16; memcpy(h, &fmtSize, 4); h += 4;
                            uint16_t audioFmt = 1; memcpy(h, &audioFmt, 2); h += 2;
                            uint16_t numCh = 1; memcpy(h, &numCh, 2); h += 2;
                            uint32_t sr = _i2sSampleRate; memcpy(h, &sr, 4); h += 4;
                            uint32_t byteRate = _i2sSampleRate * bytesPerSample; memcpy(h, &byteRate, 4); h += 4;
                            uint16_t blockAlign = bytesPerSample; memcpy(h, &blockAlign, 2); h += 2;
                            uint16_t bps = _i2sBitsPerSample; memcpy(h, &bps, 2); h += 2;
                            memcpy(h, "data", 4); h += 4;
                            memcpy(h, &dataSize, 4);
                            encodeStart = buffer;
                            encodeLen = bytesRead + 44;
                        }

                        String encoded = _i2sBase64Encode(encodeStart, encodeLen);
                        free(buffer);
                        doc["ok"] = true;
                        doc["format"] = format;
                        doc["durationMs"] = durationMs;
                        doc["sampleRate"] = _i2sSampleRate;
                        doc["bytesRecorded"] = (int)bytesRead;
                        doc["data"] = encoded;
                    }
                }
#else
                doc["error"] = "I2S audio only supported on ESP32";
#endif
                String result;
                serializeJson(doc, result);
                return result;
            });

        // i2s_play
        server.addTool("i2s_play",
            "Play base64-encoded audio through I2S speaker/DAC.",
            R"j({"type":"object","properties":{"data":{"type":"string","description":"Base64-encoded audio data (PCM or WAV)"}},"required":["data"]})j",
            [pins](const JsonObject& args) -> String {
                JsonDocument doc;
#ifdef ESP32
                if (!_i2sInitialized) {
                    doc["error"] = "I2S not initialized";
                } else {
                    const char* b64data = args["data"] | "";
                    size_t b64len = strlen(b64data);
                    size_t maxDecoded = (b64len / 4) * 3;
                    uint8_t* decoded = (uint8_t*)malloc(maxDecoded);
                    if (!decoded) {
                        doc["error"] = "Out of memory";
                    } else {
                        size_t decodedLen = 0;
                        unsigned char dtable[256] = {};
                        for (int i = 0; i < 64; i++) dtable[(unsigned char)_i2sB64Chars[i]] = i;
                        for (size_t i = 0; i + 3 < b64len; i += 4) {
                            uint32_t n = (dtable[(unsigned char)b64data[i]] << 18) |
                                         (dtable[(unsigned char)b64data[i+1]] << 12) |
                                         (dtable[(unsigned char)b64data[i+2]] << 6) |
                                         dtable[(unsigned char)b64data[i+3]];
                            decoded[decodedLen++] = (n >> 16) & 0xFF;
                            if (b64data[i+2] != '=') decoded[decodedLen++] = (n >> 8) & 0xFF;
                            if (b64data[i+3] != '=') decoded[decodedLen++] = n & 0xFF;
                        }

                        uint8_t* pcmData = decoded;
                        size_t pcmLen = decodedLen;
                        if (decodedLen > 44 && memcmp(decoded, "RIFF", 4) == 0) {
                            pcmData = decoded + 44;
                            pcmLen = decodedLen - 44;
                        }

                        if (_i2sVolume < 100 && _i2sBitsPerSample == 16) {
                            int16_t* samples = (int16_t*)pcmData;
                            for (size_t i = 0; i < pcmLen / 2; i++)
                                samples[i] = (int16_t)((int32_t)samples[i] * _i2sVolume / 100);
                        }

                        i2s_port_t port = (i2s_port_t)pins.i2sPort;
                        size_t bytesWritten = 0;
                        i2s_write(port, pcmData, pcmLen, &bytesWritten, portMAX_DELAY);
                        _i2sSamplesPlayed += bytesWritten / (_i2sBitsPerSample / 8);
                        free(decoded);
                        doc["ok"] = true;
                        doc["bytesPlayed"] = (int)bytesWritten;
                    }
                }
#else
                doc["error"] = "I2S audio only supported on ESP32";
#endif
                String result;
                serializeJson(doc, result);
                return result;
            });

        // i2s_volume
        server.addTool("i2s_volume",
            "Set I2S playback volume (software scaling).",
            R"j({"type":"object","properties":{"volume":{"type":"integer","description":"Volume 0-100","minimum":0,"maximum":100}},"required":["volume"]})j",
            [](const JsonObject& args) -> String {
                JsonDocument doc;
#ifdef ESP32
                _i2sVolume = args["volume"] | 100;
                if (_i2sVolume < 0) _i2sVolume = 0;
                if (_i2sVolume > 100) _i2sVolume = 100;
                doc["ok"] = true;
                doc["volume"] = _i2sVolume;
#else
                doc["error"] = "I2S audio only supported on ESP32";
#endif
                String result;
                serializeJson(doc, result);
                return result;
            });

        // i2s_status
        server.addTool("i2s_status",
            "Get I2S audio interface status and statistics.",
            R"j({"type":"object","properties":{}})j",
            [](const JsonObject& args) -> String {
                JsonDocument doc;
#ifdef ESP32
                doc["initialized"] = _i2sInitialized;
                doc["sampleRate"] = _i2sSampleRate;
                doc["bitsPerSample"] = _i2sBitsPerSample;
                doc["volume"] = _i2sVolume;
                doc["totalSamplesRecorded"] = _i2sSamplesRecorded;
                doc["totalSamplesPlayed"] = _i2sSamplesPlayed;
#else
                doc["error"] = "I2S audio only supported on ESP32";
#endif
                String result;
                serializeJson(doc, result);
                return result;
            });

        // i2s_stop
        server.addTool("i2s_stop",
            "Stop I2S audio interface and free driver resources.",
            R"j({"type":"object","properties":{}})j",
            [pins](const JsonObject& args) -> String {
                JsonDocument doc;
#ifdef ESP32
                if (_i2sInitialized) {
                    i2s_driver_uninstall((i2s_port_t)pins.i2sPort);
                    _i2sInitialized = false;
                    doc["ok"] = true;
                } else {
                    doc["error"] = "I2S not initialized";
                }
#else
                doc["error"] = "I2S audio only supported on ESP32";
#endif
                String result;
                serializeJson(doc, result);
                return result;
            });
    }
};

} // namespace tools

inline void addI2SAudioTools(Server& server, tools::I2SPins pins = tools::I2SPins()) {
    tools::I2SAudioTool::attach(server, pins);
}

} // namespace mcpd

#endif // MCPD_I2S_AUDIO_TOOL_H
