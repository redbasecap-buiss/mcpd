/**
 * mcpd — Built-in Buzzer/Tone Tool
 *
 * Provides: buzzer_tone, buzzer_melody, buzzer_stop
 *
 * Piezo buzzer control with frequency, duration, and melody support.
 * Uses LEDC PWM on ESP32 for precise tone generation.
 * Includes predefined melodies (alert, success, error, startup).
 */

#ifndef MCPD_BUZZER_TOOL_H
#define MCPD_BUZZER_TOOL_H

#include "../mcpd.h"

namespace mcpd {
namespace tools {

class BuzzerTool {
public:
    struct Config {
        int pin = -1;
        int ledcChannel = 0;
        bool active = false;
    };

    static Config cfg;

    // Musical note frequencies (Hz)
    static constexpr int NOTE_C4 = 262, NOTE_D4 = 294, NOTE_E4 = 330,
        NOTE_F4 = 349, NOTE_G4 = 392, NOTE_A4 = 440, NOTE_B4 = 494,
        NOTE_C5 = 523, NOTE_D5 = 587, NOTE_E5 = 659, NOTE_F5 = 698,
        NOTE_G5 = 784, NOTE_A5 = 880, NOTE_REST = 0;

    struct Note { int freq; int durationMs; };

    static void playNote(int freq, int durationMs) {
        if (freq <= 0) {
            // Rest — silence
#ifdef ESP32
            ledcWriteTone(cfg.ledcChannel, 0);
#else
            noTone(cfg.pin);
#endif
        } else {
#ifdef ESP32
            ledcWriteTone(cfg.ledcChannel, freq);
#else
            tone(cfg.pin, freq);
#endif
        }
        delay(durationMs);
    }

    static void stopTone() {
#ifdef ESP32
        ledcWriteTone(cfg.ledcChannel, 0);
#else
        noTone(cfg.pin);
#endif
        cfg.active = false;
    }

