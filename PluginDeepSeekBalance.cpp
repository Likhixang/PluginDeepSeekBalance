#define WIN32_LEAN_AND_MEAN
#include "PluginDeepSeekBalance.h"
#include "resource.h"
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

PluginDeepSeekBalance PluginDeepSeekBalance::m_instance;

PluginDeepSeekBalance::PluginDeepSeekBalance()
{
}

PluginDeepSeekBalance& PluginDeepSeekBalance::Instance()
{
    return m_instance;
}

IPluginItem* PluginDeepSeekBalance::GetItem(int index)
{
    if (index == 0)
        return &m_balance_item;
    return nullptr;
}

static std::wstring ExtractJsonString(const std::vector<char>& utf8_data, const std::string& key)
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

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, val_utf8.c_str(), (int)val_utf8.size(), nullptr, 0);
    if (wide_len <= 0)
        return {};

    std::wstring result(wide_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, val_utf8.c_str(), (int)val_utf8.size(), &result[0], wide_len);
    return result;
}

bool PluginDeepSeekBalance::FetchBalanceFromAPI(const std::wstring& api_key,
                                                 std::wstring& out_balance,
                                                 std::wstring& out_currency)
{
    if (api_key.empty())
        return false;

    HINTERNET hSession = WinHttpOpen(L"TM-DeepSeek-Plugin/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     NULL, NULL, 0);
    if (!hSession)
        return false;

    HINTERNET hConnect = WinHttpConnect(hSession, L"api.deepseek.com",
                                        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/user/balance",
                                            NULL, NULL, NULL,
                                            WINHTTP_FLAG_SECURE);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    WinHttpSetTimeouts(hRequest, 5000, 5000, 5000, 10000);

    std::wstring auth = L"Authorization: Bearer " + api_key;
    WinHttpAddRequestHeaders(hRequest, auth.c_str(), (ULONG)-1L,
                             WINHTTP_ADDREQ_FLAG_ADD);

    bool success = false;
    if (WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL))
    {
        std::vector<char> buffer;
        char tmp[4096] = {};
        DWORD bytes_read = 0;

        while (WinHttpReadData(hRequest, tmp, sizeof(tmp) - 1, &bytes_read) && bytes_read > 0)
        {
            buffer.insert(buffer.end(), tmp, tmp + bytes_read);
        }

        if (!buffer.empty())
        {
            out_balance = ExtractJsonString(buffer, "total_balance");
            out_currency = ExtractJsonString(buffer, "currency");

            if (!out_balance.empty())
            {
                size_t dot = out_balance.find(L'.');
                if (dot != std::wstring::npos && out_balance.length() > dot + 3)
                    out_balance = out_balance.substr(0, dot + 3);
                success = true;
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return success;
}

void PluginDeepSeekBalance::DataRequired()
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

    std::wstring balance, currency;
    if (FetchBalanceFromAPI(m_api_key, balance, currency))
    {
        m_balance_item.SetDisplayText(FormatBalance(balance, currency));
    }
    else
    {
        m_balance_item.SetDisplayText(L"ERR");
    }
}

std::wstring PluginDeepSeekBalance::FormatBalance(const std::wstring& balance, const std::wstring& currency)
{
    if (currency == L"CNY")
        return L"\u00A5" + balance;
    return L"$" + balance;
}

const wchar_t* PluginDeepSeekBalance::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return L"DeepSeek Balance";
    case TMI_DESCRIPTION:
        return L"Display DeepSeek API account balance in TrafficMonitor";
    case TMI_AUTHOR:
        return L"TM Plugin Dev";
    case TMI_COPYRIGHT:
        return L"Copyright (C) 2025";
    case TMI_VERSION:
        return L"1.3";
    case TMI_URL:
        return L"https://github.com/Likhixang/PluginDeepSeekBalance";
    default:
        return L"";
    }
}

static INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static PluginDeepSeekBalance* pPlugin = nullptr;

    switch (msg)
    {
    case WM_INITDIALOG:
        pPlugin = reinterpret_cast<PluginDeepSeekBalance*>(lParam);
        if (pPlugin)
        {
            SetDlgItemTextW(hDlg, IDC_API_KEY_EDIT, pPlugin->GetApiKey().c_str());

            WCHAR buff[32] = {};
            swprintf_s(buff, L"%llu", pPlugin->GetFetchIntervalMs() / 60000);
            SetDlgItemTextW(hDlg, IDC_FETCH_INTERVAL_EDIT, buff);
        }
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            if (pPlugin)
            {
                WCHAR buff[512] = {};
                GetDlgItemTextW(hDlg, IDC_API_KEY_EDIT, buff, 512);
                pPlugin->SetApiKey(buff);

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

ITMPlugin::OptionReturn PluginDeepSeekBalance::ShowOptionsDialog(void* hParent)
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

void PluginDeepSeekBalance::LoadConfig()
{
    if (m_config_dir.empty())
        return;

    std::wstring path = m_config_dir + L"\\DeepSeekBalance.ini";
    WCHAR buff[512] = {};
    GetPrivateProfileStringW(L"Settings", L"ApiKey", L"", buff, 512, path.c_str());
    m_api_key = buff;

    DWORD interval = GetPrivateProfileIntW(L"Settings", L"FetchInterval", 30, path.c_str());
    m_fetch_interval_ms = static_cast<ULONGLONG>(interval) * 60000;
}

void PluginDeepSeekBalance::SaveConfig()
{
    if (m_config_dir.empty())
        return;

    std::wstring path = m_config_dir + L"\\DeepSeekBalance.ini";
    WritePrivateProfileStringW(L"Settings", L"ApiKey", m_api_key.c_str(), path.c_str());

    WCHAR buff[32] = {};
    swprintf_s(buff, L"%llu", m_fetch_interval_ms / 60000);
    WritePrivateProfileStringW(L"Settings", L"FetchInterval", buff, path.c_str());
}

void PluginDeepSeekBalance::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    if (index == EI_CONFIG_DIR)
    {
        m_config_dir = data;
        LoadConfig();
    }
}

void PluginDeepSeekBalance::OnInitialize(ITrafficMonitor* pApp)
{
    m_app = pApp;
}

ITMPlugin* TMPluginGetInstance()
{
    return &PluginDeepSeekBalance::Instance();
}
