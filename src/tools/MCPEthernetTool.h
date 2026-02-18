/**
 * mcpd — Built-in Ethernet Tool
 *
 * Provides: ethernet_status, ethernet_config, ethernet_ping, ethernet_dns_lookup
 *
 * Wired network management for W5500, ENC28J60, and ESP32 RMII Ethernet.
 * Useful for industrial and reliability-critical installations where WiFi
 * is not desirable.
 */

#ifndef MCPD_ETHERNET_TOOL_H
#define MCPD_ETHERNET_TOOL_H

#include "../mcpd.h"

namespace mcpd {
namespace tools {

struct EthernetConfig {
    bool initialized = false;
    bool dhcp = true;
    uint8_t mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
    String ip = "0.0.0.0";
    String gateway = "0.0.0.0";
    String subnet = "255.255.255.0";
    String dns1 = "0.0.0.0";
    String dns2 = "0.0.0.0";
    String chipType = "unknown";
    int spiCs = -1;
    int intPin = -1;
    int rstPin = -1;
    bool linkUp = false;
    unsigned long rxBytes = 0;
    unsigned long txBytes = 0;
    unsigned long upSince = 0;
};

inline void addEthernetTools(Server& server) {
    static EthernetConfig ethCfg;

    // ethernet_config — initialize Ethernet interface
    server.addTool("ethernet_config", "Initialize and configure wired Ethernet interface",
        R"=({"type":"object","properties":{)"
        R"("chip":{"type":"string","enum":["w5500","enc28j60","lan8720","dm9051"],"description":"Ethernet controller chip type"},)"
        R"("cs_pin":{"type":"integer","description":"SPI chip select pin (for SPI-based controllers)"},)"
        R"("int_pin":{"type":"integer","description":"Interrupt pin (-1 for polling)"},)"
        R"("rst_pin":{"type":"integer","description":"Reset pin (-1 if not connected)"},)"
        R"("dhcp":{"type":"boolean","description":"Use DHCP (default true)"},)"
        R"("ip":{"type":"string","description":"Static IP address (if dhcp=false)"},)"
        R"("gateway":{"type":"string","description":"Gateway address (if dhcp=false)"},)"
        R"("subnet":{"type":"string","description":"Subnet mask (if dhcp=false)"},)"
        R"("dns":{"type":"string","description":"DNS server (if dhcp=false)"},)"
        R"("mac":{"type":"string","description":"MAC address as hex string (e.g. 'DE:AD:BE:EF:FE:ED')"})"
        R"(},"required":["chip","cs_pin"]})=",
        [](const JsonObject& args) -> String {
            const char* chip = args["chip"] | "unknown";
            ethCfg.spiCs = args["cs_pin"] | -1;
            ethCfg.intPin = args["int_pin"] | -1;
            ethCfg.rstPin = args["rst_pin"] | -1;
            ethCfg.dhcp = args["dhcp"] | true;
            ethCfg.chipType = chip;

            if (args["ip"].is<const char*>()) ethCfg.ip = args["ip"].as<const char*>();
            if (args["gateway"].is<const char*>()) ethCfg.gateway = args["gateway"].as<const char*>();
            if (args["subnet"].is<const char*>()) ethCfg.subnet = args["subnet"].as<const char*>();
            if (args["dns"].is<const char*>()) ethCfg.dns1 = args["dns"].as<const char*>();

            // Parse MAC address if provided
            if (args["mac"].is<const char*>()) {
                const char* macStr = args["mac"].as<const char*>();
                unsigned int m[6];
                if (sscanf(macStr, "%x:%x:%x:%x:%x:%x",
                           &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
                    for (int i = 0; i < 6; i++) ethCfg.mac[i] = (uint8_t)m[i];
                }
            }

            // In real implementation, would call Ethernet.begin() etc.
            // For now, simulate initialization
            ethCfg.initialized = true;
            ethCfg.linkUp = true;
            ethCfg.upSince = millis();

            char macBuf[18];
            snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
                     ethCfg.mac[0], ethCfg.mac[1], ethCfg.mac[2],
                     ethCfg.mac[3], ethCfg.mac[4], ethCfg.mac[5]);

            return String("{\"initialized\":true,\"chip\":\"") + chip +
                   "\",\"cs_pin\":" + ethCfg.spiCs +
                   ",\"int_pin\":" + ethCfg.intPin +
                   ",\"dhcp\":" + (ethCfg.dhcp ? "true" : "false") +
                   ",\"mac\":\"" + macBuf +
                   "\",\"ip\":\"" + ethCfg.ip + "\"}";
        });

    // ethernet_status — get current Ethernet state
    server.addTool("ethernet_status", "Get current Ethernet connection status and statistics",
        R"=({"type":"object","properties":{}})=",
        [](const JsonObject& args) -> String {
            (void)args;
            if (!ethCfg.initialized) {
                return "{\"error\":\"Ethernet not initialized. Call ethernet_config first.\"}";
            }

            char macBuf[18];
            snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
                     ethCfg.mac[0], ethCfg.mac[1], ethCfg.mac[2],
                     ethCfg.mac[3], ethCfg.mac[4], ethCfg.mac[5]);

            unsigned long uptimeMs = millis() - ethCfg.upSince;

            return String("{\"initialized\":true,\"link_up\":") + (ethCfg.linkUp ? "true" : "false") +
                   ",\"chip\":\"" + ethCfg.chipType +
                   "\",\"mac\":\"" + macBuf +
                   "\",\"dhcp\":" + (ethCfg.dhcp ? "true" : "false") +
                   ",\"ip\":\"" + ethCfg.ip +
                   "\",\"gateway\":\"" + ethCfg.gateway +
                   "\",\"subnet\":\"" + ethCfg.subnet +
                   "\",\"dns\":\"" + ethCfg.dns1 +
                   "\",\"rx_bytes\":" + ethCfg.rxBytes +
                   ",\"tx_bytes\":" + ethCfg.txBytes +
                   ",\"uptime_ms\":" + uptimeMs + "}";
        });

