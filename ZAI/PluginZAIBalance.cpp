#define WIN32_LEAN_AND_MEAN
#include "PluginZAIBalance.h"
#include "resource.h"
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

PluginZAIBalance PluginZAIBalance::m_instance;

// ── Lifecycle ──────────────────────────────────────────────────

PluginZAIBalance::PluginZAIBalance() {}

PluginZAIBalance& PluginZAIBalance::Instance()
{
    return m_instance;
}

IPluginItem* PluginZAIBalance::GetItem(int index)
{
    if (index == 0)
        return &m_balance_item;
    return nullptr;
}

const wchar_t* PluginZAIBalance::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:          return L"Z.AI Balance";
    case TMI_DESCRIPTION:   return L"Display Z.AI (GLM Coding Plan) token usage in TrafficMonitor";
    case TMI_AUTHOR:        return L"AI Liv";
    case TMI_COPYRIGHT:     return L"Copyright (C) 2026";
    case TMI_VERSION:       return L"1.0";
    case TMI_URL:           return L"https://github.com/Likhixang/AILiv";
    default:                return L"";
    }
}

void PluginZAIBalance::OnInitialize(ITrafficMonitor* pApp)
{
    m_app = pApp;
}

void PluginZAIBalance::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    if (index == EI_CONFIG_DIR)
    {
        m_config_dir = data;
        LoadConfig();
    }
}

// ── Config Persistence ─────────────────────────────────────────

void PluginZAIBalance::LoadConfig()
{
    if (m_config_dir.empty())
        return;

    std::wstring path = m_config_dir + L"\\ZAIBalance.ini";
    WCHAR buff[4096] = {};
    GetPrivateProfileStringW(L"Settings", L"ApiKey", L"", buff, 4096, path.c_str());
    m_api_key = buff;

    m_region = GetPrivateProfileIntW(L"Settings", L"Region", 0, path.c_str());

    DWORD budget_dword = GetPrivateProfileIntW(L"Settings", L"MonthlyBudget", 0, path.c_str());
    m_monthly_budget = static_cast<double>(budget_dword);

    DWORD interval = GetPrivateProfileIntW(L"Settings", L"FetchInterval", 30, path.c_str());
    m_fetch_interval_ms = static_cast<ULONGLONG>(interval) * 60000;
}

void PluginZAIBalance::SaveConfig()
{
    if (m_config_dir.empty())
        return;

    std::wstring path = m_config_dir + L"\\ZAIBalance.ini";
    WritePrivateProfileStringW(L"Settings", L"ApiKey", m_api_key.c_str(), path.c_str());

    WCHAR buff[64] = {};
    swprintf_s(buff, L"%d", m_region);
    WritePrivateProfileStringW(L"Settings", L"Region", buff, path.c_str());

    swprintf_s(buff, L"%.0f", m_monthly_budget);
    WritePrivateProfileStringW(L"Settings", L"MonthlyBudget", buff, path.c_str());

    swprintf_s(buff, L"%llu", m_fetch_interval_ms / 60000);
    WritePrivateProfileStringW(L"Settings", L"FetchInterval", buff, path.c_str());
}

// ── JSON Extraction ────────────────────────────────────────────

std::string PluginZAIBalance::ExtractJsonStr(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + search.length());
    if (pos == std::string::npos) return {};
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return {};
    return json.substr(pos + 1, end - pos - 1);
}

double PluginZAIBalance::ExtractJsonNum(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0.0;
    pos = json.find(':', pos + search.length());
    if (pos == std::string::npos) return 0.0;
    while (pos < json.size() && (json[pos] == ':' || json[pos] == ' '))
        pos++;
    size_t end = pos;
    while (end < json.size() && json[end] != ',' && json[end] != '}' &&
           json[end] != ']' && json[end] != ' ')
        end++;
    std::string num_str = json.substr(pos, end - pos);
    char* endptr = nullptr;
    double val = strtod(num_str.c_str(), &endptr);
    return (endptr == num_str.c_str()) ? 0.0 : val;
}

int PluginZAIBalance::ExtractJsonInt(const std::string& json, const std::string& key)
{
    return static_cast<int>(ExtractJsonNum(json, key));
}

// ── HTTP ───────────────────────────────────────────────────────

