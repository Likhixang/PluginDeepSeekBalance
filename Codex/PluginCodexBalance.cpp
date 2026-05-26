#define WIN32_LEAN_AND_MEAN
#include "PluginCodexBalance.h"
#include "resource.h"
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

PluginCodexBalance PluginCodexBalance::m_instance;

// ── Lifecycle ──────────────────────────────────────────────────

PluginCodexBalance::PluginCodexBalance() {}

PluginCodexBalance& PluginCodexBalance::Instance()
{
    return m_instance;
}

IPluginItem* PluginCodexBalance::GetItem(int index)
{
    if (index == 0)
        return &m_balance_item;
    return nullptr;
}

const wchar_t* PluginCodexBalance::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return L"Codex Balance";
    case TMI_DESCRIPTION:
        return L"Display OpenAI / Codex CLI usage & balance in TrafficMonitor";
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

void PluginCodexBalance::OnInitialize(ITrafficMonitor* pApp)
{
    m_app = pApp;
}

void PluginCodexBalance::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    if (index == EI_CONFIG_DIR)
    {
        m_config_dir = data;
        LoadConfig();
    }
}

// ── Config Persistence ────────────────────────────────────────

void PluginCodexBalance::LoadConfig()
{
    if (m_config_dir.empty())
        return;

    std::wstring path = m_config_dir + L"\\CodexBalance.ini";
    WCHAR buff[4096] = {};

    // Session cookie (priority 1)
    GetPrivateProfileStringW(L"Settings", L"SessionCookie", L"", buff, 4096, path.c_str());
    m_session_cookie = buff;

    // API key (priority 2 fallback)
    GetPrivateProfileStringW(L"Settings", L"ApiKey", L"", buff, 4096, path.c_str());
    m_api_key = buff;

    // Monthly budget
    DWORD budget_dword = GetPrivateProfileIntW(L"Settings", L"MonthlyBudget", 0, path.c_str());
    m_monthly_budget = static_cast<double>(budget_dword);

    // Refresh interval
    DWORD interval = GetPrivateProfileIntW(L"Settings", L"FetchInterval", 30, path.c_str());
    m_fetch_interval_ms = static_cast<ULONGLONG>(interval) * 60000;
}

void PluginCodexBalance::SaveConfig()
{
    if (m_config_dir.empty())
        return;

    std::wstring path = m_config_dir + L"\\CodexBalance.ini";
    WritePrivateProfileStringW(L"Settings", L"SessionCookie", m_session_cookie.c_str(), path.c_str());
    WritePrivateProfileStringW(L"Settings", L"ApiKey", m_api_key.c_str(), path.c_str());

    WCHAR buff[64] = {};
    swprintf_s(buff, L"%.0f", m_monthly_budget);
    WritePrivateProfileStringW(L"Settings", L"MonthlyBudget", buff, path.c_str());

    swprintf_s(buff, L"%llu", m_fetch_interval_ms / 60000);
    WritePrivateProfileStringW(L"Settings", L"FetchInterval", buff, path.c_str());
}

// ── Date Helpers ───────────────────────────────────────────────

void PluginCodexBalance::GetMonthDateRange(std::wstring& out_start_date,
                                           std::wstring& out_end_date)
{
    SYSTEMTIME st;
    GetSystemTime(&st);

    wchar_t start_buf[32] = {};
    swprintf_s(start_buf, L"%04d-%02d-%02d", st.wYear, st.wMonth, 1);
    out_start_date = start_buf;

    wchar_t end_buf[32] = {};
    swprintf_s(end_buf, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    out_end_date = end_buf;
}

// ── JSON Parsing ──────────────────────────────────────────────

static std::wstring ExtractJsonString(const std::vector<char>& utf8_data,
                                      const std::string& key)
{
    std::string search = "\"" + key + "\"";
    std::string text(utf8_data.begin(), utf8_data.end());

    size_t pos = text.find(search);
    if (pos == std::string::npos)
        return {};

    pos = text.find('"', pos + search.length());
    if (pos == std::string::npos)
        return {};

    size_t end = text.find('"', pos + 1);
    if (end == std::string::npos)
        return {};

    std::string val_utf8 = text.substr(pos + 1, end - pos - 1);

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, val_utf8.c_str(),
                                       (int)val_utf8.size(), nullptr, 0);
    if (wide_len <= 0)
        return {};

    std::wstring result(wide_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, val_utf8.c_str(),
                        (int)val_utf8.size(), &result[0], wide_len);
    return result;
}

