# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Configure (from project root)
cmake -B build

# Build all
cmake --build build

# Build specific target
cmake --build build --target ClientApp
cmake --build build --target ServerApp
```

Executables: `build/ClientApp/ClientApp` and `build/ServerApp/ServerApp`

## Running

```bash
# Start server first (GUI)
./build/ServerApp/ServerApp

# Start one or more clients (console)
./build/ClientApp/ClientApp
./build/ClientApp/ClientApp --host 192.168.1.100 --port 12345
```

## Requirements

- CMake 3.16+
- Qt 6.5+ (Core, Network, Widgets components)
- C++17 compiler

## Architecture

Client-server TCP application for device telemetry. Server runs on port 12345.

### Protocol

JSON messages delimited by newline (`\n`). Message types:

**Server → Client:**
- `ConnectionConfirm`: `{"type": "ConnectionConfirm", "client_id": N, "status": "connected"}`
- `Command`: `{"type": "Command", "command": "start"|"stop"}`

**Client → Server:**
- `NetworkMetrics`: `{"type": "NetworkMetrics", "bandwidth": X, "latency": X, "packet_loss": X}`
- `DeviceStatus`: `{"type": "DeviceStatus", "uptime": N, "cpu_usage": N, "memory_usage": N}`
- `Log`: `{"type": "Log", "message": "...", "severity": "INFO"|"WARNING"|"ERROR"|"DEBUG"}`

### ServerApp (GUI)

- `TcpServer` (`tcpserver.h/cpp`): TCP server with multi-client support, runs in separate QThread. Manages connections via `QMap<QTcpSocket*, ClientInfo>`. Emits signals for GUI updates.
- `ServerWindow` (`serverwindow.h/cpp/ui`): Main window with client table, data table, event log. Uses `QMetaObject::invokeMethod` for cross-thread communication with TcpServer.
- Configurable thresholds (`ThresholdConfig`) trigger warnings when metrics exceed limits.

### ClientApp (Console)

- `Client` (`client.h/cpp`): State machine (Disconnected → Connecting → WaitingConfirmation → WaitingStart → Running). Auto-reconnects every 5 seconds on disconnect. Sends random data at 10-100ms intervals when running.
- Generates variable-length messages: short (<50 chars), medium (50-200), long (200+ for logs).

### Key Patterns

- Signal/slot for all async events
- Newline-delimited JSON for message framing with receive buffers for incomplete messages
- `QRandomGenerator` for data generation
- Cross-thread communication via queued connections and `QMetaObject::invokeMethod`
