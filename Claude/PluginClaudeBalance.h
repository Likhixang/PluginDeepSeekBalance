#pragma once
#include "../PluginInterface.h"
#include "BalanceItem.h"
#include <string>
#include <vector>
#include <windows.h>

class PluginClaudeBalance : public ITMPlugin
{
public:
    PluginClaudeBalance();
    virtual ~PluginClaudeBalance() = default;

    static PluginClaudeBalance& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    virtual void OnInitialize(ITrafficMonitor* pApp) override;

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
    void LoadConfig();

    // Fetch month-to-date cost via /v1/organizations/cost_report
    bool FetchCost(double& out_cents);

    // Low-level: HTTP GET to Anthropic API
    static bool HttpGet(const std::wstring& path,
                        const std::wstring& api_key,
                        std::vector<char>& out_response);

    // Parse cost report: sum all amount fields (in cents)
    static double SumCostAmounts(const std::vector<char>& utf8_data);

    ClaudeItem m_balance_item;
    ITrafficMonitor* m_app{};
    HMODULE m_hModule{};

    std::wstring m_api_key;
    double m_monthly_budget{};

    std::wstring m_config_dir;
    ULONGLONG m_last_fetch_time{};
    ULONGLONG m_fetch_interval_ms{1800000};

    static PluginClaudeBalance m_instance;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
#ifdef __cplusplus
}
#endif
