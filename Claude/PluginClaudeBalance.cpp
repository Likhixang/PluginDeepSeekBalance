#define WIN32_LEAN_AND_MEAN
#include "PluginClaudeBalance.h"
#include "resource.h"
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

PluginClaudeBalance PluginClaudeBalance::m_instance;

// ── Lifecycle ──────────────────────────────────────────────────

PluginClaudeBalance::PluginClaudeBalance() {}

PluginClaudeBalance& PluginClaudeBalance::Instance()
{
    return m_instance;
}

IPluginItem* PluginClaudeBalance::GetItem(int index)
{
    if (index == 0)
        return &m_balance_item;
    return nullptr;
}

const wchar_t* PluginClaudeBalance::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return L"Claude Balance";
    case TMI_DESCRIPTION:
        return L"Display Anthropic Claude API usage & cost in TrafficMonitor";
    case TMI_AUTHOR:
        return L"AI Liv";
    case TMI_COPYRIGHT:
        return L"Copyright (C) 2026";
    case TMI_VERSION:
        return L"1.0";
    case TMI_URL:
        return L"https://github.com/Likhixang/AILiv";
    default:
        return L"";
    }
}

void PluginClaudeBalance::OnInitialize(ITrafficMonitor* pApp)
{
    m_app = pApp;
}

void PluginClaudeBalance::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    if (index == EI_CONFIG_DIR)
    {
        m_config_dir = data;
        LoadConfig();
    }
}

// ── Config ─────────────────────────────────────────────────────

void PluginClaudeBalance::LoadConfig()
{
    if (m_config_dir.empty())
        return;

    std::wstring path = m_config_dir + L"\\ClaudeBalance.ini";
    WCHAR buff[4096] = {};

    GetPrivateProfileStringW(L"Settings", L"AdminApiKey", L"", buff, 4096, path.c_str());
    m_api_key = buff;

    DWORD budget_dword = GetPrivateProfileIntW(L"Settings", L"MonthlyBudget", 0, path.c_str());
    m_monthly_budget = static_cast<double>(budget_dword);

    DWORD interval = GetPrivateProfileIntW(L"Settings", L"FetchInterval", 30, path.c_str());
    m_fetch_interval_ms = static_cast<ULONGLONG>(interval) * 60000;
}

void PluginClaudeBalance::SaveConfig()
{
    if (m_config_dir.empty())
        return;

    std::wstring path = m_config_dir + L"\\ClaudeBalance.ini";
    WritePrivateProfileStringW(L"Settings", L"AdminApiKey", m_api_key.c_str(), path.c_str());

    WCHAR buff[64] = {};
    swprintf_s(buff, L"%.0f", m_monthly_budget);
    WritePrivateProfileStringW(L"Settings", L"MonthlyBudget", buff, path.c_str());

    swprintf_s(buff, L"%llu", m_fetch_interval_ms / 60000);
    WritePrivateProfileStringW(L"Settings", L"FetchInterval", buff, path.c_str());
}

// ── HTTP ───────────────────────────────────────────────────────