    static void attach(Server& server, int pin, int ledcChannel = 0) {
        cfg.pin = pin;
        cfg.ledcChannel = ledcChannel;

#ifdef ESP32
        ledcSetup(ledcChannel, 2000, 8);
        ledcAttachPin(pin, ledcChannel);
#else
        pinMode(pin, OUTPUT);
#endif

        // buzzer_tone — play a single tone
        server.addTool("buzzer_tone", "Play a tone at a specific frequency and duration",
            R"=({"type":"object","properties":{"frequency":{"type":"integer","minimum":20,"maximum":20000,"description":"Tone frequency in Hz (20-20000)"},"duration":{"type":"integer","minimum":10,"maximum":10000,"description":"Duration in milliseconds (10-10000)"},"duty":{"type":"integer","minimum":1,"maximum":100,"description":"Duty cycle percentage for volume control (default 50, ESP32 only)"}},"required":["frequency","duration"]})=",
            [](const JsonObject& args) -> String {
                int freq = args["frequency"];
                int duration = args["duration"];

                if (freq < 20 || freq > 20000) return R"=({"error":"Frequency must be 20-20000 Hz"})=";
                if (duration < 10 || duration > 10000) return R"=({"error":"Duration must be 10-10000 ms"})=";

                cfg.active = true;
                playNote(freq, duration);
                stopTone();

                return String("{\"frequency\":") + freq +
                       ",\"duration_ms\":" + duration +
                       ",\"pin\":" + cfg.pin +
                       ",\"played\":true}";
            });

        // buzzer_melody — play a predefined or custom melody
        server.addTool("buzzer_melody", "Play a predefined melody or custom note sequence",
            R"=({"type":"object","properties":{"name":{"type":"string","enum":["alert","success","error","startup","doorbell","siren"],"description":"Predefined melody name"},"notes":{"type":"array","items":{"type":"object","properties":{"freq":{"type":"integer","description":"Frequency in Hz (0 = rest)"},"ms":{"type":"integer","description":"Duration in ms"}},"required":["freq","ms"]},"description":"Custom melody as array of {freq, ms} (max 32 notes)","maxItems":32},"tempo":{"type":"number","minimum":0.25,"maximum":4.0,"description":"Tempo multiplier (0.25 = 4x slower, 4.0 = 4x faster, default 1.0)"}},"required":[]})=",
            [](const JsonObject& args) -> String {
                float tempo = args["tempo"] | 1.0f;
                if (tempo < 0.25f) tempo = 0.25f;
                if (tempo > 4.0f) tempo = 4.0f;

                Note melody[32];
                int noteCount = 0;

                if (args.containsKey("notes")) {
                    JsonArray notesArr = args["notes"].as<JsonArray>();
                    for (JsonVariant n : notesArr) {
                        if (noteCount >= 32) break;
                        melody[noteCount].freq = n["freq"] | 0;
                        melody[noteCount].durationMs = (int)((n["ms"] | 100) / tempo);
                        noteCount++;
                    }
                } else if (args.containsKey("name")) {
                    String name = args["name"].as<const char*>();

                    if (name == "alert") {
                        Note m[] = {{NOTE_A5,100},{NOTE_REST,50},{NOTE_A5,100},{NOTE_REST,50},{NOTE_A5,100}};
                        noteCount = 5; for (int i=0;i<noteCount;i++) melody[i] = m[i];
                    } else if (name == "success") {
                        Note m[] = {{NOTE_C5,100},{NOTE_E5,100},{NOTE_G5,200}};
                        noteCount = 3; for (int i=0;i<noteCount;i++) melody[i] = m[i];
                    } else if (name == "error") {
                        Note m[] = {{NOTE_G4,200},{NOTE_REST,50},{NOTE_C4,400}};
                        noteCount = 3; for (int i=0;i<noteCount;i++) melody[i] = m[i];
                    } else if (name == "startup") {
                        Note m[] = {{NOTE_C4,100},{NOTE_E4,100},{NOTE_G4,100},{NOTE_C5,200}};
                        noteCount = 4; for (int i=0;i<noteCount;i++) melody[i] = m[i];
                    } else if (name == "doorbell") {
                        Note m[] = {{NOTE_E5,300},{NOTE_C5,400}};
                        noteCount = 2; for (int i=0;i<noteCount;i++) melody[i] = m[i];
                    } else if (name == "siren") {
                        Note m[] = {{NOTE_A4,200},{NOTE_E5,200},{NOTE_A4,200},{NOTE_E5,200}};
                        noteCount = 4; for (int i=0;i<noteCount;i++) melody[i] = m[i];
                    } else {
                        return R"=({"error":"Unknown melody name"})=";
                    }

                    // Apply tempo
                    for (int i = 0; i < noteCount; i++)
                        melody[i].durationMs = (int)(melody[i].durationMs / tempo);
                } else {
                    return R"=({"error":"Specify 'name' for predefined melody or 'notes' for custom"})=";
                }

                cfg.active = true;
                int totalMs = 0;
                for (int i = 0; i < noteCount; i++) {
                    playNote(melody[i].freq, melody[i].durationMs);
                    totalMs += melody[i].durationMs;
                }
                stopTone();

                return String("{\"notes_played\":") + noteCount +
                       ",\"total_duration_ms\":" + totalMs +
                       ",\"tempo\":" + String(tempo, 2) +
                       ",\"played\":true}";
            });

        // buzzer_stop — stop any playing tone
        server.addTool("buzzer_stop", "Stop the buzzer immediately",
            R"=({"type":"object","properties":{}})=",
            [](const JsonObject& args) -> String {
                (void)args;
                bool wasActive = cfg.active;
                (void)args;
                stopTone();
                return String("{\"stopped\":true,\"was_active\":") +
                       (wasActive ? "true" : "false") + "}";
            });
    }
};

// Static member definitions
BuzzerTool::Config BuzzerTool::cfg;

} // namespace tools

/**
 * Register buzzer/tone tools.
 *
 * @param server       The mcpd::Server instance
 * @param pin          Buzzer GPIO pin
 * @param ledcChannel  ESP32 LEDC channel for tone generation (default 0)
 */
inline void addBuzzerTools(Server& server, int pin, int ledcChannel = 0) {
    tools::BuzzerTool::attach(server, pin, ledcChannel);
}

} // namespace mcpd

#endif // MCPD_BUZZER_TOOL_H
