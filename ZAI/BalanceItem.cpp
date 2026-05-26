#include "BalanceItem.h"
#include <windows.h>
#include <shellapi.h>

ZaiItem::ZaiItem()
{
}

const wchar_t* ZaiItem::GetItemName() const
{
    return L"Z.AI Balance";
}

const wchar_t* ZaiItem::GetItemId() const
{
    return L"ZAIBalance";
}

const wchar_t* ZaiItem::GetItemLableText() const
{
    return L"ZAI";
}

const wchar_t* ZaiItem::GetItemValueText() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_display_text.c_str();
}

const wchar_t* ZaiItem::GetItemValueSampleText() const
{
    return L"45%";
}

int ZaiItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag)
{
    if (type == MT_LCLICKED)
    {
        ShellExecuteW(nullptr, L"open",
                      L"https://z.ai/subscribe",
                      nullptr, nullptr, SW_SHOW);
        return 1;
    }
    return 0;
}

void ZaiItem::SetDisplayText(const std::wstring& text)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_display_text = text;
}
