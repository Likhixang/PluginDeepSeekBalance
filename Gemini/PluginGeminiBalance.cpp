#define WIN32_LEAN_AND_MEAN
#include "PluginGeminiBalance.h"
#include "resource.h"
#include <windows.h>
#include <wincrypt.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

// ── Pad PKCS8 DER for CNG import ────────────────────────────
// CNG's NCRYPT_PKCS8_PRIVATE_KEY_BLOB expects raw DER bytes
// prefixed with a CRYPT_PRIVATE_KEY_INFO-compatible structure.
// For simplicity we pass raw DER and let NCryptImportKey handle it.

PluginGeminiBalance PluginGeminiBalance::m_instance;

// ── Lifecycle ──────────────────────────────────────────────────

PluginGeminiBalance::PluginGeminiBalance() {}

PluginGeminiBalance& PluginGeminiBalance::Instance()
{
    return m_instance;
}

IPluginItem* PluginGeminiBalance::GetItem(int index)
{
    if (index == 0) return &m_balance_item;
    return nullptr;
}

const wchar_t* PluginGeminiBalance::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:          return L"Gemini Balance";
    case TMI_DESCRIPTION:   return L"Display Google Gemini API usage in TrafficMonitor";
    case TMI_AUTHOR:        return L"AI Liv";
    case TMI_COPYRIGHT:     return L"Copyright (C) 2026";
    case TMI_VERSION:       return L"1.0";
    case TMI_URL:           return L"https://github.com/Likhixang/AILiv";
    default:                return L"";
    }
}

void PluginGeminiBalance::OnInitialize(ITrafficMonitor* pApp) { m_app = pApp; }

void PluginGeminiBalance::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    if (index == EI_CONFIG_DIR) { m_config_dir = data; LoadConfig(); }
}

// ── Config ─────────────────────────────────────────────────────

void PluginGeminiBalance::LoadConfig()
{
    if (m_config_dir.empty()) return;
    std::wstring path = m_config_dir + L"\\GeminiBalance.ini";
    WCHAR buff[8192] = {};
    GetPrivateProfileStringW(L"Settings", L"SaJson", L"", buff, 8192, path.c_str());
    m_sa_json = buff;
    DWORD b = GetPrivateProfileIntW(L"Settings", L"MonthlyBudget", 0, path.c_str());
    m_monthly_budget = static_cast<double>(b);
    DWORD interval = GetPrivateProfileIntW(L"Settings", L"FetchInterval", 30, path.c_str());
    m_fetch_interval_ms = static_cast<ULONGLONG>(interval) * 60000;
}

void PluginGeminiBalance::SaveConfig()
{
    if (m_config_dir.empty()) return;
    std::wstring path = m_config_dir + L"\\GeminiBalance.ini";
    WritePrivateProfileStringW(L"Settings", L"SaJson", m_sa_json.c_str(), path.c_str());
    WCHAR b[64] = {};
    swprintf_s(b, L"%.0f", m_monthly_budget);
    WritePrivateProfileStringW(L"Settings", L"MonthlyBudget", b, path.c_str());
    swprintf_s(b, L"%llu", m_fetch_interval_ms / 60000);
    WritePrivateProfileStringW(L"Settings", L"FetchInterval", b, path.c_str());
}

// ── HTTP (string-based, for text APIs) ────────────────────────