static bool WinHttpGet(const std::wstring& host, const std::wstring& path,
                       bool https, const std::wstring& bearer_token,
                       std::string& out)
{
    HINTERNET hSession = WinHttpOpen(L"AILiv/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     NULL, NULL, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
                                        https ? INTERNET_DEFAULT_HTTPS_PORT
                                              : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                            NULL, NULL, NULL, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    WinHttpSetTimeouts(hRequest, 5000, 5000, 5000, 10000);

    if (!bearer_token.empty())
    {
        std::wstring auth = L"Authorization: Bearer " + bearer_token;
        WinHttpAddRequestHeaders(hRequest, auth.c_str(),
                                 (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
    }

    bool ok = false;
    if (WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL))
    {
        char tmp[4096] = {};
        DWORD bytes_read = 0;
        while (WinHttpReadData(hRequest, tmp, sizeof(tmp) - 1, &bytes_read) && bytes_read > 0)
        {
            out.append(tmp, bytes_read);
        }
        ok = !out.empty();
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

// ── API Fetch ──────────────────────────────────────────────────

bool PluginZAIBalance::FetchQuota(std::wstring& out_display, int& out_pct)
{
    if (m_api_key.empty())
        return false;

    // Select host and path based on region
    std::wstring host = (m_region == 0) ? L"api.z.ai" : L"open.bigmodel.cn";
    std::wstring path = (m_region == 0)
        ? L"/api/monitor/usage/quota/limit"
        : L"/api/monitor/usage/quota/limit";

    std::string response;
    if (!WinHttpGet(host, path, true, m_api_key, response))
        return false;

    // Parse the response
    // Expected structure:
    // {
    //   "data": {
    //     "level": "lite" | "standard" | "pro",
    //     "planName": "Coding Plan Lite",
    //     "limits": [
    //       {
    //         "type": "TOKENS_LIMIT",
    //         "percentage": 45,
    //         "remaining": 5500000,
    //         "used": 4500000,
    //         "total": 10000000,
    //         "nextResetTime": 1717000000000,
    //         "usageDetails": [ ... ]
    //       },
    //       {
    //         "type": "TIME_LIMIT",
    //         "percentage": 30,
    //         ...
    //       }
    //     ]
    //   }
    // }

    // Find the "data" object
    std::string data_key = "\"data\":{";
    size_t data_pos = response.find(data_key);
    if (data_pos == std::string::npos)
    {
        // Try with space: "data" : {
        data_key = "\"data\": {";
        data_pos = response.find(data_key);
        if (data_pos == std::string::npos)
            return false;
    }

    // Extract the data object - find matching braces
    std::string data_obj = "{" + data_key.substr(data_key.find('{'));
    // Actually just take from the first { after "data":
    // This is getting complex. Let's just search for "limits" within the data section.
    // Simpler approach: just search for specific keys in the full response

    // Extract level/plan
    std::string level = ExtractJsonStr(response, "level");
    std::string plan_name = ExtractJsonStr(response, "planName");
    if (plan_name.empty()) plan_name = "Coding Plan";

    // Find the TOKENS_LIMIT block
    std::string tokens_marker = "\"TOKENS_LIMIT\"";
    size_t tokens_pos = response.find(tokens_marker);

    if (tokens_pos == std::string::npos)
    {
        // No token limit found, try TIMES_LIMIT as fallback
        tokens_marker = "\"TIMES_LIMIT\"";
        tokens_pos = response.find(tokens_marker);
        if (tokens_pos == std::string::npos)
            return false;
    }

    // Find the enclosing limit block
    // Go backwards to find the opening { of this limit entry
    size_t limit_start = response.rfind('{', tokens_pos);
    if (limit_start == std::string::npos) return false;

    // Find the closing } of this limit entry
    int brace_depth = 0;
    size_t limit_end = limit_start;
    for (size_t i = limit_start; i < response.size(); i++)
    {
        if (response[i] == '{') brace_depth++;
        else if (response[i] == '}') brace_depth--;
        if (brace_depth == 0) { limit_end = i; break; }
    }

    std::string limit_block = response.substr(limit_start, limit_end - limit_start + 1);

    // Extract fields from this limit block using string search within the block
    auto extractNumFromBlock = [&](const std::string& key) -> double {
        std::string s = "\"" + key + "\"";
        size_t p = limit_block.find(s);
        if (p == std::string::npos) return 0.0;
        p = limit_block.find(':', p + s.length());
        if (p == std::string::npos) return 0.0;
        while (p < limit_block.size() && (limit_block[p] == ':' || limit_block[p] == ' '))
            p++;
        size_t e = p;
        while (e < limit_block.size() && limit_block[e] != ',' && limit_block[e] != '}' &&
               limit_block[e] != ']' && limit_block[e] != ' ')
            e++;
        std::string n = limit_block.substr(p, e - p);
        char* end = nullptr;
        double v = strtod(n.c_str(), &end);
        return (end == n.c_str()) ? 0.0 : v;
    };

    double pct = extractNumFromBlock("percentage");
    double remaining = extractNumFromBlock("remaining");
    double used = extractNumFromBlock("used");
    double total = extractNumFromBlock("total");

    out_pct = static_cast<int>(pct);

    // Format the display
    wchar_t buf[128] = {};

    if (total > 0 && used >= 0)
    {
        // Show: "5.5M/10M" format
        auto formatCount = [](double val) -> std::wstring {
            if (val >= 1000000.0)
            {
                wchar_t b[32];
                swprintf_s(b, L"%.1fM", val / 1000000.0);
                return b;
            }
            if (val >= 1000.0)
            {
                wchar_t b[32];
                swprintf_s(b, L"%.0fK", val / 1000.0);
                return b;
            }
            return std::to_wstring(static_cast<int>(val));
        };

        std::wstring used_str = formatCount(used);
        std::wstring total_str = formatCount(total);

        if (m_monthly_budget > 0.0)
            swprintf_s(buf, L"%d%% | $%.0f cap", static_cast<int>(pct), m_monthly_budget);
        else
            swprintf_s(buf, L"%s/%s", used_str.c_str(), total_str.c_str());
    }
    else if (remaining > 0)
    {
        if (m_monthly_budget > 0.0)
            swprintf_s(buf, L"%d%% | $%.0f cap", static_cast<int>(pct), m_monthly_budget);
        else
            swprintf_s(buf, L"%d%%", static_cast<int>(pct));
    }
    else
    {
        swprintf_s(buf, L"%d%%", static_cast<int>(pct));
    }

    out_display = buf;
    return true;
}

// ── Data Refresh ──────────────────────────────────────────────

void PluginZAIBalance::DataRequired()
{
    ULONGLONG now = GetTickCount64();
    if (now - m_last_fetch_time < m_fetch_interval_ms)
        return;
    m_last_fetch_time = now;

    if (m_api_key.empty())
    {
        m_balance_item.SetDisplayText(L"No Key");
        return;
    }

    std::wstring display;
    int pct = 0;
    if (FetchQuota(display, pct))
    {
        m_balance_item.SetDisplayText(display);
    }
    else
    {
        m_balance_item.SetDisplayText(L"ERR");
    }
}

// ── Settings Dialog ───────────────────────────────────────────

static INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static PluginZAIBalance* pPlugin = nullptr;

    switch (msg)
    {
    case WM_INITDIALOG:
        pPlugin = reinterpret_cast<PluginZAIBalance*>(lParam);
        if (pPlugin)
        {
            SetDlgItemTextW(hDlg, IDC_API_KEY_EDIT, pPlugin->GetApiKey().c_str());

            // Populate region combo
            HWND hCombo = GetDlgItem(hDlg, IDC_REGION_COMBO);
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"International (api.z.ai)");
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"China (open.bigmodel.cn)");
            SendMessageW(hCombo, CB_SETCURSEL, pPlugin->GetRegion(), 0);

            WCHAR buff[64] = {};
            swprintf_s(buff, L"%.0f", pPlugin->GetMonthlyBudget());
            SetDlgItemTextW(hDlg, IDC_BUDGET_EDIT, buff);

            swprintf_s(buff, L"%llu", pPlugin->GetFetchIntervalMs() / 60000);
            SetDlgItemTextW(hDlg, IDC_FETCH_INTERVAL_EDIT, buff);

            // Status
            if (!pPlugin->GetApiKey().empty())
                SetDlgItemTextW(hDlg, IDC_AUTH_STATUS, L"Status: API Key configured");
            else
                SetDlgItemTextW(hDlg, IDC_AUTH_STATUS, L"Status: No API Key configured");
        }
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_REFRESH_BUTTON:
            if (pPlugin)
            {
                pPlugin->ForceRefresh();
                pPlugin->DataRequired();
            }
            return TRUE;

        case IDOK:
            if (pPlugin)
            {
                WCHAR buff[4096] = {};
                GetDlgItemTextW(hDlg, IDC_API_KEY_EDIT, buff, 4096);
                pPlugin->SetApiKey(buff);

                HWND hCombo = GetDlgItem(hDlg, IDC_REGION_COMBO);
                int sel = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
                if (sel < 0) sel = 0;
                pPlugin->SetRegion(sel);

                GetDlgItemTextW(hDlg, IDC_BUDGET_EDIT, buff, 64);
                double budget = _wtof(buff);
                if (budget < 0.0) budget = 0.0;
                pPlugin->SetMonthlyBudget(budget);

                GetDlgItemTextW(hDlg, IDC_FETCH_INTERVAL_EDIT, buff, 32);
                ULONGLONG interval_min = _wtoi64(buff);
                if (interval_min < 1) interval_min = 1;
                if (interval_min > 1440) interval_min = 1440;
                pPlugin->SetFetchIntervalMs(interval_min * 60000);

                pPlugin->SaveConfig();
                pPlugin->ForceRefresh();
            }
            EndDialog(hDlg, IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

ITMPlugin::OptionReturn PluginZAIBalance::ShowOptionsDialog(void* hParent)
{
    INT_PTR ret = DialogBoxParamW(m_hModule,
                                  MAKEINTRESOURCEW(IDD_SETTINGS_DIALOG),
                                  static_cast<HWND>(hParent),
                                  SettingsDlgProc,
                                  reinterpret_cast<LPARAM>(this));
    if (ret == IDOK)
        return OR_OPTION_CHANGED;
    return OR_OPTION_UNCHANGED;
}

// ── Plugin Entry ──────────────────────────────────────────────

ITMPlugin* TMPluginGetInstance()
{
    return &PluginZAIBalance::Instance();
}
