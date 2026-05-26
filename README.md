# AILiv — AI usage monitors for your Windows taskbar

A collection of [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) plugins that display AI service usage, balances, and quotas directly in the taskbar.

## Plugins

| Plugin | Directory | Description | Status |
|--------|-----------|-------------|--------|
| **DeepSeek Balance** | [`DeepSeek/`](DeepSeek/) | DeepSeek API account balance | ✅ Ready |
| Codex | `codex/` | OpenAI Codex CLI usage/credits | 🚧 Planned |
| Gemini | `gemini/` | Google Gemini API usage | 🚧 Planned |
| Claude | `claude/` | Anthropic Claude API usage | 🚧 Planned |
| *(more TBD)* | | | |

---

## DeepSeek Balance

Displays your **DeepSeek API account balance** in the taskbar.

**Features:**
- Real-time balance display — `¥12.34` or `$12.34` in the taskbar
- Configurable refresh interval (1–1440 min)
- Left-click opens DeepSeek billing page
- Native settings dialog for API key & interval
- Zero external dependencies (WinHTTP only)

### Installation

1. Download `DeepSeekBalance.dll` from [Releases](https://github.com/Likhixang/AILiv/releases)
2. Copy to `<TrafficMonitor>/plugins/`
3. Restart TrafficMonitor, enable in **Plugin Management**
4. Right-click → **Options** / **Settings** to configure API key

### Configuration

Stored in `DeepSeekBalance.ini` under TrafficMonitor's config directory:

```ini
[Settings]
ApiKey=sk-you...-key
FetchInterval=30
```

## Building All Plugins

### Prerequisites

- **Windows**: Visual Studio 2019+ or CMake + MSVC
- **Linux**: CMake + MinGW-w64 (for cross-compilation)

### From source

```bash
mkdir build && cd build

# MSVC
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Linux MinGW cross-compile
cmake .. -DCMAKE_TOOLCHAIN_FILE=../mingw-toolchain.cmake
cmake --build .
```

Output: `build/plugins/*.dll`

Each plugin compiles independently — you can also build a single one by targeting its name (e.g. `cmake --build . --target DeepSeekBalance`).

## Shared SDK

[`PluginInterface.h`](PluginInterface.h) sits in the project root and is the official TrafficMonitor plugin SDK. All plugins include it from the parent path.

Future shared utility code (JSON parsing, HTTP helpers, etc.) will also live in the root as it gets extracted from individual plugins.

## License

MIT
