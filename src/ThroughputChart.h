#pragma once

#include "pch.h"

// Task-manager style real-time throughput chart.
//
// The control is purely presentational.  Only the UI thread may call the
// mutators below: the benchmark worker thread posts BenchmarkUpdate messages to
// CBenchmarkPane, and the pane forwards the samples here while handling those
// messages on the UI thread.
class CThroughputChart : public CWindowImpl<CThroughputChart>
{
public:
    DECLARE_WND_CLASS(L"USBInfoThroughputChart")

    BEGIN_MSG_MAP(CThroughputChart)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_SIZE(OnSize)
        MSG_WM_PAINT(OnPaint)
        MSG_WM_ERASEBKGND(OnEraseBackground)
        MSG_WM_SETFONT(OnSetFont)
    END_MSG_MAP()

    // Appends one interval throughput sample in MB/s and repaints.
    void AddSample(double mbps);

    // Starts a new stage: drops the old curve, sets the stage title and clears
    // any status message.  An empty stage keeps the default title.
    void Clear(const std::wstring& stage);

    // Shows a short status message in the plot area (used on done/cancel).
    void SetMessage(const std::wstring& message);

    // Repaints with the current language.  Titles/messages are translated when
    // painting, so only an invalidation is needed; the samples and the
    // collected result data stay untouched.
    void RefreshLanguage();

private:
    int OnCreate(LPCREATESTRUCT createStruct);
    void OnDestroy();
    void OnSize(UINT type, CSize size);
    void OnPaint(HDC dc);
    BOOL OnEraseBackground(HDC dc);
    void OnSetFont(HFONT font, BOOL redraw);

    void RebuildFonts();
    void Draw(HDC dc, const RECT& client, int dpi);
    double Average() const;
    static double NiceMaximum(double value);

    std::vector<double> m_samples;
    std::wstring m_stage;
    std::wstring m_message;
    double m_total = 0.0;
    HFONT m_baseFont = nullptr;   // borrowed from the parent pane, not owned
    HFONT m_titleFont = nullptr;  // owned
    HFONT m_smallFont = nullptr;  // owned
    int m_dpi = 96;
};