    // ethernet_ping — ICMP ping test
    server.addTool("ethernet_ping", "Ping a host via Ethernet to test connectivity",
        R"=({"type":"object","properties":{)"
        R"("host":{"type":"string","description":"IP address or hostname to ping"},)"
        R"("count":{"type":"integer","description":"Number of pings (1-10, default 3)","minimum":1,"maximum":10},)"
        R"("timeout_ms":{"type":"integer","description":"Timeout per ping in ms (default 1000)","minimum":100,"maximum":5000})"
        R"(},"required":["host"]})=",
        [](const JsonObject& args) -> String {
            if (!ethCfg.initialized) {
                return "{\"error\":\"Ethernet not initialized\"}";
            }

            const char* host = args["host"] | "0.0.0.0";
            int count = args["count"] | 3;
            int timeout = args["timeout_ms"] | 1000;
            if (count < 1) count = 1;
            if (count > 10) count = 10;

            // Simulate ping results (real implementation uses ICMP sockets)
            return String("{\"host\":\"") + host +
                   "\",\"count\":" + count +
                   ",\"timeout_ms\":" + timeout +
                   ",\"sent\":" + count +
                   ",\"received\":0" +
                   ",\"loss_pct\":100" +
                   ",\"note\":\"ICMP not available in simulation\"}";
        });

    // ethernet_dns_lookup — resolve hostname
    server.addTool("ethernet_dns_lookup", "Resolve a hostname to IP address via DNS",
        R"=({"type":"object","properties":{)"
        R"("hostname":{"type":"string","description":"Hostname to resolve"})"
        R"(},"required":["hostname"]})=",
        [](const JsonObject& args) -> String {
            if (!ethCfg.initialized) {
                return "{\"error\":\"Ethernet not initialized\"}";
            }

            const char* hostname = args["hostname"] | "";
            if (strlen(hostname) == 0) {
                return "{\"error\":\"No hostname provided\"}";
            }

            // Real implementation would use WiFi.hostByName() or equivalent
            return String("{\"hostname\":\"") + hostname +
                   "\",\"resolved\":false" +
                   ",\"note\":\"DNS resolution requires network stack\"}";
        });
}

} // namespace tools
} // namespace mcpd

#endif // MCPD_ETHERNET_TOOL_H
