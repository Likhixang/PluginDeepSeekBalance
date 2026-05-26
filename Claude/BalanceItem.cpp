#include "BalanceItem.h"
#include <windows.h>
#include <shellapi.h>

ClaudeItem::ClaudeItem()
{
}

const wchar_t* ClaudeItem::GetItemName() const
{
    return L"Claude Balance";
}

const wchar_t* ClaudeItem::GetItemId() const
{
    return L"ClaudeBalance";
}

const wchar_t* ClaudeItem::GetItemLableText() const
{
    return L"Claude";
}

const wchar_t* ClaudeItem::GetItemValueText() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_display_text.c_str();
}

const wchar_t* ClaudeItem::GetItemValueSampleText() const
{
    return L"$47.21";
}

int ClaudeItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag)
{
    if (type == MT_LCLICKED)
    {
        ShellExecuteW(nullptr, L"open", L"https://platform.claude.com/usage",
                      nullptr, nullptr, SW_SHOW);
        return 1;
    }
    return 0;
}

void ClaudeItem::SetDisplayText(const std::wstring& text)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_display_text = text;
}
