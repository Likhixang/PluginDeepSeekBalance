#pragma once
#include "../PluginInterface.h"
#include "BalanceItem.h"
#include <string>
#include <vector>
#include <windows.h>
#include <bcrypt.h>
#include <ncrypt.h>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ncrypt.lib")

class PluginGeminiBalance : public ITMPlugin
{
public:
    PluginGeminiBalance();
    virtual ~PluginGeminiBalance() = default;

    static PluginGeminiBalance& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    virtual void OnInitialize(ITrafficMonitor* pApp) override;

    const std::wstring& GetSaJson() const { return m_sa_json; }
    void SetSaJson(const std::wstring& json) { m_sa_json = json; }
    double GetMonthlyBudget() const { return m_monthly_budget; }
    void SetMonthlyBudget(double budget) { m_monthly_budget = budget; }
    ULONGLONG GetFetchIntervalMs() const { return m_fetch_interval_ms; }
    void SetFetchIntervalMs(ULONGLONG ms) { m_fetch_interval_ms = ms; }
    void ForceRefresh() { m_last_fetch_time = 0; }
    void SaveConfig();
    void SetModuleHandle(HMODULE hMod) { m_hModule = hMod; }

private:
    void LoadConfig();

    // Authenticate and query Cloud Monitoring
    bool FetchUsageCount(double& out_count);

    // JWT + OAuth helpers
    static bool GetOAuthToken(const std::string& sa_json_str,
                              std::string& out_token,
                              std::string& out_project_id);
    static std::string ExtractJsonStr(const std::string& json,
                                      const std::string& key);
    static std::string Base64UrlEncode(const std::vector<unsigned char>& data);
    static bool Base64Decode(const std::string& b64, std::vector<unsigned char>& out);
    static bool SignSHA256RSA(const std::vector<unsigned char>& der_key,
                              const std::vector<unsigned char>& data,
                              std::vector<unsigned char>& out_sig);

    // HTTP
    static bool HttpPost(const std::string& url,
                         const std::string& content_type,
                         const std::string& body,
                         const std::string& bearer,
                         std::string& out_response);
    static bool HttpGet(const std::string& url,
                        const std::string& bearer,
                        std::string& out_response);

    GeminiItem m_balance_item;
    ITrafficMonitor* m_app{};
    HMODULE m_hModule{};

    std::wstring m_sa_json;
    double m_monthly_budget{};

    std::wstring m_config_dir;
    ULONGLONG m_last_fetch_time{};
    ULONGLONG m_fetch_interval_ms{1800000};

    static PluginGeminiBalance m_instance;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
#ifdef __cplusplus
}
#endif
