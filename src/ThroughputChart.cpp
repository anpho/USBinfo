#include "pch.h"
#include "ThroughputChart.h"
#include "Language.h"

#include <cmath>
#include <cwchar>

namespace
{
constexpr size_t kMaxSamples = 240;
constexpr COLORREF kGridColor = RGB(232, 232, 232);
constexpr COLORREF kAxisTextColor = RGB(120, 120, 120);
constexpr COLORREF kTitleColor = RGB(32, 32, 32);
constexpr COLORREF kCurrentColor = RGB(0, 102, 204);
constexpr COLORREF kAverageColor = RGB(88, 88, 88);
constexpr COLORREF kLineColor = RGB(0, 120, 215);
constexpr COLORREF kFillColor = RGB(214, 234, 248);
constexpr COLORREF kBorderColor = RGB(205, 205, 205);
constexpr COLORREF kWaitingColor = RGB(130, 130, 130);
constexpr COLORREF kMessageColor = RGB(20, 100, 180);

int ScaleValue(int value, int dpi)
{
    return ::MulDiv(value, dpi, 96);
}

HFONT PickFont(HFONT font)
{
    return font != nullptr ? font : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
}
}

int CThroughputChart::OnCreate(LPCREATESTRUCT)
{
    m_dpi = static_cast<int>(::GetDpiForWindow(m_hWnd));
    if (m_dpi <= 0)
        m_dpi = 96;
    m_baseFont = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
    RebuildFonts();
    return 0;
}

void CThroughputChart::OnDestroy()
{
    if (m_titleFont != nullptr)
    {
        ::DeleteObject(m_titleFont);
        m_titleFont = nullptr;
    }
    if (m_smallFont != nullptr)
    {
        ::DeleteObject(m_smallFont);
        m_smallFont = nullptr;
    }
}

void CThroughputChart::OnSize(UINT, CSize)
{
    ::InvalidateRect(m_hWnd, nullptr, FALSE);
}

BOOL CThroughputChart::OnEraseBackground(HDC)
{
    // The whole client area is painted by OnPaint, so suppressing the erase
    // step removes flicker.
    return TRUE;
}

void CThroughputChart::OnSetFont(HFONT font, BOOL redraw)
{
    m_baseFont = PickFont(font);
    RebuildFonts();
    if (redraw && ::IsWindow(m_hWnd))
        ::InvalidateRect(m_hWnd, nullptr, FALSE);
}

void CThroughputChart::RebuildFonts()
{
    if (m_titleFont != nullptr)
    {
        ::DeleteObject(m_titleFont);
        m_titleFont = nullptr;
    }
    if (m_smallFont != nullptr)
    {
        ::DeleteObject(m_smallFont);
        m_smallFont = nullptr;
    }

    LOGFONTW base = {};
    HFONT font = PickFont(m_baseFont);
    if (::GetObjectW(font, sizeof(base), &base) != static_cast<int>(sizeof(base)))
    {
        ::GetObjectW(::GetStockObject(DEFAULT_GUI_FONT), sizeof(base), &base);
    }
    if (base.lfHeight == 0)
        base.lfHeight = -::MulDiv(9, m_dpi, 72);
    base.lfQuality = CLEARTYPE_QUALITY;

    LOGFONTW title = base;
    title.lfWeight = FW_BOLD;
    m_titleFont = ::CreateFontIndirectW(&title);

    LOGFONTW smallLog = base;
    const int baseHeight = static_cast<int>(smallLog.lfHeight);
    if (baseHeight < 0)
        smallLog.lfHeight = -std::max(8, (-baseHeight) * 8 / 10);
    else
        smallLog.lfHeight = std::max(8, baseHeight * 8 / 10);
    smallLog.lfWeight = FW_NORMAL;
    m_smallFont = ::CreateFontIndirectW(&smallLog);
}

