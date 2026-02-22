# mcpd Architecture

## Overview

mcpd is a single-header-friendly MCP server SDK designed for resource-constrained microcontrollers. The architecture prioritizes low memory usage, zero dynamic allocation where possible, and a clean separation between protocol handling and hardware abstraction.

```
┌─────────────────────────────────────────────┐
│                MCP Clients                  │
│  (Claude Desktop, Cursor, custom clients)   │
└──────────┬──────────┬──────────┬────────────┘
           │ HTTP/SSE │    WS    │   BLE
           ▼          ▼          ▼
┌──────────────────────────────────────────────┐
│              Transport Layer                 │
│  MCPTransport │ MCPTransportWS │ BLETransport│
└──────────────────┬───────────────────────────┘
                   │ JSON-RPC 2.0
                   ▼
┌──────────────────────────────────────────────┐
│              mcpd::Server                    │
│  ┌─────────┐ ┌──────────┐ ┌───────────────┐ │
│  │ Dispatch │ │ Sessions │ │ Capabilities  │ │
│  │ Router   │ │ Manager  │ │ Negotiation   │ │
│  └────┬─────┘ └──────────┘ └───────────────┘ │
│       │                                      │
│  ┌────┴──────────────────────────────────┐   │
│  │          Feature Modules              │   │
│  │ Tools │ Resources │ Prompts │ Tasks   │   │
│  │ Logging │ Sampling │ Elicitation      │   │
│  │ Progress │ Roots │ Completion         │   │
│  └───────────────────────────────────────┘   │
│       │                                      │
│  ┌────┴──────────────────────────────────┐   │
│  │       Cross-cutting Concerns          │   │
│  │ Auth │ RateLimit │ Metrics │ Hooks    │   │
│  └───────────────────────────────────────┘   │
└──────────────────┬───────────────────────────┘
                   │
┌──────────────────┴───────────────────────────┐
│         Platform HAL + Built-in Tools        │
│  GPIO │ I2C │ SPI │ ADC │ PWM │ UART │ ...  │
│  (106 hardware tools across 20+ categories)  │
└──────────────────────────────────────────────┘
```

## Core Components

### `mcpd::Server` (`mcpd.h` / `mcpd.cpp`)

The central orchestrator (~1500 lines). Responsibilities:

- **JSON-RPC dispatch**: Routes incoming requests to the correct handler based on `method` field. Supports batch requests (JSON arrays).
- **Capability negotiation**: `initialize` handshake advertises supported features (tools, resources, prompts, logging, sampling, etc.).
- **Session management**: Tracks client sessions with unique IDs, handles `initialized` lifecycle.
- **Notification fan-out**: Pushes `notifications/tools/list_changed`, `notifications/resources/list_changed`, etc. to all active sessions.

### Transport Layer

Three transport implementations, all feeding JSON-RPC strings into the same dispatch:

| Transport | File | Use Case |
|-----------|------|----------|
| **Streamable HTTP + SSE** | `MCPTransport.h` / `MCPTransportSSE.h` | Primary. Claude Desktop via mcpd-bridge. |
| **WebSocket** | `MCPTransportWS.h` | Browser clients, persistent connections. |
| **BLE GATT** | `MCPTransportBLE.h` | Phone apps, proximity-based. Automatic chunking for MTU limits. |

### Feature Modules

Each MCP capability is encapsulated in its own header:

| Module | Header | MCP Method(s) |
|--------|--------|---------------|
| Tools | `MCPTool.h` | `tools/list`, `tools/call` |
| Resources | `MCPResource.h` | `resources/list`, `resources/read`, `resources/subscribe` |
| Resource Templates | `MCPResourceTemplate.h` | `resources/templates/list` |
| Prompts | `MCPPrompt.h` | `prompts/list`, `prompts/get` |
| Tasks | `MCPTask.h` | `tasks/get`, `tasks/list`, `tasks/result`, `tasks/cancel` |
| Logging | `MCPLogging.h` | `logging/setLevel` |
| Sampling | `MCPSampling.h` | `sampling/createMessage` (server→client) |
| Elicitation | `MCPElicitation.h` | `elicitation/create` (server→client) |
| Progress | `MCPProgress.h` | `notifications/progress` |
| Roots | `MCPRoots.h` | `roots/list` |
| Completion | `MCPCompletion.h` | `completion/complete` |
| Content | `MCPContent.h` | Text, Image, Audio, ResourceLink content types |
| Icons | `MCPIcon.h` | Server/tool/resource/prompt icons |

