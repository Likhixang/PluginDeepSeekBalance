# AILiv — AI usage monitors for your Windows taskbar

A collection of [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) plugins that display AI service usage, balances, and quotas directly in the taskbar.

## Plugins

| Plugin | Directory | Description | Status |
|--------|-----------|-------------|--------|
| **DeepSeek Balance** | [`DeepSeek/`](DeepSeek/) | DeepSeek API account balance | ✅ Ready |
| **Codex Balance** | [`Codex/`](Codex/) | OpenAI / Codex CLI usage & balance | ✅ Ready |
| **Claude Balance** | [`Claude/`](Claude/) | Anthropic Claude API usage & cost | ✅ Ready |
| **Gemini Balance** | [`Gemini/`](Gemini/) | Google Gemini API usage & cost | ✅ Ready |
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

| 插件 | 查询内容 | 接口类型 | 认证方式 |
|------|---------|---------|---------|
| **DeepSeek Balance** | 余额 | 官方 API (`/user/balance`) | API Key (`sk-...`) |
| **Codex Balance** | 累计花费 + 余额 | Dashboard API (优先) + `/organization/costs` (回退) | Session Cookie 优先，API Key 回退 |

## Codex Balance

Displays OpenAI / Codex CLI usage in the taskbar. Two data sources with priority fallback:

### Priority 1 — Session Cookie (full billing access)

Provides month-to-date spend, daily breakdown, and account balance (credits/grants).

**Endpoints (internal, undocumented):**
```
GET https://api.openai.com/dashboard/billing/usage
GET https://api.openai.com/dashboard/billing/subscription
```

**How to get the session cookie:**

1. In the plugin settings dialog, click **「Get Cookie」** — it opens `https://platform.openai.com/auth/login`
2. Log in with your OpenAI account
3. After logging in, open **Browser Developer Tools** (F12):
   - **Chrome/Edge**: `Application` → `Cookies` → `https://api.openai.com`
   - **Firefox**: `Storage` → `Cookies` → `https://api.openai.com`
4. Find the cookie named `__session` (or `Auth0` session cookie)
5. Copy its full value
6. Paste it into the **Session Cookie** field in the plugin settings

   > The cookie value is long (a JWT token, ~1000+ characters). Make sure you copy the entire string — it starts with `eyJ` (base64-encoded JSON) or `__session=eyJ...`

7. Click **OK** — the plugin will immediately try to fetch your billing data

**Troubleshooting:**
- Session cookies expire after a few hours to a few days. When you see `ERR` and your session was working before, re-do steps 2-6.
- If you see `No Auth`, no cookie or API key is configured.

### Priority 2 — API Key (fallback)

If no session cookie is configured, falls back to the official costs API:

```
GET https://api.openai.com/v1/organization/costs
```

Provides: month-to-date spend only (no balance/credit info).

### Display modes

| Auth mode | Taskbar example | Budget configured | Balance available |
|-----------|----------------|-------------------|-------------------|
| Session + budget | `Codex: $47.21 / $120.00` | ✅ Yes | ✅ Yes |
| Session only | `Codex: $47.21 \| $72.79 left` | ❌ No | ✅ Yes |
| API key + budget | `Codex: $47.21 / $120.00` | ✅ Yes | ❌ No |
| API key only | `Codex: $47.21` | ❌ No | ❌ No |

### Installation

1. Download `CodexBalance.dll` from [Releases](https://github.com/Likhixang/AILiv/releases)
2. Copy to `<TrafficMonitor>/plugins/`
3. Restart TrafficMonitor, enable in **Plugin Management**
4. Right-click → **Options** / **Settings** to configure auth

### Configuration

Stored in `CodexBalance.ini` under TrafficMonitor's config directory:

```ini
[Settings]
SessionCookie=__session=eyJ...           ; Priority 1 (optional)
ApiKey=sk-proj-...                        ; Priority 2 fallback (optional)
MonthlyBudget=120                         ; Optional, for progress display
FetchInterval=30
```

## Claude Balance

Displays Anthropic Claude API usage cost in the taskbar. Uses the official documented [Usage & Cost Admin API](https://platform.claude.com/docs/en/manage-claude/usage-cost-api).

**Auth:** Admin API Key (`sk-ant-admin-...`) — created in Claude Console under Organization Settings.

### API

```
GET https://api.anthropic.com/v1/organizations/cost_report
Headers: X-Api-Key: sk-ant-admin-...
         anthropic-version: 2023-06-01
```

Returns month-to-date cost in USD. The Cost API is the only official billing endpoint — Anthropic has no balance/credit API (postpaid billing).

### Display

| Auth mode | Taskbar example | Budget configured |
|-----------|----------------|-------------------|
| Admin Key + budget | `Claude: $47.21 / $120.00` | ✅ Yes |
| Admin Key only | `Claude: $47.21` | ❌ No |
| No key | `No Key` | — |

### Installation

1. Download `ClaudeBalance.dll` from [Releases](https://github.com/Likhixang/AILiv/releases)
2. Copy to `<TrafficMonitor>/plugins/`
3. Restart TrafficMonitor, enable in **Plugin Management**
4. Right-click → **Options** / **Settings** → enter your Admin API Key

### Configuration

Stored in `ClaudeBalance.ini` under TrafficMonitor's config directory:

```ini
[Settings]
AdminApiKey=sk-ant-admin-...
MonthlyBudget=120
FetchInterval=30
```

## Gemini Balance

Displays Google Gemini API usage in the taskbar. Uses a **GCP Service Account** to authenticate with Google Cloud Monitoring API — the same approach Google Gemini CLI / agy uses under the hood (OAuth2 via JWT assertion).

### Auth setup (one-time)

1. Go to [GCP Console → IAM → Service Accounts](https://console.cloud.google.com/iam-admin/serviceaccounts)
2. Create a service account → assign **Monitoring Viewer** role
3. Create a JSON key → download
4. Open the JSON file in a text editor — copy the **entire content**
5. Paste it into the plugin settings dialog (multiline edit field)

The plugin parses the JSON, generates a JWT, exchanges it for an OAuth2 token, and queries:
```
GET /v3/projects/{project}/timeSeries
  metric.type = generativeai.googleapis.com/request_count
```

Returns: month-to-date request count (shown as `12.5K req`).

### Display

| Auth mode | Taskbar example | Budget configured |
|-----------|----------------|-------------------|
| Service Account + budget | `Gemini: 12.5K req \| $120 cap` | ✅ Yes |
| Service Account only | `Gemini: 12.5K req` | ❌ No |
| No key | `No Key` | — |

### Installation

1. Download `GeminiBalance.dll` from [Releases](https://github.com/Likhixang/AILiv/releases)
2. Copy to `<TrafficMonitor>/plugins/`
3. Restart TrafficMonitor, enable in **Plugin Management**
4. Right-click → **Options** / **Settings** → paste GCP Service Account JSON key

### Configuration

Stored in `GeminiBalance.ini` under TrafficMonitor's config directory:

```ini
[Settings]
SaJson={"type":"service_account","project_id":"...","private_key":"-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----\n","client_email":"...@....iam.gserviceaccount.com",...}
MonthlyBudget=120
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