bool PluginClaudeBalance::HttpGet(const std::wstring& path,
                                  const std::wstring& api_key,
                                  std::vector<char>& out_response)
{
    HINTERNET hSession = WinHttpOpen(L"AILiv/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     NULL, NULL, 0);
    if (!hSession)
        return false;

    HINTERNET hConnect = WinHttpConnect(hSession, L"api.anthropic.com",
                                        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                            NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    WinHttpSetTimeouts(hRequest, 5000, 5000, 5000, 10000);

    // Anthropic requires X-Api-Key header + anthropic-version header
    std::wstring auth_header = L"X-Api-Key: " + api_key;
    WinHttpAddRequestHeaders(hRequest, auth_header.c_str(),
                             (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hRequest, L"anthropic-version: 2023-06-01",
                             (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);

    bool success = false;
    if (WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL))
    {
        char tmp[4096] = {};
        DWORD bytes_read = 0;
        while (WinHttpReadData(hRequest, tmp, sizeof(tmp) - 1, &bytes_read) && bytes_read > 0)
        {
            out_response.insert(out_response.end(), tmp, tmp + bytes_read);
        }
        success = !out_response.empty();
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return success;
}

// ── Cost Report Parsing ────────────────────────────────────────

double PluginClaudeBalance::SumCostAmounts(const std::vector<char>& utf8_data)
{
    // cost_report response: results[].amount is a decimal string in cents
    // e.g., "amount": "123.45678" means 123.45678 cents = $1.23
    // We sum all amount values then divide by 100.
    std::string text(utf8_data.begin(), utf8_data.end());
    double total_cents = 0.0;

    std::string search = "\"amount\":";
    size_t pos = 0;
    while ((pos = text.find(search, pos)) != std::string::npos)
    {
        pos += search.length();
        // Skip whitespace
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '"'))
            pos++;
        size_t end = pos;
        while (end < text.size() && text[end] != '"' && text[end] != ',' &&
               text[end] != '}' && text[end] != ']' && text[end] != ' ')
            end++;
        std::string num_str = text.substr(pos, end - pos);
        char* endptr = nullptr;
        double val = strtod(num_str.c_str(), &endptr);
        if (endptr != num_str.c_str())
            total_cents += val;
    }

    return total_cents;
}

// ── Data Fetch ─────────────────────────────────────────────────

bool PluginClaudeBalance::FetchCost(double& out_cents)
{
    // Compute month-to-date timestamps in RFC 3339 format
    SYSTEMTIME st;
    GetSystemTime(&st);

    wchar_t start_buf[32] = {};
    swprintf_s(start_buf, L"%04d-%02d-01T00:00:00Z", st.wYear, st.wMonth);

    wchar_t end_buf[32] = {};
    swprintf_s(end_buf, L"%04d-%02d-%02dT23:59:59Z", st.wYear, st.wMonth, st.wDay);

    wchar_t path[256] = {};
    swprintf_s(path,
        L"/v1/organizations/cost_report?"
        L"starting_at=%s&ending_at=%s&bucket_width=1d&limit=31",
        start_buf, end_buf);

    std::vector<char> resp;
    if (!HttpGet(path, m_api_key, resp))
        return false;

    out_cents = SumCostAmounts(resp);
    return true;
}

// ── Data Refresh ──────────────────────────────────────────────

void PluginClaudeBalance::DataRequired()
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

    double cents = 0.0;
    if (FetchCost(cents))
    {
        double usd = cents / 100.0;
        std::wstring display;
        wchar_t buf[128] = {};

        if (m_monthly_budget > 0.0)
        {
            swprintf_s(buf, L"$%.2f / $%.2f", usd, m_monthly_budget);
            display = buf;
        }
        else
        {
            swprintf_s(buf, L"$%.2f", usd);
            display = buf;
        }

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
    static PluginClaudeBalance* pPlugin = nullptr;

    switch (msg)
    {
    case WM_INITDIALOG:
        pPlugin = reinterpret_cast<PluginClaudeBalance*>(lParam);
        if (pPlugin)
        {
            SetDlgItemTextW(hDlg, IDC_API_KEY_EDIT, pPlugin->GetApiKey().c_str());

            WCHAR buff[64] = {};
            swprintf_s(buff, L"%.0f", pPlugin->GetMonthlyBudget());
            SetDlgItemTextW(hDlg, IDC_BUDGET_EDIT, buff);

            swprintf_s(buff, L"%llu", pPlugin->GetFetchIntervalMs() / 60000);
            SetDlgItemTextW(hDlg, IDC_FETCH_INTERVAL_EDIT, buff);
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

ITMPlugin::OptionReturn PluginClaudeBalance::ShowOptionsDialog(void* hParent)
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

// ── Plugin Entry Point ────────────────────────────────────────

ITMPlugin* TMPluginGetInstance()
{
    return &PluginClaudeBalance::Instance();
}
