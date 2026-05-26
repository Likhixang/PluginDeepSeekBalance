#include "BalanceItem.h"
#include <windows.h>
#include <shellapi.h>

MiMoItem::MiMoItem()
{
}

const wchar_t* MiMoItem::GetItemName() const
{
    return L"MiMo Balance";
}

const wchar_t* MiMoItem::GetItemId() const
{
    return L"MiMoBalance";
}

const wchar_t* MiMoItem::GetItemLableText() const
{
    return L"MiMo";
}

const wchar_t* MiMoItem::GetItemValueText() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_display_text.c_str();
}

const wchar_t* MiMoItem::GetItemValueSampleText() const
{
    return L"Lite 60M";
}

int MiMoItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag)
{
    if (type == MT_LCLICKED)
    {
        ShellExecuteW(nullptr, L"open",
                      L"https://platform.xiaomimimo.com/token-plan",
                      nullptr, nullptr, SW_SHOW);
        return 1;
    }
    return 0;
}

void MiMoItem::SetDisplayText(const std::wstring& text)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_display_text = text;
}
