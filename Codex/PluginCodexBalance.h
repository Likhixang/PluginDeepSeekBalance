#pragma once
#include "../PluginInterface.h"
#include "BalanceItem.h"
#include <string>
#include <vector>
#include <windows.h>

class PluginCodexBalance : public ITMPlugin
{
public:
    PluginCodexBalance();
    virtual ~PluginCodexBalance() = default;

    static PluginCodexBalance& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    virtual void OnInitialize(ITrafficMonitor* pApp) override;

    // Config accessors
    const std::wstring& GetSessionCookie() const { return m_session_cookie; }
    void SetSessionCookie(const std::wstring& cookie) { m_session_cookie = cookie; }
    const std::wstring& GetApiKey() const { return m_api_key; }
    void SetApiKey(const std::wstring& key) { m_api_key = key; }
    double GetMonthlyBudget() const { return m_monthly_budget; }
    void SetMonthlyBudget(double budget) { m_monthly_budget = budget; }
    ULONGLONG GetFetchIntervalMs() const { return m_fetch_interval_ms; }
    void SetFetchIntervalMs(ULONGLONG ms) { m_fetch_interval_ms = ms; }
    void ForceRefresh() { m_last_fetch_time = 0; }
    void SaveConfig();
    void SetModuleHandle(HMODULE hMod) { m_hModule = hMod; }

private:
    // Config persistence
    void LoadConfig();

    // Data fetching — priority chain
    // Priority 1: session cookie → dashboard API (full billing)
    bool FetchWithSessionCookie(double& out_spent, double& out_balance);

    // Priority 2: API key → official /organization/costs (fallback)
    bool FetchWithApiKey(double& out_spent);

    // Low-level: perform an HTTP GET with optional Cookie or Bearer auth
    static bool HttpGet(const std::wstring& host,
                        const std::wstring& path,
                        bool https,
                        const std::wstring& cookie,
                        const std::wstring& bearer_token,
                        std::vector<char>& out_response);

    // Helper: compute month-to-date timestamps
    static void GetMonthDateRange(std::wstring& out_start_date,
                                  std::wstring& out_end_date);

    // Members
    CodexItem m_balance_item;
    ITrafficMonitor* m_app{};
    HMODULE m_hModule{};

    // Auth credentials
    std::wstring m_session_cookie;   // Priority 1
    std::wstring m_api_key;          // Priority 2 (fallback)

    // Display config
    double m_monthly_budget{};

    // Timing
    std::wstring m_config_dir;
    ULONGLONG m_last_fetch_time{};
    ULONGLONG m_fetch_interval_ms{1800000}; // default 30 min

    static PluginCodexBalance m_instance;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
#ifdef __cplusplus
}
#endif
