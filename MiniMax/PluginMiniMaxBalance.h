#pragma once
#include "../PluginInterface.h"
#include "BalanceItem.h"
#include <string>
#include <vector>
#include <windows.h>

class PluginMiniMaxBalance : public ITMPlugin
{
public:
    PluginMiniMaxBalance();
    virtual ~PluginMiniMaxBalance() = default;

    static PluginMiniMaxBalance& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    virtual void OnInitialize(ITrafficMonitor* pApp) override;

    const std::wstring& GetApiKey() const { return m_api_key; }
    void SetApiKey(const std::wstring& key) { m_api_key = key; }
    int GetRegion() const { return m_region; }  // 0=Intl (minimax.io), 1=China (minimaxi.com)
    void SetRegion(int region) { m_region = region; }
    double GetMonthlyBudget() const { return m_monthly_budget; }
    void SetMonthlyBudget(double budget) { m_monthly_budget = budget; }
    ULONGLONG GetFetchIntervalMs() const { return m_fetch_interval_ms; }
    void SetFetchIntervalMs(ULONGLONG ms) { m_fetch_interval_ms = ms; }
    void ForceRefresh() { m_last_fetch_time = 0; }
    void SaveConfig();
    void SetModuleHandle(HMODULE hMod) { m_hModule = hMod; }

private:
    void LoadConfig();
    bool FetchUsage(std::wstring& out_display);

    // HTTP + JSON helpers
    static bool HttpGet(const std::wstring& host, const std::wstring& path,
                        const std::wstring& bearer, std::string& out);

    MiniMaxItem m_balance_item;
    ITrafficMonitor* m_app{};
    HMODULE m_hModule{};

    std::wstring m_api_key;
    int m_region{};                 // 0=Intl, 1=China
    double m_monthly_budget{};

    std::wstring m_config_dir;
    ULONGLONG m_last_fetch_time{};
    ULONGLONG m_fetch_interval_ms{1800000};

    static PluginMiniMaxBalance m_instance;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
#ifdef __cplusplus
}
#endif
