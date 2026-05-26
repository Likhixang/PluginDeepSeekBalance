#pragma once
#include "../PluginInterface.h"
#include "BalanceItem.h"
#include <string>
#include <vector>
#include <windows.h>

class PluginZAIBalance : public ITMPlugin
{
public:
    PluginZAIBalance();
    virtual ~PluginZAIBalance() = default;

    static PluginZAIBalance& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    virtual void OnInitialize(ITrafficMonitor* pApp) override;

    const std::wstring& GetApiKey() const { return m_api_key; }
    void SetApiKey(const std::wstring& key) { m_api_key = key; }
    int GetRegion() const { return m_region; }  // 0=Intl, 1=China
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
    bool FetchQuota(std::wstring& out_display, int& out_pct);

    // Raw JSON extraction helpers
    static std::string ExtractJsonStr(const std::string& json, const std::string& key);
    static double ExtractJsonNum(const std::string& json, const std::string& key);
    static int ExtractJsonInt(const std::string& json, const std::string& key);

    ZaiItem m_balance_item;
    ITrafficMonitor* m_app{};
    HMODULE m_hModule{};

    std::wstring m_api_key;
    int m_region{};                 // 0=Intl (api.z.ai), 1=China (open.bigmodel.cn)
    double m_monthly_budget{};

    std::wstring m_config_dir;
    ULONGLONG m_last_fetch_time{};
    ULONGLONG m_fetch_interval_ms{1800000};

    static PluginZAIBalance m_instance;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
#ifdef __cplusplus
}
#endif
