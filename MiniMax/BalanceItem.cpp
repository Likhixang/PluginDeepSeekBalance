#include "BalanceItem.h"
#include <windows.h>
#include <shellapi.h>

MiniMaxItem::MiniMaxItem()
{
}

const wchar_t* MiniMaxItem::GetItemName() const
{
    return L"MiniMax Balance";
}

const wchar_t* MiniMaxItem::GetItemId() const
{
    return L"MiniMaxBalance";
}

const wchar_t* MiniMaxItem::GetItemLableText() const
{
    return L"MiniMax";
}

const wchar_t* MiniMaxItem::GetItemValueText() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_display_text.c_str();
}

const wchar_t* MiniMaxItem::GetItemValueSampleText() const
{
    return L"650/1500 req";
}

int MiniMaxItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag)
{
    if (type == MT_LCLICKED)
    {
        ShellExecuteW(nullptr, L"open",
                      L"https://platform.minimax.io/token-plan",
                      nullptr, nullptr, SW_SHOW);
        return 1;
    }
    return 0;
}

void MiniMaxItem::SetDisplayText(const std::wstring& text)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_display_text = text;
}
