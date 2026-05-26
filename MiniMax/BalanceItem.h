#pragma once
#include "../PluginInterface.h"
#include <string>
#include <mutex>

class MiniMaxItem : public IPluginItem
{
public:
    MiniMaxItem();
    virtual ~MiniMaxItem() = default;

    virtual const wchar_t* GetItemName() const override;
    virtual const wchar_t* GetItemId() const override;
    virtual const wchar_t* GetItemLableText() const override;
    virtual const wchar_t* GetItemValueText() const override;
    virtual const wchar_t* GetItemValueSampleText() const override;
    virtual int OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) override;

    void SetDisplayText(const std::wstring& text);

private:
    std::wstring m_display_text;
    mutable std::mutex m_mutex;
};