void CThroughputChart::AddSample(double mbps)
{
    if (!std::isfinite(mbps) || mbps < 0.0)
        return;

    m_message.clear();
    if (m_samples.size() >= kMaxSamples)
    {
        m_total -= m_samples.front();
        m_samples.erase(m_samples.begin());
    }
    m_samples.push_back(mbps);
    m_total += mbps;
    if (::IsWindow(m_hWnd))
        ::InvalidateRect(m_hWnd, nullptr, FALSE);
}

void CThroughputChart::Clear(const std::wstring& stage)
{
    m_samples.clear();
    m_total = 0.0;
    // An empty stage selects the localized default title, drawn in Draw().
    m_stage = stage;
    m_message.clear();
    if (::IsWindow(m_hWnd))
        ::InvalidateRect(m_hWnd, nullptr, FALSE);
}

void CThroughputChart::SetMessage(const std::wstring& message)
{
    m_message = message;
    if (::IsWindow(m_hWnd))
        ::InvalidateRect(m_hWnd, nullptr, FALSE);
}

void CThroughputChart::RefreshLanguage()
{
    // The curve, the samples and the stage name are kept; the captions are
    // translated while painting, so a repaint is enough.
    if (::IsWindow(m_hWnd))
        ::InvalidateRect(m_hWnd, nullptr, FALSE);
}

double CThroughputChart::Average() const
{
    if (m_samples.empty())
        return 0.0;
    return m_total / static_cast<double>(m_samples.size());
}

double CThroughputChart::NiceMaximum(double value)
{
    if (!(value > 0.0))
        return 1.0;
    const double target = value * 1.15;
    const double magnitude = std::pow(10.0, std::floor(std::log10(target)));
    const double normalized = target / magnitude;
    static const double steps[] = { 1.0, 1.2, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 8.0, 10.0 };
    double step = 10.0;
    for (double candidate : steps)
    {
        if (normalized <= candidate + 1e-9)
        {
            step = candidate;
            break;
        }
    }
    return step * magnitude;
}

void CThroughputChart::OnPaint(HDC)
{
    PAINTSTRUCT paint = {};
    HDC dc = ::BeginPaint(m_hWnd, &paint);
    if (dc != nullptr)
    {
        RECT client = {};
        ::GetClientRect(m_hWnd, &client);
        int dpi = static_cast<int>(::GetDpiForWindow(m_hWnd));
        if (dpi <= 0)
            dpi = 96;
        if (dpi != m_dpi)
        {
            m_dpi = dpi;
            RebuildFonts();
        }
        if (client.right > client.left && client.bottom > client.top)
        {
            // Double buffering: draw the whole control off-screen and blit it
            // once, so the frequent sample repaints do not flicker.
            HDC memory = ::CreateCompatibleDC(dc);
            HBITMAP bitmap = memory != nullptr
                ? ::CreateCompatibleBitmap(dc, client.right - client.left, client.bottom - client.top)
                : nullptr;
            if (memory != nullptr && bitmap != nullptr)
            {
                HGDIOBJ oldBitmap = ::SelectObject(memory, bitmap);
                Draw(memory, client, dpi);
                ::BitBlt(dc, 0, 0, client.right - client.left, client.bottom - client.top,
                    memory, 0, 0, SRCCOPY);
                ::SelectObject(memory, oldBitmap);
            }
            if (bitmap != nullptr)
                ::DeleteObject(bitmap);
            if (memory != nullptr)
                ::DeleteDC(memory);
        }
    }
    ::EndPaint(m_hWnd, &paint);
}

