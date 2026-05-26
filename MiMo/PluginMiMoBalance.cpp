#define WIN32_LEAN_AND_MEAN
#include "PluginMiMoBalance.h"
#include "resource.h"
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

PluginMiMoBalance PluginMiMoBalance::m_instance;

// ── Plan credit limits ─────────────────────────────────────────

double PluginMiMoBalance::GetPlanCredits(int plan, int cycle)
{
    // Monthly: Lite=60M, Standard=200M, Pro=700M, Max=1600M
    // Annual:  Lite=720M, Standard=2400M, Pro=8400M, Max=19200M
    static const double monthly[]  = { 0.0, 60.0e6, 200.0e6, 700.0e6, 1600.0e6 };
    static const double annual[]   = { 0.0, 720.0e6, 2400.0e6, 8400.0e6, 19200.0e6 };
    if (plan < 0 || plan > 4) return 0.0;
    return (cycle == 0) ? monthly[plan] : annual[plan];
}

// ── Lifecycle ──────────────────────────────────────────────────

PluginMiMoBalance::PluginMiMoBalance() {}

PluginMiMoBalance& PluginMiMoBalance::Instance()
{
    return m_instance;
}

IPluginItem* PluginMiMoBalance::GetItem(int index)
{
    if (index == 0) return &m_balance_item;
    return nullptr;
}

const wchar_t* PluginMiMoBalance::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:          return L"MiMo Balance";
    case TMI_DESCRIPTION:   return L"Display Xiaomi MiMo Token Plan info in TrafficMonitor";
    case TMI_AUTHOR:        return L"AI Liv";
    case TMI_COPYRIGHT:     return L"Copyright (C) 2026";
    case TMI_VERSION:       return L"1.0";
    case TMI_URL:           return L"https://github.com/Likhixang/AILiv";
    default:                return L"";
    }
}

void PluginMiMoBalance::OnInitialize(ITrafficMonitor* pApp)
{
    m_app = pApp;
}

void PluginMiMoBalance::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    if (index == EI_CONFIG_DIR)
    {
        m_config_dir = data;
        LoadConfig();
    }
}

// ── Config Persistence ─────────────────────────────────────────

void PluginMiMoBalance::LoadConfig()
{
    if (m_config_dir.empty()) return;

    std::wstring path = m_config_dir + L"\\MiMoBalance.ini";
    WCHAR buff[4096] = {};
    GetPrivateProfileStringW(L"Settings", L"ApiKey", L"", buff, 4096, path.c_str());
    m_api_key = buff;

    m_region = GetPrivateProfileIntW(L"Settings", L"Region", 0, path.c_str());
    m_plan = GetPrivateProfileIntW(L"Settings", L"Plan", 0, path.c_str());
    m_plan_cycle = GetPrivateProfileIntW(L"Settings", L"PlanCycle", 0, path.c_str());

    DWORD budget_dword = GetPrivateProfileIntW(L"Settings", L"MonthlyBudget", 0, path.c_str());
    m_monthly_budget = static_cast<double>(budget_dword);

    DWORD interval = GetPrivateProfileIntW(L"Settings", L"FetchInterval", 30, path.c_str());
    m_fetch_interval_ms = static_cast<ULONGLONG>(interval) * 60000;
}

void PluginMiMoBalance::SaveConfig()
{
    if (m_config_dir.empty()) return;

    std::wstring path = m_config_dir + L"\\MiMoBalance.ini";
    WritePrivateProfileStringW(L"Settings", L"ApiKey", m_api_key.c_str(), path.c_str());

    WCHAR buff[64] = {};
    swprintf_s(buff, L"%d", m_region);
    WritePrivateProfileStringW(L"Settings", L"Region", buff, path.c_str());

    swprintf_s(buff, L"%d", m_plan);
    WritePrivateProfileStringW(L"Settings", L"Plan", buff, path.c_str());

    swprintf_s(buff, L"%d", m_plan_cycle);
    WritePrivateProfileStringW(L"Settings", L"PlanCycle", buff, path.c_str());

    swprintf_s(buff, L"%.0f", m_monthly_budget);
    WritePrivateProfileStringW(L"Settings", L"MonthlyBudget", buff, path.c_str());

    swprintf_s(buff, L"%llu", m_fetch_interval_ms / 60000);
    WritePrivateProfileStringW(L"Settings", L"FetchInterval", buff, path.c_str());
}

// ── HTTP ───────────────────────────────────────────────────────

