#define WIN32_LEAN_AND_MEAN
#include "PluginMiniMaxBalance.h"
#include "resource.h"
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

PluginMiniMaxBalance PluginMiniMaxBalance::m_instance;

// ── Lifecycle ──────────────────────────────────────────────────

PluginMiniMaxBalance::PluginMiniMaxBalance() {}

PluginMiniMaxBalance& PluginMiniMaxBalance::Instance()
{
    return m_instance;
}

IPluginItem* PluginMiniMaxBalance::GetItem(int index)
{
    if (index == 0) return &m_balance_item;
    return nullptr;
}

const wchar_t* PluginMiniMaxBalance::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:          return L"MiniMax Balance";
    case TMI_DESCRIPTION:   return L"Display MiniMax Token Plan usage in TrafficMonitor";
    case TMI_AUTHOR:        return L"AI Liv";
    case TMI_COPYRIGHT:     return L"Copyright (C) 2026";
    case TMI_VERSION:       return L"1.0";
    case TMI_URL:           return L"https://github.com/Likhixang/AILiv";
    default:                return L"";
    }
}

void PluginMiniMaxBalance::OnInitialize(ITrafficMonitor* pApp)
{
    m_app = pApp;
}

void PluginMiniMaxBalance::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    if (index == EI_CONFIG_DIR)
    {
        m_config_dir = data;
        LoadConfig();
    }
}

// ── Config Persistence ─────────────────────────────────────────

void PluginMiniMaxBalance::LoadConfig()
{
    if (m_config_dir.empty()) return;

    std::wstring path = m_config_dir + L"\\MiniMaxBalance.ini";
    WCHAR buff[4096] = {};
    GetPrivateProfileStringW(L"Settings", L"ApiKey", L"", buff, 4096, path.c_str());
    m_api_key = buff;

    m_region = GetPrivateProfileIntW(L"Settings", L"Region", 0, path.c_str());

    DWORD budget_dword = GetPrivateProfileIntW(L"Settings", L"MonthlyBudget", 0, path.c_str());
    m_monthly_budget = static_cast<double>(budget_dword);

    DWORD interval = GetPrivateProfileIntW(L"Settings", L"FetchInterval", 30, path.c_str());
    m_fetch_interval_ms = static_cast<ULONGLONG>(interval) * 60000;
}

void PluginMiniMaxBalance::SaveConfig()
{
    if (m_config_dir.empty()) return;

    std::wstring path = m_config_dir + L"\\MiniMaxBalance.ini";
    WritePrivateProfileStringW(L"Settings", L"ApiKey", m_api_key.c_str(), path.c_str());

    WCHAR buff[64] = {};
    swprintf_s(buff, L"%d", m_region);
    WritePrivateProfileStringW(L"Settings", L"Region", buff, path.c_str());

    swprintf_s(buff, L"%.0f", m_monthly_budget);
    WritePrivateProfileStringW(L"Settings", L"MonthlyBudget", buff, path.c_str());

    swprintf_s(buff, L"%llu", m_fetch_interval_ms / 60000);
    WritePrivateProfileStringW(L"Settings", L"FetchInterval", buff, path.c_str());
}

// ── HTTP ───────────────────────────────────────────────────────

