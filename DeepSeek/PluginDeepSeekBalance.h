#pragma once
#include "PluginInterface.h"
#include "BalanceItem.h"
#include <string>
#include <windows.h>

class PluginDeepSeekBalance : public ITMPlugin
{
public:
    PluginDeepSeekBalance();
    virtual ~PluginDeepSeekBalance() = default;

    static PluginDeepSeekBalance& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    virtual void OnInitialize(ITrafficMonitor* pApp) override;

    const std::wstring& GetApiKey() const { return m_api_key; }
    void SetApiKey(const std::wstring& key) { m_api_key = key; }
    ULONGLONG GetFetchIntervalMs() const { return m_fetch_interval_ms; }
    void SetFetchIntervalMs(ULONGLONG ms) { m_fetch_interval_ms = ms; }
    void ForceRefresh() { m_last_fetch_time = 0; }
    void SaveConfig();

    void SetModuleHandle(HMODULE hMod) { m_hModule = hMod; }

private:
    void LoadConfig();
    static bool FetchBalanceFromAPI(const std::wstring& api_key,
                                    std::wstring& out_balance,
                                    std::wstring& out_currency);
    static std::wstring FormatBalance(const std::wstring& balance, const std::wstring& currency);

    BalanceItem m_balance_item;
    ITrafficMonitor* m_app{};
    HMODULE m_hModule{};
    std::wstring m_api_key;
    std::wstring m_config_dir;
    ULONGLONG m_last_fetch_time{};
    ULONGLONG m_fetch_interval_ms{1800000};

    static PluginDeepSeekBalance m_instance;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
#ifdef __cplusplus
}
#endif