static double ExtractJsonNumber(const std::vector<char>& utf8_data,
                                const std::string& key)
{
    std::string search = "\"" + key + "\"";
    std::string text(utf8_data.begin(), utf8_data.end());

    size_t pos = text.find(search);
    if (pos == std::string::npos)
        return 0.0;

    pos = text.find(':', pos + search.length());
    if (pos == std::string::npos)
        return 0.0;

    // Skip whitespace
    while (pos < text.size() && (text[pos] == ':' || text[pos] == ' '))
        pos++;

    // Read until comma, close-brace, or whitespace
    size_t end = pos;
    while (end < text.size() && text[end] != ',' && text[end] != '}' &&
           text[end] != ']' && text[end] != ' ')
        end++;

    std::string num_str = text.substr(pos, end - pos);
    char* endptr = nullptr;
    double val = strtod(num_str.c_str(), &endptr);
    return (endptr == num_str.c_str()) ? 0.0 : val;
}

static double ExtractTotalUsageFromCosts(const std::vector<char>& utf8_data)
{
    // The /organization/costs response has nested buckets → results → amount.value
    // We sum all amount.value across all buckets
    std::string text(utf8_data.begin(), utf8_data.end());
    double total = 0.0;

    // Simple strategy: find all "amount" objects and sum "value" fields
    std::string search = "\"value\":";
    size_t pos = 0;
    while ((pos = text.find(search, pos)) != std::string::npos)
    {
        pos += search.length();
        // skip whitespace
        while (pos < text.size() && text[pos] == ' ')
            pos++;
        size_t end = pos;
        while (end < text.size() && text[end] != ',' && text[end] != '}' &&
               text[end] != ']' && text[end] != ' ')
            end++;
        std::string num_str = text.substr(pos, end - pos);
        char* endptr = nullptr;
        double val = strtod(num_str.c_str(), &endptr);
        if (endptr != num_str.c_str())
            total += val;
    }
    return total;
}

// ── HTTP Low-Level ────────────────────────────────────────────

