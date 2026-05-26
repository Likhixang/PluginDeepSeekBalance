#include "BalanceItem.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

BalanceItem::BalanceItem()
    : m_display_text(L"--")
{
}

const wchar_t* BalanceItem::GetItemName() const
{
    return L"DeepSeek Balance";
}

const wchar_t* BalanceItem::GetItemId() const
{
    return L"D33pSeekBaL4nc3";
}

const wchar_t* BalanceItem::GetItemLableText() const
{
    return L"DS:";
}

const wchar_t* BalanceItem::GetItemValueText() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_display_text.c_str();
}

const wchar_t* BalanceItem::GetItemValueSampleText() const
{
    return L"\u00A5100.00";
}

int BalanceItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag)
{
    if (type == MT_LCLICKED)
    {
        ShellExecuteW(nullptr, L"open", L"https://platform.deepseek.com/billing", nullptr, nullptr, SW_SHOW);
        return 1;
    }
    return 0;
}

void BalanceItem::SetDisplayText(const std::wstring& text)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_display_text = text;
}