static bool WinHttpGetString(const std::wstring& host, const std::wstring& path,
                              bool https, const std::wstring& bearer,
                              std::string& out)
{
    HINTERNET hSession = WinHttpOpen(L"AILiv/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     NULL, NULL, 0);
    if (!hSession) return false;
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
                                        https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                            NULL, NULL, NULL, https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    WinHttpSetTimeouts(hRequest, 10000, 10000, 10000, 30000);
    if (!bearer.empty())
    {
        std::wstring ah = L"Authorization: Bearer " + std::wstring(bearer.begin(), bearer.end());
        WinHttpAddRequestHeaders(hRequest, ah.c_str(), (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
    }

    bool ok = false;
    if (WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(hRequest, NULL))
    {
        char tmp[8192]; DWORD read;
        while (WinHttpReadData(hRequest, tmp, sizeof(tmp), &read) && read > 0)
            out.append(tmp, read);
        ok = !out.empty();
    }
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    return ok;
}

static bool WinHttpPostString(const std::wstring& host, const std::wstring& path,
                               bool https, const std::string& body,
                               const std::string& content_type,
                               std::string& out)
{
    HINTERNET hSession = WinHttpOpen(L"AILiv/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     NULL, NULL, 0);
    if (!hSession) return false;
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
                                        https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
                                            NULL, NULL, NULL, https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    WinHttpSetTimeouts(hRequest, 10000, 10000, 10000, 30000);
    if (!content_type.empty())
    {
        std::wstring ct(content_type.begin(), content_type.end());
        std::wstring hdr = L"Content-Type: " + ct;
        WinHttpAddRequestHeaders(hRequest, hdr.c_str(), (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
    }

    // Send the body
    bool ok = false;
    if (WinHttpSendRequest(hRequest, NULL, 0, (LPVOID)body.data(), (DWORD)body.size(),
                           (DWORD)body.size(), 0) &&
        WinHttpReceiveResponse(hRequest, NULL))
    {
        char tmp[8192]; DWORD read;
        while (WinHttpReadData(hRequest, tmp, sizeof(tmp), &read) && read > 0)
            out.append(tmp, read);
        ok = !out.empty();
    }
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    return ok;
}

// ── JSON Extract ──────────────────────────────────────────────

std::string PluginGeminiBalance::ExtractJsonStr(const std::string& json, const std::string& key)
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

// ── Base64 ────────────────────────────────────────────────────

bool PluginGeminiBalance::Base64Decode(const std::string& b64, std::vector<unsigned char>& out)
{
    DWORD len = 0;
    if (!CryptStringToBinaryA(b64.c_str(), (DWORD)b64.size(), CRYPT_STRING_BASE64,
                               NULL, &len, NULL, NULL))
        return false;
    out.resize(len);
    return !!CryptStringToBinaryA(b64.c_str(), (DWORD)b64.size(), CRYPT_STRING_BASE64,
                                   out.data(), &len, NULL, NULL);
}

std::string PluginGeminiBalance::Base64UrlEncode(const std::vector<unsigned char>& data)
{
    DWORD len = 0;
    CryptBinaryToStringA(data.data(), (DWORD)data.size(),
                         CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                         NULL, &len);
    std::string result(len, '\0');
    CryptBinaryToStringA(data.data(), (DWORD)data.size(),
                         CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                         &result[0], &len);
    // Base64 → Base64url
    for (auto& c : result) { if (c == '+') c = '-'; else if (c == '/') c = '_'; }
    // Strip padding '='
    while (!result.empty() && result.back() == '=') result.pop_back();
    return result;
}

// ── RSA-SHA256 Signing ────────────────────────────────────────

bool PluginGeminiBalance::SignSHA256RSA(const std::vector<unsigned char>& der_key,
                                         const std::vector<unsigned char>& data,
                                         std::vector<unsigned char>& out_sig)
{
    NCRYPT_PROV_HANDLE hProv = 0;
    NCRYPT_KEY_HANDLE hKey = 0;
    bool ok = false;

    do {
        if (NCryptOpenStorageProvider(&hProv, MS_KEY_STORAGE_PROVIDER, 0) != ERROR_SUCCESS)
            break;

        // Import the PKCS8 private key
        if (NCryptImportKey(hProv, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB,
                            NULL, &hKey, (PBYTE)der_key.data(), (DWORD)der_key.size(),
                            NCRYPT_SILENT_FLAG) != ERROR_SUCCESS)
            break;

        // Compute SHA256 hash
        BCRYPT_ALG_HANDLE hHashAlg = NULL;
        if (BCryptOpenAlgorithmProvider(&hHashAlg, BCRYPT_SHA256_ALGORITHM,
                                        NULL, 0) != 0)
            break;

        std::vector<unsigned char> hash(32);
        if (BCryptHash(hHashAlg, NULL, 0, (PUCHAR)data.data(), (ULONG)data.size(),
                        hash.data(), (ULONG)hash.size()) != 0)
        {
            BCryptCloseAlgorithmProvider(hHashAlg, 0);
            break;
        }
        BCryptCloseAlgorithmProvider(hHashAlg, 0);

        // Sign the hash with RSA PKCS1 v1.5 (the BCrypt padding info)
        BCRYPT_PKCS1_PADDING_INFO pad_info = { BCRYPT_SHA256_ALGORITHM };

        DWORD sig_len = 0;
        NCryptSignHash(hKey, &pad_info, hash.data(), (ULONG)hash.size(),
                       NULL, 0, &sig_len, NCRYPT_SILENT_FLAG);
        if (sig_len == 0) break;

        out_sig.resize(sig_len);
        if (NCryptSignHash(hKey, &pad_info, hash.data(), (ULONG)hash.size(),
                           out_sig.data(), (ULONG)out_sig.size(), &sig_len,
                           NCRYPT_SILENT_FLAG) == ERROR_SUCCESS)
            ok = true;

    } while (false);

    if (hKey) NCryptFreeObject(hKey);
    if (hProv) NCryptFreeObject(hProv);
    return ok;
}

// ── JWT → OAuth2 Token Exchange ──────────────────────────────

bool PluginGeminiBalance::GetOAuthToken(const std::string& sa_json_str,
                                         std::string& out_token,
                                         std::string& out_project_id)
{
    std::string client_email = ExtractJsonStr(sa_json_str, "client_email");
    std::string private_key_pem = ExtractJsonStr(sa_json_str, "private_key");
    std::string project_id = ExtractJsonStr(sa_json_str, "project_id");
    std::string private_key_id = ExtractJsonStr(sa_json_str, "private_key_id");

    if (client_email.empty() || private_key_pem.empty() || project_id.empty())
        return false;

    out_project_id = project_id;

    // Decode PEM → DER
    // Strip -----BEGIN/END lines
    size_t b = private_key_pem.find("-----BEGIN");
    size_t e = private_key_pem.rfind("-----END");
    if (b == std::string::npos || e == std::string::npos) return false;
    std::string b64 = private_key_pem.substr(b + private_key_pem.find('\n', b) + 1,
                                              e - b - private_key_pem.find('\n', b) - 1);
    // Remove all whitespace from the base64
    std::string clean;
    for (char c : b64) if (!isspace(c)) clean += c;

    std::vector<unsigned char> der_key;
    if (!Base64Decode(clean, der_key)) return false;

    // Build JWT
    // Header: {"alg":"RS256","typ":"JWT"}
    std::string header = "{\"alg\":\"RS256\",\"typ\":\"JWT\"}";
    std::string header_b64 = Base64UrlEncode(
        std::vector<unsigned char>(header.begin(), header.end()));

    // Payload
    ULONGLONG now_sec = 0;
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER ui;
    ui.LowPart = ft.dwLowDateTime;
    ui.HighPart = ft.dwHighDateTime;
    now_sec = (ui.QuadPart - 116444736000000000ULL) / 10000000;

    char payload_buf[512];
    snprintf(payload_buf, sizeof(payload_buf),
        "{\"iss\":\"%s\",\"scope\":\"https://www.googleapis.com/auth/cloud-platform\","
        "\"aud\":\"https://oauth2.googleapis.com/token\","
        "\"exp\":%llu,\"iat\":%llu}",
        client_email.c_str(), now_sec + 3600, now_sec);

    std::string payload = payload_buf;
    std::string payload_b64 = Base64UrlEncode(
        std::vector<unsigned char>(payload.begin(), payload.end()));

    // Sign
    std::string message = header_b64 + "." + payload_b64;
    std::vector<unsigned char> sig;
    if (!SignSHA256RSA(der_key,
                        std::vector<unsigned char>(message.begin(), message.end()),
                        sig))
        return false;

    std::string jwt = message + "." + Base64UrlEncode(sig);

    // Exchange JWT for access token
    std::string body = "grant_type=urn:ietf:params:oauth:grant-type:jwt-bearer&assertion=";
    // URL-encode the JWT (JWT is already safe except for some chars)
    // Actually JWT is alpha-numeric with dots, should be safe for form data
    body += jwt;

    std::string resp;
    if (!WinHttpPostString(L"oauth2.googleapis.com", L"/token", true,
                            body, "application/x-www-form-urlencoded", resp))
        return false;

    // Extract access_token from response
    std::string token = ExtractJsonStr(resp, "access_token");
    if (token.empty()) return false;

    out_token = token;
    return true;
}

// ── Cloud Monitoring Query ───────────────────────────────────

bool PluginGeminiBalance::FetchUsageCount(double& out_count)
{
    if (m_sa_json.empty()) return false;

    // Convert wstring to UTF-8 string for JSON parsing
    int len = WideCharToMultiByte(CP_UTF8, 0, m_sa_json.c_str(), -1, NULL, 0, NULL, NULL);
    std::string sa_utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, m_sa_json.c_str(), -1, &sa_utf8[0], len, NULL, NULL);
    // Remove null terminator that WideCharToMultiByte adds
    if (!sa_utf8.empty() && sa_utf8.back() == '\0') sa_utf8.pop_back();

    std::string token, project_id;
    if (!GetOAuthToken(sa_utf8, token, project_id))
        return false;

    // Build time range: month-to-date
    SYSTEMTIME st;
    GetSystemTime(&st);
    char start_buf[32], end_buf[32];
    snprintf(start_buf, sizeof(start_buf), "%04d-%02d-01T00:00:00Z", st.wYear, st.wMonth);
    snprintf(end_buf, sizeof(end_buf), "%04d-%02d-%02dT23:59:59Z", st.wYear, st.wMonth, st.wDay);

    char url[1024];
    snprintf(url, sizeof(url),
        "https://monitoring.googleapis.com/v3/projects/%s/timeSeries"
        "?filter=metric.type%%3D%%22generativeai.googleapis.com%%2Frequest_count%%22"
        "&interval.startTime=%s&interval.endTime=%s"
        "&aggregation.alignmentPeriod=86400s&aggregation.perSeriesAligner=ALIGN_SUM",
        project_id.c_str(), start_buf, end_buf);

    std::string resp;
    if (!WinHttpGetString(L"monitoring.googleapis.com",
                           std::wstring(url + 8, url + 8 + strlen(url) - 8), true,
                           std::wstring(token.begin(), token.end()), resp))
        return false;

    // Parse: find int64Value in the response
    // The response has "points":[{"interval":...,"value":{"int64Value":"12345"}}]
    std::string search = "\"int64Value\":";
    size_t pos = resp.find(search);
    if (pos == std::string::npos) {
        // Try doubleValue
        search = "\"doubleValue\":";
        pos = resp.find(search);
        if (pos == std::string::npos) return false;
    }

    pos += search.length();
    // Skip whitespace and quotes
    while (pos < resp.size() && (resp[pos] == ' ' || resp[pos] == '"')) pos++;
    size_t end = pos;
    while (end < resp.size() && resp[end] != '"' && resp[end] != ',' &&
           resp[end] != '}' && resp[end] != ']') end++;

    std::string num_str = resp.substr(pos, end - pos);
    char* endptr = nullptr;
    double val = strtod(num_str.c_str(), &endptr);
    if (endptr == num_str.c_str()) return false;

    out_count = val;
    return true;
}

// ── Data Refresh ──────────────────────────────────────────────

void PluginGeminiBalance::DataRequired()
{
    ULONGLONG now = GetTickCount64();
    if (now - m_last_fetch_time < m_fetch_interval_ms) return;
    m_last_fetch_time = now;

    if (m_sa_json.empty())
    {
        m_balance_item.SetDisplayText(L"No Key");
        return;
    }

    double count = 0;
    if (FetchUsageCount(count))
    {
        std::wstring display;
        wchar_t buf[128] = {};

        // Format the count
        if (count >= 1000000.0)
            swprintf_s(buf, L"%.1fM req", count / 1000000.0);
        else if (count >= 1000.0)
            swprintf_s(buf, L"%.1fK req", count / 1000.0);
        else
            swprintf_s(buf, L"%.0f req", count);

        display = buf;

        if (m_monthly_budget > 0.0)
        {
            // Try to attach budget info — but since we have request counts not costs,
            // show it separately
            display += L" | $" + std::to_wstring((int)m_monthly_budget) + L" cap";
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
    static PluginGeminiBalance* pPlugin = nullptr;

    switch (msg)
    {
    case WM_INITDIALOG:
        pPlugin = reinterpret_cast<PluginGeminiBalance*>(lParam);
        if (pPlugin)
        {
            SetDlgItemTextW(hDlg, IDC_SA_JSON_EDIT, pPlugin->GetSaJson().c_str());
            WCHAR b[64] = {};
            swprintf_s(b, L"%.0f", pPlugin->GetMonthlyBudget());
            SetDlgItemTextW(hDlg, IDC_BUDGET_EDIT, b);
            swprintf_s(b, L"%llu", pPlugin->GetFetchIntervalMs() / 60000);
            SetDlgItemTextW(hDlg, IDC_FETCH_INTERVAL_EDIT, b);
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
                WCHAR b[8192] = {};
                GetDlgItemTextW(hDlg, IDC_SA_JSON_EDIT, b, 8192);
                pPlugin->SetSaJson(b);
                GetDlgItemTextW(hDlg, IDC_BUDGET_EDIT, b, 64);
                double budget = _wtof(b);
                if (budget < 0) budget = 0;
                pPlugin->SetMonthlyBudget(budget);
                GetDlgItemTextW(hDlg, IDC_FETCH_INTERVAL_EDIT, b, 32);
                ULONGLONG interval_min = _wtoi64(b);
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

ITMPlugin::OptionReturn PluginGeminiBalance::ShowOptionsDialog(void* hParent)
{
    INT_PTR ret = DialogBoxParamW(m_hModule,
                                  MAKEINTRESOURCEW(IDD_SETTINGS_DIALOG),
                                  static_cast<HWND>(hParent),
                                  SettingsDlgProc,
                                  reinterpret_cast<LPARAM>(this));
    return (ret == IDOK) ? OR_OPTION_CHANGED : OR_OPTION_UNCHANGED;
}

// ── Entry Point ───────────────────────────────────────────────

ITMPlugin* TMPluginGetInstance()
{
    return &PluginGeminiBalance::Instance();
}