bool PluginMiniMaxBalance::HttpGet(const std::wstring& host, const std::wstring& path,
                                    const std::wstring& bearer, std::string& out)
{
    HINTERNET hSession = WinHttpOpen(L"AILiv/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     NULL, NULL, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
                                        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                            NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    WinHttpSetTimeouts(hRequest, 5000, 5000, 5000, 10000);

    if (!bearer.empty())
    {
        std::wstring auth = L"Authorization: Bearer " + bearer;
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
            out.append(tmp, bytes_read);
        ok = !out.empty();
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

// ── JSON Extraction ────────────────────────────────────────────

static double ExtractJsonNum(const std::string& json, const std::string& key)
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

// ── API Fetch ──────────────────────────────────────────────────

bool PluginMiniMaxBalance::FetchUsage(std::wstring& out_display)
{
    if (m_api_key.empty()) return false;

    // Select host based on region
    std::wstring host = (m_region == 0) ? L"api.minimax.io" : L"api.minimaxi.com";
    std::wstring path = L"/v1/token_plan/remains";

    std::string response;
    if (!HttpGet(host, path, m_api_key, response))
        return false;

    // Handle error response: check for "base_resp" with non-zero status_code
    if (response.find("\"base_resp\"") != std::string::npos)
    {
        double status_code = ExtractJsonNum(response, "status_code");
        if (status_code != 0.0 && status_code != 200.0)
            return false;
    }

    // Parse the response
    // Fields: current_interval_usage_count (remaining), current_interval_total_count
    //         current_weekly_usage_count (remaining), current_weekly_total_count
    //         next_reset_time
    // Note: "usage_count" fields actually return REMAINING, not used.
    double interval_rem = ExtractJsonNum(response, "current_interval_usage_count");
    double interval_total = ExtractJsonNum(response, "current_interval_total_count");

    if (interval_total <= 0.0)
        return false;

    double interval_used = interval_total - interval_rem;
    int pct = static_cast<int>((interval_used / interval_total) * 100.0);

    wchar_t buf[128] = {};

    if (m_monthly_budget > 0.0)
    {
        swprintf_s(buf, L"%d%% | $%.0f cap", pct, m_monthly_budget);
    }
    else
    {
        // Format: "650/1500 req" or "43%"
        if (interval_total >= 1000.0)
        {
            // Show as percentage
            swprintf_s(buf, L"%d%%", pct);
        }
        else
        {
            // Show as fraction for smaller numbers
            swprintf_s(buf, L"%.0f/%.0f", interval_used, interval_total);
        }
    }

    out_display = buf;
    return true;
}

// ── Data Refresh ──────────────────────────────────────────────

void PluginMiniMaxBalance::DataRequired()
{
    ULONGLONG now = GetTickCount64();
    if (now - m_last_fetch_time < m_fetch_interval_ms) return;
    m_last_fetch_time = now;

    if (m_api_key.empty())
    {
        m_balance_item.SetDisplayText(L"No Key");
        return;
    }

    std::wstring display;
    if (FetchUsage(display))
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
    static PluginMiniMaxBalance* pPlugin = nullptr;

    switch (msg)
    {
    case WM_INITDIALOG:
        pPlugin = reinterpret_cast<PluginMiniMaxBalance*>(lParam);
        if (pPlugin)
        {
            SetDlgItemTextW(hDlg, IDC_API_KEY_EDIT, pPlugin->GetApiKey().c_str());

            HWND hCombo = GetDlgItem(hDlg, IDC_REGION_COMBO);
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"International (minimax.io)");
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"China (minimaxi.com)");
            SendMessageW(hCombo, CB_SETCURSEL, pPlugin->GetRegion(), 0);

            WCHAR buff[64] = {};
            swprintf_s(buff, L"%.0f", pPlugin->GetMonthlyBudget());
            SetDlgItemTextW(hDlg, IDC_BUDGET_EDIT, buff);

            swprintf_s(buff, L"%llu", pPlugin->GetFetchIntervalMs() / 60000);
            SetDlgItemTextW(hDlg, IDC_FETCH_INTERVAL_EDIT, buff);

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
            if (pPlugin) { pPlugin->ForceRefresh(); pPlugin->DataRequired(); }
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

ITMPlugin::OptionReturn PluginMiniMaxBalance::ShowOptionsDialog(void* hParent)
{
    INT_PTR ret = DialogBoxParamW(m_hModule,
                                  MAKEINTRESOURCEW(IDD_SETTINGS_DIALOG),
                                  static_cast<HWND>(hParent),
                                  SettingsDlgProc,
                                  reinterpret_cast<LPARAM>(this));
    if (ret == IDOK) return OR_OPTION_CHANGED;
    return OR_OPTION_UNCHANGED;
}

// ── Plugin Entry ──────────────────────────────────────────────

ITMPlugin* TMPluginGetInstance()
{
    return &PluginMiniMaxBalance::Instance();
}