bool PluginCodexBalance::HttpGet(const std::wstring& host,
                                 const std::wstring& path,
                                 bool https,
                                 const std::wstring& cookie,
                                 const std::wstring& bearer_token,
                                 std::vector<char>& out_response)
{
    HINTERNET hSession = WinHttpOpen(L"AILiv/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     NULL, NULL, 0);
    if (!hSession)
        return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
                                        https ? INTERNET_DEFAULT_HTTPS_PORT
                                              : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                            NULL, NULL, NULL, flags);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    WinHttpSetTimeouts(hRequest, 5000, 5000, 5000, 10000);

    // Add auth headers
    if (!cookie.empty())
    {
        std::wstring cookie_header = L"Cookie: " + cookie;
        WinHttpAddRequestHeaders(hRequest, cookie_header.c_str(),
                                 (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
    }
    else if (!bearer_token.empty())
    {
        std::wstring auth_header = L"Authorization: Bearer " + bearer_token;
        WinHttpAddRequestHeaders(hRequest, auth_header.c_str(),
                                 (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
    }

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

// ── Priority 1: Session Cookie ────────────────────────────────

bool PluginCodexBalance::FetchWithSessionCookie(double& out_spent,
                                                double& out_balance)
{
    std::wstring start_date, end_date;
    GetMonthDateRange(start_date, end_date);

    // 1) Get usage/costs
    std::wstring usage_path = L"/dashboard/billing/usage?start_date="
                              + start_date + L"&end_date=" + end_date;
    std::vector<char> resp;
    if (!HttpGet(L"api.openai.com", usage_path, true,
                 m_session_cookie, L"", resp))
    {
        return false;
    }

    out_spent = ExtractJsonNumber(resp, "total_usage");

    // 2) Get subscription info (balance/credits)
    std::vector<char> sub_resp;
    if (HttpGet(L"api.openai.com", L"/dashboard/billing/subscription",
                true, m_session_cookie, L"", sub_resp))
    {
        out_balance = ExtractJsonNumber(sub_resp, "account_balance");
    }

    return true;
}

// ── Priority 2: API Key ───────────────────────────────────────

bool PluginCodexBalance::FetchWithApiKey(double& out_spent)
{
    // Compute Unix timestamps for the current month (UTC)
    SYSTEMTIME st;
    GetSystemTime(&st);

    // Start of month: year-month-01 00:00:00 UTC
    SYSTEMTIME st_start = {};
    st_start.wYear = st.wYear;
    st_start.wMonth = st.wMonth;
    st_start.wDay = 1;

    FILETIME ft_start;
    SystemTimeToFileTime(&st_start, &ft_start);
    ULONGLONG start_100ns = (static_cast<ULONGLONG>(ft_start.dwHighDateTime) << 32)
                            | ft_start.dwLowDateTime;
    ULONGLONG start_unix = (start_100ns - 116444736000000000ULL) / 10000000;

    // End: current time
    FILETIME ft_now;
    GetSystemTimeAsFileTime(&ft_now);
    ULONGLONG now_100ns = (static_cast<ULONGLONG>(ft_now.dwHighDateTime) << 32)
                          | ft_now.dwLowDateTime;
    ULONGLONG now_unix = (now_100ns - 116444736000000000ULL) / 10000000;

    wchar_t path_buf[256] = {};
    swprintf_s(path_buf,
        L"/v1/organization/costs?start_time=%llu&end_time=%llu&bucket_width=1d&limit=31",
        start_unix, now_unix);

    std::vector<char> resp;
    if (!HttpGet(L"api.openai.com", path_buf, true,
                 L"", m_api_key, resp))
    {
        return false;
    }

    out_spent = ExtractTotalUsageFromCosts(resp);
    return true;
}

// ── Data Refresh ──────────────────────────────────────────────

void PluginCodexBalance::DataRequired()
{
    ULONGLONG now = GetTickCount64();
    if (now - m_last_fetch_time < m_fetch_interval_ms)
        return;
    m_last_fetch_time = now;

    // ── Priority 1: Session Cookie ──
    if (!m_session_cookie.empty())
    {
        double spent = 0.0, balance = 0.0;
        if (FetchWithSessionCookie(spent, balance))
        {
            std::wstring display;
            wchar_t buf[128] = {};

            if (m_monthly_budget > 0.0)
            {
                // Show: spent / budget
                swprintf_s(buf, L"$%.2f / $%.2f", spent, m_monthly_budget);
                display = buf;
            }
            else if (balance > 0.0)
            {
                // Show: spent, remaining = balance
                double remaining = balance - spent;
                swprintf_s(buf, L"$%.2f | $%.2f left", spent, remaining);
                display = buf;
            }
            else
            {
                swprintf_s(buf, L"$%.2f", spent);
                display = buf;
            }

            m_balance_item.SetDisplayText(display);
            return;
        }
    }

    // ── Priority 2: API Key (fallback) ──
    if (!m_api_key.empty())
    {
        double spent = 0.0;
        if (FetchWithApiKey(spent))
        {
            std::wstring display;
            wchar_t buf[64] = {};

            if (m_monthly_budget > 0.0)
            {
                swprintf_s(buf, L"$%.2f / $%.2f", spent, m_monthly_budget);
                display = buf;
            }
            else
            {
                swprintf_s(buf, L"$%.2f", spent);
                display = buf;
            }

            m_balance_item.SetDisplayText(display);
            return;
        }
    }

    // ── No auth configured or all requests failed ──
    if (m_session_cookie.empty() && m_api_key.empty())
        m_balance_item.SetDisplayText(L"No Auth");
    else
        m_balance_item.SetDisplayText(L"ERR");
}

// ── Settings Dialog ───────────────────────────────────────────

static INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static PluginCodexBalance* pPlugin = nullptr;

    switch (msg)
    {
    case WM_INITDIALOG:
        pPlugin = reinterpret_cast<PluginCodexBalance*>(lParam);
        if (pPlugin)
        {
            SetDlgItemTextW(hDlg, IDC_SESSION_COOKIE_EDIT,
                            pPlugin->GetSessionCookie().c_str());
            SetDlgItemTextW(hDlg, IDC_API_KEY_EDIT,
                            pPlugin->GetApiKey().c_str());

            WCHAR buff[64] = {};
            swprintf_s(buff, L"%.0f", pPlugin->GetMonthlyBudget());
            SetDlgItemTextW(hDlg, IDC_BUDGET_EDIT, buff);

            swprintf_s(buff, L"%llu", pPlugin->GetFetchIntervalMs() / 60000);
            SetDlgItemTextW(hDlg, IDC_FETCH_INTERVAL_EDIT, buff);

            // Show active auth method
            if (!pPlugin->GetSessionCookie().empty())
                SetDlgItemTextW(hDlg, IDC_AUTH_STATUS, L"Auth: Session Cookie (full access)");
            else if (!pPlugin->GetApiKey().empty())
                SetDlgItemTextW(hDlg, IDC_AUTH_STATUS, L"Auth: API Key (usage only, no balance)");
            else
                SetDlgItemTextW(hDlg, IDC_AUTH_STATUS, L"Auth: None configured");
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

        case IDC_LOGIN_BUTTON:
            ShellExecuteW(nullptr, L"open",
                          L"https://platform.openai.com/auth/login",
                          nullptr, nullptr, SW_SHOW);
            return TRUE;

        case IDOK:
            if (pPlugin)
            {
                WCHAR buff[4096] = {};
                GetDlgItemTextW(hDlg, IDC_SESSION_COOKIE_EDIT, buff, 4096);
                pPlugin->SetSessionCookie(buff);

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

ITMPlugin::OptionReturn PluginCodexBalance::ShowOptionsDialog(void* hParent)
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
    return &PluginCodexBalance::Instance();
}
