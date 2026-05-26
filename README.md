# DeepSeek Balance Plugin for TrafficMonitor

A [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) plugin that displays your **DeepSeek API account balance** directly in the taskbar.

![Screenshot placeholder](https://via.placeholder.com/400x60?text=DS:+¥200.00)

## Features

- **Real-time balance display** — Shows `¥<balance>` or `$<balance>` in the taskbar
- **Configurable refresh interval** — Default 30 min, adjustable from 1 to 1440 min
- **Click to open billing page** — Left-click opens `https://platform.deepseek.com/billing`
- **Settings UI** — Configure API key and interval through a native dialog
- **Zero external dependencies** — Only uses standard Windows DLLs (WinHTTP, Shell32, etc.)

## Installation

1. Download the latest `PluginDeepSeekBalance.dll` from [Releases](https://github.com/Likhixang/PluginDeepSeekBalance/releases)
2. Copy it to `<TrafficMonitor>/plugins/` directory
3. Restart TrafficMonitor
4. Right-click TrafficMonitor → **Other Functions** → **Plugin Management**
5. Enable **DeepSeek Balance**
6. Right-click TrafficMonitor → **Options** / **Settings** to configure your API key

## Configuration

The plugin stores settings in `DeepSeekBalance.ini` under TrafficMonitor's config directory:

```ini
[Settings]
ApiKey=sk-your-deepseek-api-key
FetchInterval=30
```

You can edit this file directly, or use the built-in settings dialog.

## Building from Source

### Prerequisites

- **Windows**: Visual Studio 2019+ (with "Desktop development with C++") or CMake + MSVC
- **Linux**: CMake + MinGW-w64 (for cross-compilation)

### Windows (MSVC)

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Output: `build/plugins/PluginDeepSeekBalance.dll`

### Linux (MinGW cross-compilation)

```bash
# Install MinGW-w64 (Ubuntu/Debian)
sudo apt install g++-mingw-w64-x86-64 cmake

mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../mingw-toolchain.cmake
cmake --build .
```

Output: `build/plugins/PluginDeepSeekBalance.dll`

## API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `GET /user/balance` | `GET` | Query DeepSeek account balance |

**Response:**
```json
{
  "is_available": true,
  "balance_infos": [
    {
      "currency": "CNY",
      "total_balance": "215.07",
      "granted_balance": "0.00",
      "topped_up_balance": "215.07"
    }
  ]
}
```

## License

MIT