### Cross-cutting Concerns

| Module | Header | Purpose |
|--------|--------|---------|
| Auth | `MCPAuth.h` | API key authentication, multi-key support |
| Rate Limiting | `MCPRateLimit.h` | Token-bucket rate limiter per endpoint |
| Metrics | `MCPMetrics.h` | Prometheus-compatible metrics endpoint |
| Diagnostics | `MCPDiagnostics.h` | Heap monitoring, uptime, request counters |
| Session | `MCPSession.h` | Client session state, subscription tracking |
| Heap | `MCPHeap.h` | Memory-safe allocation tracking for MCU |

### Platform HAL (`src/platform/`)

Hardware abstraction layer isolating platform-specific calls:

- `MCPPlatformESP32.h` — ESP-IDF / Arduino ESP32
- `MCPPlatformRP2040.h` — Raspberry Pi Pico
- `MCPPlatformSTM32.h` — STM32 HAL
- `MCPPlatformGeneric.h` — Fallback for testing

### Built-in Tools (`src/tools/`)

106 pre-built hardware tools organized by category (GPIO, PWM, Servo, I2C, SPI, ADC, UART, CAN, Modbus, LCD, IR, RTC, Camera, ESP-NOW, OTA, etc.). Each tool auto-registers with correct JSON Schema and annotations.

## Design Decisions

### Single-header-friendly

All modules are header-only except `mcpd.cpp`. This means a project can `#include <mcpd.h>` and get everything. The trade-off is compile time, but for MCU projects this is acceptable.

### ArduinoJson for JSON

ArduinoJson v7 is used throughout for JSON parsing and serialization. It provides:
- Static allocation (`StaticJsonDocument`) for predictable memory use
- Streaming serialization to avoid large intermediate buffers

### String over std::string

Arduino `String` is used instead of `std::string` for compatibility across Arduino cores. This is a pragmatic choice — some Arduino cores don't ship a full C++ stdlib.

### No Exceptions

The entire codebase is exception-free. Error handling uses return values (bool, nullptr, error JSON). This is critical for MCU targets where exceptions may not be available or are prohibitively expensive.

### Test Architecture

Tests compile as native C++ (no MCU needed) using mock headers in `test/mock_includes/` that stub out Arduino, WiFi, BLE, and peripheral APIs. The test framework (`test_framework.h`) is custom and minimal — no external dependencies.

```
test/
├── mock_includes/      # Arduino/WiFi/BLE stubs for native compilation
├── native/
│   ├── Makefile        # Build and run all test suites
│   └── test_mcp_http.cpp
├── test_framework.h    # Custom test macros (TEST, ASSERT_*, RUN_TEST)
├── test_jsonrpc.cpp    # JSON-RPC protocol tests
├── test_tools.cpp      # Tool/resource/prompt tests
├── test_modules.cpp    # Feature module tests
├── test_tasks.cpp      # Async tasks tests
└── ...                 # 14 test files, 1146+ tests total
```

## Message Flow

```
Client                          mcpd::Server
  │                                   │
  │──── POST /mcp ────────────────────│
  │     {"jsonrpc":"2.0",             │
  │      "method":"tools/call",       │
  │      "params":{...},              │
  │      "id":1}                      │
  │                                   │
  │                      ┌────────────┤
  │                      │ Auth check │
  │                      │ Rate limit │
  │                      │ Session    │
  │                      │ lookup     │
  │                      │ Before-hook│
  │                      │ Dispatch   │
  │                      │ After-hook │
  │                      └────────────┤
  │                                   │
  │◄─── 200 OK ──────────────────────│
  │     {"jsonrpc":"2.0",             │
  │      "result":{...},              │
  │      "id":1}                      │
  │                                   │
```

## Memory Budget

Target memory footprint for a typical configuration:

| Component | RAM (approx.) |
|-----------|---------------|
| Server core | ~2 KB |
| Per session | ~200 bytes |
| Per tool registration | ~100 bytes |
| Per resource registration | ~120 bytes |
| JSON parse buffer | 4-16 KB (configurable) |
| HTTP/SSE transport | ~1 KB |
| WebSocket transport | ~2 KB |
| BLE transport | ~3 KB |
| **Typical total** | **~10-20 KB** |

This leaves plenty of room on ESP32 (520 KB SRAM) and RP2040 (264 KB SRAM).
