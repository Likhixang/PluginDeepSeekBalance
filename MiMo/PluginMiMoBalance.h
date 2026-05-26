#pragma once
#include "../PluginInterface.h"
#include "BalanceItem.h"
#include <string>
#include <vector>
#include <windows.h>

class PluginMiMoBalance : public ITMPlugin
{
public:
    PluginMiMoBalance();
    virtual ~PluginMiMoBalance() = default;

    static PluginMiMoBalance& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    virtual void OnInitialize(ITrafficMonitor* pApp) override;

    const std::wstring& GetApiKey() const { return m_api_key; }
    void SetApiKey(const std::wstring& key) { m_api_key = key; }
    int GetRegion() const { return m_region; }
    void SetRegion(int region) { m_region = region; }
    int GetPlan() const { return m_plan; }  // 0=None/PAYG, 1=Lite, 2=Standard, 3=Pro, 4=Max
    void SetPlan(int plan) { m_plan = plan; }
    int GetPlanCycle() const { return m_plan_cycle; }  // 0=Monthly, 1=Annual
    void SetPlanCycle(int cycle) { m_plan_cycle = cycle; }
    double GetMonthlyBudget() const { return m_monthly_budget; }
    void SetMonthlyBudget(double budget) { m_monthly_budget = budget; }
    ULONGLONG GetFetchIntervalMs() const { return m_fetch_interval_ms; }
    void SetFetchIntervalMs(ULONGLONG ms) { m_fetch_interval_ms = ms; }
    void ForceRefresh() { m_last_fetch_time = 0; }
    void SaveConfig();
    void SetModuleHandle(HMODULE hMod) { m_hModule = hMod; }

    // Get credit limit for selected plan
    static double GetPlanCredits(int plan, int cycle);

private:
    void LoadConfig();
    bool TryQueryBalance(std::wstring& out_display);

    // HTTP helper
    static bool HttpGet(const std::wstring& url, const std::wstring& api_key_header,
                        bool use_api_key_header, std::string& out);

    MiMoItem m_balance_item;
    ITrafficMonitor* m_app{};
    HMODULE m_hModule{};

    std::wstring m_api_key;
    int m_region{};             // 0=PAYG (api.xiaomimimo.com), 1=CN-TokenPlan, 2=SGP-TokenPlan, 3=EU-TokenPlan
    int m_plan{};               // 0=None, 1=Lite, 2=Standard, 3=Pro, 4=Max
    int m_plan_cycle{};         // 0=Monthly, 1=Annual
    double m_monthly_budget{};

    std::wstring m_config_dir;
    ULONGLONG m_last_fetch_time{};
    ULONGLONG m_fetch_interval_ms{1800000};

    static PluginMiMoBalance m_instance;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
#ifdef __cplusplus
}
#endif