bool PluginMiMoBalance::HttpGet(const std::wstring& url, const std::wstring& api_key_header,
                                 bool use_api_key_header, std::string& out)
{
    // Parse URL to get host and path
    // URL format: https://host/path
    std::wstring host, path;
    size_t slash_pos = url.find(L"://");
    if (slash_pos == std::wstring::npos) return false;
    size_t host_start = slash_pos + 3;
    size_t path_start = url.find(L'/', host_start);
    if (path_start == std::wstring::npos) return false;
    host = url.substr(host_start, path_start - host_start);
    path = url.substr(path_start);

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

    if (!api_key_header.empty())
    {
        if (use_api_key_header)
        {
            std::wstring hdr = L"api-key: " + api_key_header;
            WinHttpAddRequestHeaders(hRequest, hdr.c_str(),
                                     (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
        }
        else
        {
            std::wstring hdr = L"Authorization: Bearer " + api_key_header;
            WinHttpAddRequestHeaders(hRequest, hdr.c_str(),
                                     (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
        }
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

// ── Balance query (best-effort, no documented API) ───────────

bool PluginMiMoBalance::TryQueryBalance(std::wstring& out_display)
{
    // MiMo has NO documented balance/usage API.
    // This method tries a few common undocumented patterns
    // as a best-effort. If none works, it falls back to manual plan info.
    //
    // Attempted endpoints:
    // 1. /v1/user/balance (common pattern)
    // 2. /api/v1/user/balance (common pattern)
    //
    // MiMo API keys start with sk- (PAYG) or tp- (Token Plan).
    // Auth header: "api-key: <key>" or "Authorization: Bearer <key>"

    if (m_api_key.empty() && m_plan == 0)
    {
        out_display = L"No config";
        return true; // Not a real error, just no config
    }

    // Try to query an endpoint
    const wchar_t* hosts[] = {
        L"https://api.xiaomimimo.com",
        L"https://token-plan-cn.xiaomimimo.com",
        L"https://token-plan-sgp.xiaomimimo.com",
        L"https://token-plan-ams.xiaomimimo.com"
    };

    bool use_api_key_header = (m_region != 0); // Token Plan endpoints use api-key header
    // Actually, both auth methods work. Let's try Bearer first, then api-key.

    const char* endpoints[] = { "/v1/user/balance", "/api/v1/user/balance" };
    bool found = false;
    std::string resp;

    for (int e = 0; e < 2 && !found; e++)
    {
        std::wstring url = std::wstring(hosts[m_region]) + L"/v1/user/balance";
        // Convert endpoint to wstring
        int len = MultiByteToWideChar(CP_UTF8, 0, endpoints[e], -1, NULL, 0);
        std::wstring ep_ws(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, endpoints[e], -1, &ep_ws[0], len);
        if (!ep_ws.empty() && ep_ws.back() == L'\0') ep_ws.pop_back();
        url = hosts[m_region] + ep_ws;

        if (HttpGet(url, m_api_key, false, resp))
        {
            found = true;
            break;
        }
        if (HttpGet(url, m_api_key, true, resp))
        {
            found = true;
            break;
        }
    }

    if (found)
    {
        // Check if the response looks like a balance response
        if (resp.find("balance") != std::string::npos ||
            resp.find("credits") != std::string::npos ||
            resp.find("credit") != std::string::npos)
        {
            // Try to extract balance
            std::string search_key = "\"balance\"";
            size_t pos = resp.find(search_key);
            if (pos != std::string::npos)
            {
                // Extract numeric value
                pos = resp.find(':', pos + search_key.length());
                if (pos != std::string::npos)
                {
                    while (pos < resp.size() && (resp[pos] == ':' || resp[pos] == ' '))
                        pos++;
                    size_t end = pos;
                    while (end < resp.size() && resp[end] != ',' && resp[end] != '}' &&
                           resp[end] != ']' && resp[end] != ' ')
                        end++;
                    std::string num_str = resp.substr(pos, end - pos);
                    char* endptr = nullptr;
                    double balance = strtod(num_str.c_str(), &endptr);
                    if (endptr != num_str.c_str())
                    {
                        wchar_t buf[128];
                        if (balance >= 1.0e6)
                            swprintf_s(buf, L"$%.2f", balance / 100.0);
                        else
                            swprintf_s(buf, L"$%.2f", balance);
                        out_display = buf;
                        return true;
                    }
                }
            }
        }
        // Got a response but couldn't parse balance - show raw first bytes
        // Fall through to plan info
    }

    // Fallback: show plan info
    if (m_plan > 0)
    {
        static const wchar_t* plan_names[] = { L"None", L"Lite", L"Std", L"Pro", L"Max" };
        double credits = GetPlanCredits(m_plan, m_plan_cycle);
        wchar_t buf[128] = {};
        if (credits >= 1.0e6)
        {
            if (m_monthly_budget > 0.0)
                swprintf_s(buf, L"%s %.0fM | $%.0f", plan_names[m_plan], credits / 1.0e6, m_monthly_budget);
            else
                swprintf_s(buf, L"%s %.0fM", plan_names[m_plan], credits / 1.0e6);
        }
        else
        {
            if (m_monthly_budget > 0.0)
                swprintf_s(buf, L"%s | $%.0f", plan_names[m_plan], m_monthly_budget);
            else
                swprintf_s(buf, L"%s", plan_names[m_plan]);
        }
        out_display = buf;
        return true;
    }

    return false;
}

// ── Data Refresh ──────────────────────────────────────────────

void PluginMiMoBalance::DataRequired()
{
    ULONGLONG now = GetTickCount64();
    if (now - m_last_fetch_time < m_fetch_interval_ms) return;
    m_last_fetch_time = now;

    std::wstring display;
    if (TryQueryBalance(display))
    {
        m_balance_item.SetDisplayText(display);
    }
    else
    {
        m_balance_item.SetDisplayText(L"No config");
    }
}

// ── Settings Dialog ───────────────────────────────────────────

static INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static PluginMiMoBalance* pPlugin = nullptr;

    switch (msg)
    {
    case WM_INITDIALOG:
        pPlugin = reinterpret_cast<PluginMiMoBalance*>(lParam);
        if (pPlugin)
        {
            SetDlgItemTextW(hDlg, IDC_API_KEY_EDIT, pPlugin->GetApiKey().c_str());

            // Region combo
            HWND hCombo = GetDlgItem(hDlg, IDC_REGION_COMBO);
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Pay-as-you-go (api.xiaomimimo.com)");
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Token Plan China");
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Token Plan Singapore");
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Token Plan Europe");
            SendMessageW(hCombo, CB_SETCURSEL, pPlugin->GetRegion(), 0);

            // Plan tier combo
            HWND hPlan = GetDlgItem(hDlg, IDC_PLAN_COMBO);
            SendMessageW(hPlan, CB_ADDSTRING, 0, (LPARAM)L"None / Pay-as-you-go");
            SendMessageW(hPlan, CB_ADDSTRING, 0, (LPARAM)L"Lite (60M/mo)");
            SendMessageW(hPlan, CB_ADDSTRING, 0, (LPARAM)L"Standard (200M/mo)");
            SendMessageW(hPlan, CB_ADDSTRING, 0, (LPARAM)L"Pro (700M/mo)");
            SendMessageW(hPlan, CB_ADDSTRING, 0, (LPARAM)L"Max (1600M/mo)");
            SendMessageW(hPlan, CB_SETCURSEL, pPlugin->GetPlan(), 0);

            // Plan cycle combo
            HWND hCycle = GetDlgItem(hDlg, IDC_PLAN_CYCLE_COMBO);
            SendMessageW(hCycle, CB_ADDSTRING, 0, (LPARAM)L"Monthly");
            SendMessageW(hCycle, CB_ADDSTRING, 0, (LPARAM)L"Annual");
            SendMessageW(hCycle, CB_SETCURSEL, pPlugin->GetPlanCycle(), 0);

            WCHAR buff[64] = {};
            swprintf_s(buff, L"%.0f", pPlugin->GetMonthlyBudget());
            SetDlgItemTextW(hDlg, IDC_BUDGET_EDIT, buff);

            swprintf_s(buff, L"%llu", pPlugin->GetFetchIntervalMs() / 60000);
            SetDlgItemTextW(hDlg, IDC_FETCH_INTERVAL_EDIT, buff);

            // Status
            std::wstring status = L"Status: ";
            if (!pPlugin->GetApiKey().empty())
                status += L"API Key set";
            else
                status += L"No API Key";
            if (pPlugin->GetPlan() > 0)
            {
                status += L", Plan: ";
                static const wchar_t* names[] = { L"None", L"Lite", L"Standard", L"Pro", L"Max" };
                status += names[pPlugin->GetPlan()];
            }
            SetDlgItemTextW(hDlg, IDC_AUTH_STATUS, status.c_str());
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

                HWND hPlan = GetDlgItem(hDlg, IDC_PLAN_COMBO);
                sel = (int)SendMessageW(hPlan, CB_GETCURSEL, 0, 0);
                if (sel < 0) sel = 0;
                pPlugin->SetPlan(sel);

                HWND hCycle = GetDlgItem(hDlg, IDC_PLAN_CYCLE_COMBO);
                sel = (int)SendMessageW(hCycle, CB_GETCURSEL, 0, 0);
                if (sel < 0) sel = 0;
                pPlugin->SetPlanCycle(sel);

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

ITMPlugin::OptionReturn PluginMiMoBalance::ShowOptionsDialog(void* hParent)
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
    return &PluginMiMoBalance::Instance();
}
