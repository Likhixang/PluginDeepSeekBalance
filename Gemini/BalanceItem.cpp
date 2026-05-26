#include "BalanceItem.h"
#include <windows.h>
#include <shellapi.h>

GeminiItem::GeminiItem()
{
}

const wchar_t* GeminiItem::GetItemName() const
{
    return L"Gemini Balance";
}

const wchar_t* GeminiItem::GetItemId() const
{
    return L"GeminiBalance";
}

const wchar_t* GeminiItem::GetItemLableText() const
{
    return L"Gemini";
}

const wchar_t* GeminiItem::GetItemValueText() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_display_text.c_str();
}

const wchar_t* GeminiItem::GetItemValueSampleText() const
{
    return L"12.5K req";
}

int GeminiItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag)
{
    if (type == MT_LCLICKED)
    {
        ShellExecuteW(nullptr, L"open",
                      L"https://aistudio.google.com/usage",
                      nullptr, nullptr, SW_SHOW);
        return 1;
    }
    return 0;
}

void GeminiItem::SetDisplayText(const std::wstring& text)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_display_text = text;
}