void CThroughputChart::Draw(HDC dc, const RECT& client, int dpi)
{
    const int pad = ScaleValue(10, dpi);
    const int headerHeight = ScaleValue(24, dpi);
    const int bottomPad = ScaleValue(8, dpi);
    const int labelWidth = ScaleValue(48, dpi);
    const int gap = ScaleValue(16, dpi);

    ::SetBkMode(dc, TRANSPARENT);
    ::FillRect(dc, &client, static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH)));

    RECT titleRect = { client.left + pad, client.top + pad, client.right - pad, client.top + pad + headerHeight };
    RECT plot = { client.left + pad + labelWidth, client.top + pad + headerHeight + ScaleValue(4, dpi),
        client.right - pad, client.bottom - pad - bottomPad };

    HFONT baseFont = PickFont(m_baseFont);
    HGDIOBJ oldFont = ::SelectObject(dc, PickFont(m_titleFont));

    // Stage title (localized while painting so a language switch only needs a
    // repaint; the stage name itself is kept).
    ::SetTextColor(dc, kTitleColor);
    RECT titleText = titleRect;
    titleText.right = titleText.left + (titleRect.right - titleRect.left) * 45 / 100;
    const std::wstring title = Tr(m_stage.empty() ? L"实时吞吐 (MB/s)" : m_stage.c_str());
    ::DrawTextW(dc, title.c_str(), -1, &titleText,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

    // Current and average throughput, right aligned in the header.
    ::SelectObject(dc, baseFont);
    const bool hasSamples = !m_samples.empty();
    const double current = hasSamples ? m_samples.back() : 0.0;
    const double average = Average();
    wchar_t currentText[64] = {};
    wchar_t averageText[64] = {};
    if (hasSamples)
    {
        swprintf_s(currentText, Tr(L"当前 %.2f MB/s"), current);
        swprintf_s(averageText, Tr(L"平均 %.2f MB/s"), average);
    }
    else
    {
        swprintf_s(currentText, Tr(L"当前 -- MB/s"));
        swprintf_s(averageText, Tr(L"平均 -- MB/s"));
    }

    SIZE currentSize = {};
    SIZE averageSize = {};
    ::GetTextExtentPoint32W(dc, currentText, static_cast<int>(wcslen(currentText)), &currentSize);
    ::GetTextExtentPoint32W(dc, averageText, static_cast<int>(wcslen(averageText)), &averageSize);

    RECT currentRect = titleRect;
    currentRect.left = std::max(titleRect.left, titleRect.right - currentSize.cx);
    ::SetTextColor(dc, kCurrentColor);
    ::DrawTextW(dc, currentText, -1, &currentRect,
        DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    RECT averageRect = titleRect;
    averageRect.right = currentRect.left - gap;
    averageRect.left = std::max(titleRect.left, averageRect.right - averageSize.cx);
    if (averageRect.right > averageRect.left)
    {
        ::SetTextColor(dc, kAverageColor);
        ::DrawTextW(dc, averageText, -1, &averageRect,
            DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }

    if (plot.right <= plot.left || plot.bottom <= plot.top)
    {
        ::SelectObject(dc, oldFont);
        HBRUSH border = ::CreateSolidBrush(kBorderColor);
        ::FrameRect(dc, &client, border);
        ::DeleteObject(border);
        return;
    }

    double maximum = 0.0;
    for (double sample : m_samples)
        maximum = std::max(maximum, sample);
    const double yMax = NiceMaximum(maximum);

    // Light grid: four horizontal bands and six vertical divisions.
    HPEN gridPen = ::CreatePen(PS_SOLID, 1, kGridColor);
    HGDIOBJ oldPen = ::SelectObject(dc, gridPen);
    for (int i = 0; i <= 4; ++i)
    {
        const int y = plot.top + (plot.bottom - plot.top) * i / 4;
        ::MoveToEx(dc, plot.left, y, nullptr);
        ::LineTo(dc, plot.right, y);
    }
    for (int i = 0; i <= 6; ++i)
    {
        const int x = plot.left + (plot.right - plot.left) * i / 6;
        ::MoveToEx(dc, x, plot.top, nullptr);
        ::LineTo(dc, x, plot.bottom);
    }
    ::SelectObject(dc, oldPen);
    ::DeleteObject(gridPen);

    // Scale labels on the left gutter.
    if (hasSamples)
    {
        HGDIOBJ oldSmallFont = ::SelectObject(dc, PickFont(m_smallFont));
        ::SetTextColor(dc, kAxisTextColor);
        for (int i = 0; i <= 4; ++i)
        {
            const int y = plot.top + (plot.bottom - plot.top) * i / 4;
            const double value = yMax * (4 - i) / 4.0;
            wchar_t label[32] = {};
            if (yMax >= 10.0)
                swprintf_s(label, L"%.0f", value);
            else
                swprintf_s(label, L"%.1f", value);
            RECT labelRect = { client.left + pad, y - ScaleValue(9, dpi),
                plot.left - ScaleValue(6, dpi), y + ScaleValue(9, dpi) };
            ::DrawTextW(dc, label, -1, &labelRect,
                DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        }
        ::SelectObject(dc, oldSmallFont);
    }

    if (hasSamples)
    {
        const int plotWidth = plot.right - plot.left;
        const int plotHeight = plot.bottom - plot.top;
        const size_t count = m_samples.size();

        std::vector<POINT> points;
        points.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            const double ratio = count <= 1 ? 1.0 : static_cast<double>(i) / static_cast<double>(count - 1);
            const double value = std::clamp(m_samples[i], 0.0, yMax);
            POINT point = {};
            point.x = plot.left + static_cast<int>(ratio * plotWidth);
            int y = plot.bottom - static_cast<int>((value / yMax) * plotHeight);
            y = std::clamp(y, static_cast<int>(plot.top), static_cast<int>(plot.bottom));
            point.y = y;
            points.push_back(point);
        }

        // Filled area below the line.
        std::vector<POINT> area;
        area.reserve(points.size() + 2);
        area.push_back({ plot.left, plot.bottom });
        area.insert(area.end(), points.begin(), points.end());
        area.push_back({ plot.right, plot.bottom });

        HBRUSH fillBrush = ::CreateSolidBrush(kFillColor);
        HGDIOBJ oldBrush = ::SelectObject(dc, fillBrush);
        HGDIOBJ oldFillPen = ::SelectObject(dc, ::GetStockObject(NULL_PEN));
        ::Polygon(dc, area.data(), static_cast<int>(area.size()));
        ::SelectObject(dc, oldFillPen);
        ::SelectObject(dc, oldBrush);
        ::DeleteObject(fillBrush);

        // Blue throughput line.
        HPEN linePen = ::CreatePen(PS_SOLID, std::max(1, ScaleValue(2, dpi)), kLineColor);
        HGDIOBJ oldLinePen = ::SelectObject(dc, linePen);
        if (points.size() > 1)
        {
            ::Polyline(dc, points.data(), static_cast<int>(points.size()));
        }
        else
        {
            HBRUSH dot = ::CreateSolidBrush(kLineColor);
            HGDIOBJ oldDotBrush = ::SelectObject(dc, dot);
            HGDIOBJ oldDotPen = ::SelectObject(dc, ::GetStockObject(NULL_PEN));
            const int radius = ScaleValue(3, dpi);
            ::Ellipse(dc, points[0].x - radius, points[0].y - radius,
                points[0].x + radius, points[0].y + radius);
            ::SelectObject(dc, oldDotPen);
            ::SelectObject(dc, oldDotBrush);
            ::DeleteObject(dot);
        }
        ::SelectObject(dc, oldLinePen);
        ::DeleteObject(linePen);
    }

    // Center message: the localized "waiting" text while idle, status text
    // after done/cancel (translated while painting).
    const std::wstring message = !m_message.empty()
        ? std::wstring(Tr(m_message))
        : (hasSamples ? std::wstring() : std::wstring(Tr(L"等待测速…")));
    if (!message.empty())
    {
        ::SelectObject(dc, baseFont);
        ::SetTextColor(dc, m_message.empty() ? kWaitingColor : kMessageColor);
        RECT messageRect = plot;
        ::DrawTextW(dc, message.c_str(), -1, &messageRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    ::SelectObject(dc, oldFont);
    HBRUSH border = ::CreateSolidBrush(kBorderColor);
    ::FrameRect(dc, &client, border);
    ::DeleteObject(border);
}
