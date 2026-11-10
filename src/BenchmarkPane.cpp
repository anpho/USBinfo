#include "pch.h"
#include "BenchmarkPane.h"
#include "Messages.h"
#include "Language.h"

#include <cstring>

namespace
{
// Context-menu commands for the result list.  They are private to this pane,
// so no IDs are added to the shared Messages.h.
constexpr UINT kCmdCopyResultRow = 0x7E10;
constexpr UINT kCmdCopyAllResults = 0x7E11;
constexpr int kResultColumns = 7;

int ScaleForWindow(HWND window, int value)
{
    UINT dpi = window != nullptr ? ::GetDpiForWindow(window) : 96;
    if (dpi == 0)
        dpi = 96;
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

std::wstring FormatSeconds(double seconds)
{
    wchar_t buffer[64] = {};
    swprintf_s(buffer, Tr(L"%.3f s"), seconds);
    return buffer;
}

std::wstring FormatRate(double mbps)
{
    wchar_t buffer[64] = {};
    swprintf_s(buffer, Tr(L"%.2f MB/s"), mbps);
    return buffer;
}

std::wstring FormatIops(double iops)
{
    wchar_t buffer[64] = {};
    swprintf_s(buffer, L"%.0f", iops);
    return buffer;
}

std::wstring FormatLatency(double milliseconds)
{
    wchar_t buffer[64] = {};
    swprintf_s(buffer, Tr(L"%.3f ms"), milliseconds);
    return buffer;
}

// Updates an existing result-list column caption (LVM_SETCOLUMN) so
// RefreshLanguage does not have to recreate the list.
void SetColumnText(CListViewCtrl& list, int column, const wchar_t* text)
{
    if (list.m_hWnd == nullptr || text == nullptr)
        return;
    LVCOLUMNW info = {};
    info.mask = LVCF_TEXT;
    info.pszText = const_cast<LPWSTR>(text);
    ::SendMessageW(list.m_hWnd, LVM_SETCOLUMN, static_cast<WPARAM>(column), reinterpret_cast<LPARAM>(&info));
}

std::wstring VolumeDisplayLabel(const VolumeInfo& volume)
{
    std::wstring label = volume.rootPath;
    if (!volume.label.empty())
        label += L"  " + volume.label;
    label += L"  " + FormatBytesShort(volume.totalBytes);
    if (volume.freeBytes > 0)
    {
        wchar_t buffer[128] = {};
        swprintf_s(buffer, Tr(L"，可用 %s"), FormatBytesShort(volume.freeBytes).c_str());
        label += buffer;
    }
    if (volume.readOnly)
        label += Tr(L"（只读）");
    return label;
}

std::wstring TargetDisplayLabel(const StorageInfo& storage)
{
    std::wstring target = storage.model.empty() ? std::wstring(Tr(L"USB 存储设备")) : storage.model;
    if (!storage.bridgeChip.empty())
    {
        wchar_t buffer[256] = {};
        swprintf_s(buffer, Tr(L"桥接: %s"), storage.bridgeChip.c_str());
        target += L"  ·  " + std::wstring(buffer);
    }
    if (storage.capacityBytes > 0)
        target += L"  ·  " + FormatBytes(storage.capacityBytes);
    return target;
}

std::wstring TimeoutText(uint32_t seconds)
{
    if (seconds == 0)
        return Tr(L"无限制");
    wchar_t buffer[64] = {};
    swprintf_s(buffer, Tr(L"%u 秒"), seconds);
    return buffer;
}

bool TextContainsAny(const std::wstring& value, const wchar_t* source)
{
    if (ContainsNoCase(value, source))
        return true;
    return ContainsNoCase(value, Tr(source));
}

bool TextStartsWithAny(const std::wstring& value, const wchar_t* source)
{
    if (StartsWithNoCase(value, source))
        return true;
    return StartsWithNoCase(value, Tr(source));
}

std::wstring ResultRowText(HWND list, int row)
{
    std::wstring text;
    for (int column = 0; column < kResultColumns; ++column)
    {
        if (column > 0)
            text.push_back(L'\t');
        wchar_t value[512] = {};
        ListView_GetItemText(list, row, column, value, static_cast<int>(sizeof(value) / sizeof(value[0])));
        text += value;
    }
    return text;
}

std::wstring AllResultsText(HWND list, int rows)
{
    std::wstring text;
    for (int column = 0; column < kResultColumns; ++column)
    {
        if (column > 0)
            text.push_back(L'\t');
        wchar_t value[128] = {};
        LVCOLUMNW info = {};
        info.mask = LVCF_TEXT;
        info.pszText = value;
        info.cchTextMax = static_cast<int>(sizeof(value) / sizeof(value[0]));
        if (ListView_GetColumn(list, column, &info) != FALSE)
            text += value;
    }
    for (int row = 0; row < rows; ++row)
    {
        text += L"\r\n";
        text += ResultRowText(list, row);
    }
    return text;
}

void SetClipboardText(HWND owner, const std::wstring& text)
{
    if (text.empty())
        return;
    if (!::OpenClipboard(owner))
        return;

    if (::EmptyClipboard())
    {
        const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (memory != nullptr)
        {
            void* target = ::GlobalLock(memory);
            if (target != nullptr)
            {
                std::memcpy(target, text.c_str(), bytes);
                ::GlobalUnlock(memory);
                if (::SetClipboardData(CF_UNICODETEXT, memory) != nullptr)
                    memory = nullptr; // ownership transferred to the clipboard
            }
            if (memory != nullptr)
                ::GlobalFree(memory);
        }
    }
    ::CloseClipboard();
}
}

int CBenchmarkPane::OnCreate(LPCREATESTRUCT)
{
    m_font = AtlCreateControlFont();
    m_boldFont = AtlCreateBoldFont(m_font);

    m_targetLabel.Create(m_hWnd, rcDefault, Tr(L"选择一个 USB 存储设备以进行测速。"),
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, 0);
    m_targetLabel.SetFont(m_boldFont);

    m_volumeLabel.Create(m_hWnd, rcDefault, Tr(L"测试卷:"), WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, 0);
    m_volumeLabel.SetFont(m_font);
    m_volume.Create(m_hWnd, rcDefault, nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 0, IDC_BENCH_VOLUME);
    m_volume.SetFont(m_font);

    m_sizeLabel.Create(m_hWnd, rcDefault, Tr(L"大文件大小:"), WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, 0);
    m_sizeLabel.SetFont(m_font);
    m_size.Create(m_hWnd, rcDefault, nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 0, IDC_BENCH_SIZE);
    m_size.SetFont(m_font);

    m_timeoutLabel.Create(m_hWnd, rcDefault, Tr(L"每项超时:"), WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, 0);
    m_timeoutLabel.SetFont(m_font);
    m_timeout.Create(m_hWnd, rcDefault, nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 0, IDC_BENCH_TIMEOUT);
    m_timeout.SetFont(m_font);

    m_start.Create(m_hWnd, rcDefault, Tr(L"开始测速"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0, IDC_BENCH_START);
    m_start.SetFont(m_font);
    m_cancel.Create(m_hWnd, rcDefault, Tr(L"取消"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0, IDC_BENCH_CANCEL);
    m_cancel.SetFont(m_font);
    m_cancel.EnableWindow(FALSE);

    m_progress.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 0, IDC_BENCH_PROGRESS);
    m_progress.SetRange(0, 100);
    m_progress.SetPos(0);

    m_status.Create(m_hWnd, rcDefault, Tr(L"等待测速。"), WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, 0);
    m_status.SetFont(m_font);

    m_warning.Create(m_hWnd, rcDefault,
        Tr(L"测速会在所选卷上创建临时文件，仅通过文件系统写入，不会直接写裸盘；测试完成后自动删除。"
           L"测试会产生写入、占用空间并使设备发热，请确认卷上有足够空间。"),
        WS_CHILD | WS_VISIBLE | SS_LEFT, 0);
    m_warning.SetFont(m_font);

    m_chart.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, IDC_BENCH_CHART);
    m_chart.SetFont(m_font);

    m_results.Create(m_hWnd, rcDefault, nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        WS_EX_CLIENTEDGE, IDC_BENCH_RESULTS);
    m_results.SetFont(m_font);
    m_results.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    m_results.InsertColumn(0, Tr(L"测试"), LVCFMT_LEFT, ScaleForWindow(m_hWnd, 196));
    m_results.InsertColumn(1, Tr(L"数据量"), LVCFMT_RIGHT, ScaleForWindow(m_hWnd, 84));
    m_results.InsertColumn(2, Tr(L"耗时"), LVCFMT_RIGHT, ScaleForWindow(m_hWnd, 77));
    m_results.InsertColumn(3, Tr(L"速度"), LVCFMT_RIGHT, ScaleForWindow(m_hWnd, 98));
    m_results.InsertColumn(4, L"IOPS", LVCFMT_RIGHT, ScaleForWindow(m_hWnd, 70));
    m_results.InsertColumn(5, Tr(L"延迟"), LVCFMT_RIGHT, ScaleForWindow(m_hWnd, 84));
    m_results.InsertColumn(6, Tr(L"状态"), LVCFMT_LEFT, ScaleForWindow(m_hWnd, 91));

    m_size.AddString(L"128 MB");
    m_size.AddString(L"256 MB");
    m_size.AddString(L"512 MB");
    m_size.AddString(L"1024 MB");
    m_size.SetItemData(0, 128);
    m_size.SetItemData(1, 256);
    m_size.SetItemData(2, 512);
    m_size.SetItemData(3, 1024);
    m_size.SetCurSel(1);
    RebuildTimeoutCombo();

    LayoutChildren();
    SetRunningUi(false);
    return 0;
}

void CBenchmarkPane::OnSize(UINT, CSize)
{
    LayoutChildren();
}

void CBenchmarkPane::OnDestroy()
{
    m_benchCancel = true;
    JoinThread();
    if (m_boldFont != nullptr)
    {
        ::DeleteObject(m_boldFont);
        m_boldFont = nullptr;
    }
    if (m_font != nullptr)
    {
        ::DeleteObject(m_font);
        m_font = nullptr;
    }
}

BOOL CBenchmarkPane::OnEraseBackground(HDC dc)
{
    // Paint the pane background with the system window colour instead of the
    // default grey control face, so the tab content blends with the rest of the
    // detail page.  WS_CLIPCHILDREN keeps the chart and result list untouched.
    RECT client = {};
    ::GetClientRect(m_hWnd, &client);
    ::FillRect(dc, &client, ::GetSysColorBrush(COLOR_WINDOW));
    return TRUE;
}

HBRUSH CBenchmarkPane::OnCtlColorStatic(HDC dc, HWND)
{
    // Static labels (target, status, warning, group captions) must show the
    // window background, not the grey default control face.
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, ::GetSysColor(COLOR_WINDOWTEXT));
    return ::GetSysColorBrush(COLOR_WINDOW);
}

void CBenchmarkPane::LayoutChildren()
{
    if (!::IsWindow(m_hWnd))
        return;
    RECT client = {};
    ::GetClientRect(m_hWnd, &client);
    const int margin = ScaleForWindow(m_hWnd, 12);
    const int width = std::max(10, static_cast<int>(client.right) - margin * 2);
    const int rowHeight = ScaleForWindow(m_hWnd, 26);
    const int buttonWidth = ScaleForWindow(m_hWnd, 96);
    const int gap = ScaleForWindow(m_hWnd, 8);
    int y = ScaleForWindow(m_hWnd, 6);

    m_targetLabel.MoveWindow(margin, y, width, ScaleForWindow(m_hWnd, 22));
    y += ScaleForWindow(m_hWnd, 30);

    int volumeWidth = std::min(ScaleForWindow(m_hWnd, 240),
        std::max(ScaleForWindow(m_hWnd, 120), width - ScaleForWindow(m_hWnd, 460)));
    m_volumeLabel.MoveWindow(margin, y + 3, ScaleForWindow(m_hWnd, 64), rowHeight);
    m_volume.MoveWindow(margin + ScaleForWindow(m_hWnd, 66), y, volumeWidth, ScaleForWindow(m_hWnd, 200));

    int sizeX = margin + ScaleForWindow(m_hWnd, 66) + volumeWidth + ScaleForWindow(m_hWnd, 12);
    m_sizeLabel.MoveWindow(sizeX, y + 3, ScaleForWindow(m_hWnd, 82), rowHeight);
    m_size.MoveWindow(sizeX + ScaleForWindow(m_hWnd, 84), y, ScaleForWindow(m_hWnd, 96), ScaleForWindow(m_hWnd, 180));

    int timeoutX = sizeX + ScaleForWindow(m_hWnd, 84) + ScaleForWindow(m_hWnd, 96) + ScaleForWindow(m_hWnd, 12);
    m_timeoutLabel.MoveWindow(timeoutX, y + 3, ScaleForWindow(m_hWnd, 82), rowHeight);
    m_timeout.MoveWindow(timeoutX + ScaleForWindow(m_hWnd, 84), y, ScaleForWindow(m_hWnd, 88), ScaleForWindow(m_hWnd, 180));
    y += ScaleForWindow(m_hWnd, 36);

    m_start.MoveWindow(static_cast<int>(client.right) - margin - buttonWidth * 2 - gap,
        y, buttonWidth, rowHeight);
    m_cancel.MoveWindow(static_cast<int>(client.right) - margin - buttonWidth, y, buttonWidth, rowHeight);
    y += ScaleForWindow(m_hWnd, 36);

    m_progress.MoveWindow(margin, y, width, ScaleForWindow(m_hWnd, 18));
    y += ScaleForWindow(m_hWnd, 24);
    m_status.MoveWindow(margin, y, width, ScaleForWindow(m_hWnd, 22));
    y += ScaleForWindow(m_hWnd, 26);
    m_warning.MoveWindow(margin, y, width, ScaleForWindow(m_hWnd, 44));
    y += ScaleForWindow(m_hWnd, 50) + gap;

    // The live chart sits between the warning/status block and the result table.
    // At 96 dpi it gets 130-150 px and the table keeps the rest.
    const int available = std::max(0, static_cast<int>(client.bottom) - y - margin);
    const int minResults = ScaleForWindow(m_hWnd, 120);
    int chartHeight = std::clamp(ScaleForWindow(m_hWnd, 140),
        ScaleForWindow(m_hWnd, 110), ScaleForWindow(m_hWnd, 160));
    if (available - chartHeight - gap < minResults)
        chartHeight = available - minResults - gap;
    if (chartHeight < ScaleForWindow(m_hWnd, 90))
        chartHeight = std::min(available, ScaleForWindow(m_hWnd, 90));
    chartHeight = std::max(0, std::min(chartHeight, available));

    m_chart.MoveWindow(margin, y, width, chartHeight);
    const int resultsY = y + chartHeight + gap;
    const int resultsHeight = std::max(0, static_cast<int>(client.bottom) - resultsY - margin);
    m_results.MoveWindow(margin, resultsY, width, resultsHeight);

    // Size columns from the actual list client width (after the vertical
    // scrollbar is accounted for) so no horizontal scrollbar is ever needed.
    RECT resultsClient = {};
    m_results.GetClientRect(&resultsClient);
    const int tableWidth = std::max(120, static_cast<int>(resultsClient.right));
    int widths[kResultColumns] = {};
    widths[0] = tableWidth * 28 / 100;
    widths[1] = tableWidth * 12 / 100;
    widths[2] = tableWidth * 11 / 100;
    widths[3] = tableWidth * 14 / 100;
    widths[4] = tableWidth * 10 / 100;
    widths[5] = tableWidth * 12 / 100;
    widths[6] = tableWidth - widths[0] - widths[1] - widths[2] - widths[3] - widths[4] - widths[5];
    if (widths[6] < 50)
    {
        widths[6] = 50;
        int excess = 50 - (tableWidth - widths[0] - widths[1] - widths[2] - widths[3] - widths[4] - widths[5]);
        for (int column = 0; column < 6 && excess > 0; ++column)
        {
            int reducible = std::max(0, widths[column] - 40);
            int reduce = std::min(reducible, excess);
            widths[column] -= reduce;
            excess -= reduce;
        }
    }
    for (int column = 0; column < kResultColumns; ++column)
        m_results.SetColumnWidth(column, widths[column]);
    ::ShowScrollBar(m_results, SB_HORZ, FALSE);
}

void CBenchmarkPane::ResetChart(const std::wstring& title, const std::wstring& message)
{
    m_chartStage.clear();
    m_chart.Clear(title);
    if (!message.empty())
        m_chart.SetMessage(message);
}

void CBenchmarkPane::RebuildTimeoutCombo()
{
    if (!::IsWindow(m_timeout))
        return;
    uint32_t selected = 0;
    const int current = m_timeout.GetCurSel();
    if (current >= 0)
        selected = static_cast<uint32_t>(m_timeout.GetItemData(current));

    m_timeout.ResetContent();
    const uint32_t values[] = { 3, 5, 10, 0 };
    for (uint32_t value : values)
    {
        const int index = m_timeout.AddString(TimeoutText(value).c_str());
        if (index >= 0)
            m_timeout.SetItemData(index, value);
    }

    int defaultIndex = 0;
    for (int i = 0; i < m_timeout.GetCount(); ++i)
    {
        if (m_timeout.GetItemData(i) == selected)
        {
            defaultIndex = i;
            break;
        }
    }
    m_timeout.SetCurSel(defaultIndex);
}

void CBenchmarkPane::SetTarget(const StorageInfo* storage)
{
    m_hasTarget = storage != nullptr && storage->present;
    if (!m_hasTarget)
    {
        ClearTarget();
        return;
    }

    std::wstring targetKey = storage->pnpInstance;
    if (targetKey.empty())
        targetKey = storage->diskPath + L"|" + storage->model;
    const bool targetChanged = targetKey != m_targetKey;
    if (targetChanged)
    {
        if (m_running)
            m_benchCancel = true;
        m_results.DeleteAllItems();
        m_progress.SetPos(0);
        m_chartStage.clear();
        m_chart.Clear(std::wstring());
        m_chart.SetMessage(L"等待测速…");
        m_targetKey = targetKey;
    }

    m_storage = *storage;

    // A new target starts from the first writable volume, matching the previous
    // behaviour; the selection is only preserved across language switches.
    RebuildVolumeCombo(!targetChanged);

    m_targetLabel.SetWindowText(TargetDisplayLabel(m_storage).c_str());
    m_status.SetWindowText(Tr(L"请选择卷和文件大小，然后点击“开始测速”。"));
    m_progress.SetPos(0);
    if (!m_running)
        ResetChart(L"", L"");
    SetRunningUi(m_running);
}

void CBenchmarkPane::RebuildVolumeCombo(bool preserveSelection)
{
    DWORD_PTR previous = static_cast<DWORD_PTR>(-1);
    if (preserveSelection)
    {
        const int current = m_volume.GetCurSel();
        if (current >= 0)
            previous = m_volume.GetItemData(current);
    }

    m_volume.ResetContent();
    m_hasWritableVolume = false;
    for (size_t i = 0; i < m_storage.volumes.size(); ++i)
    {
        const VolumeInfo& volume = m_storage.volumes[i];
        if (volume.rootPath.empty() || volume.totalBytes == 0)
            continue;
        const int index = m_volume.AddString(VolumeDisplayLabel(volume).c_str());
        if (index >= 0)
        {
            m_volume.SetItemData(index, static_cast<DWORD_PTR>(i));
            if (!volume.readOnly)
            {
                m_hasWritableVolume = true;
                if (!preserveSelection && m_volume.GetCurSel() < 0)
                    m_volume.SetCurSel(index);
            }
        }
    }

    if (!m_hasWritableVolume)
    {
        m_volume.AddString(Tr(L"未找到可写卷（请确认设备已挂载并具有写权限）"));
        m_volume.SetCurSel(0);
        return;
    }

    if (!preserveSelection)
        return;

    // Restore the previously selected volume, otherwise fall back to the first
    // writable one exactly like a fresh target does.
    const int count = m_volume.GetCount();
    for (int i = 0; i < count; ++i)
    {
        if (m_volume.GetItemData(i) == previous)
        {
            m_volume.SetCurSel(i);
            return;
        }
    }
    for (int i = 0; i < count; ++i)
    {
        const DWORD_PTR data = m_volume.GetItemData(i);
        if (data < m_storage.volumes.size() && !m_storage.volumes[static_cast<size_t>(data)].readOnly)
        {
            m_volume.SetCurSel(i);
            return;
        }
    }
}

void CBenchmarkPane::ClearTarget()
{
    m_hasTarget = false;
    m_hasWritableVolume = false;
    m_storage = StorageInfo{};
    m_targetKey.clear();
    m_results.DeleteAllItems();
    m_progress.SetPos(0);
    m_chartStage.clear();
    m_chart.Clear(std::wstring());
    m_chart.SetMessage(L"等待测速…");
    m_volume.ResetContent();
    m_volume.AddString(Tr(L"仅 USB 存储设备支持测速"));
    m_volume.SetCurSel(0);
    m_targetLabel.SetWindowText(Tr(L"仅 USB 存储设备支持测速。"));
    m_status.SetWindowText(Tr(L"请选择一个 USB 存储设备节点。"));
    m_progress.SetPos(0);
    if (!m_running)
        ResetChart(L"", L"");
    SetRunningUi(false);
}

void CBenchmarkPane::RefreshLanguage()
{
    if (!::IsWindow(m_hWnd))
        return;

    m_volumeLabel.SetWindowText(Tr(L"测试卷:"));
    m_sizeLabel.SetWindowText(Tr(L"大文件大小:"));
    m_timeoutLabel.SetWindowText(Tr(L"每项超时:"));
    m_start.SetWindowText(Tr(L"开始测速"));
    m_cancel.SetWindowText(Tr(L"取消"));
    m_warning.SetWindowText(Tr(L"测速会在所选卷上创建临时文件，仅通过文件系统写入，不会直接写裸盘；测试完成后自动删除。"
                               L"测试会产生写入、占用空间并使设备发热，请确认卷上有足够空间。"));

    // Result table captions are updated in place through LVM_SETCOLUMN.
    SetColumnText(m_results, 0, Tr(L"测试"));
    SetColumnText(m_results, 1, Tr(L"数据量"));
    SetColumnText(m_results, 2, Tr(L"耗时"));
    SetColumnText(m_results, 3, Tr(L"速度"));
    SetColumnText(m_results, 4, L"IOPS");
    SetColumnText(m_results, 5, Tr(L"延迟"));
    SetColumnText(m_results, 6, Tr(L"状态"));

    if (m_hasTarget)
    {
        // Rebuild the localized volume labels in place; the measurement, the
        // collected results and the chart curve are left untouched.
        RebuildVolumeCombo(true);
        m_targetLabel.SetWindowText(TargetDisplayLabel(m_storage).c_str());
        if (!m_running)
            m_status.SetWindowText(Tr(L"请选择卷和文件大小，然后点击“开始测速”。"));
    }
    else
    {
        m_volume.ResetContent();
        m_volume.AddString(Tr(L"仅 USB 存储设备支持测速"));
        m_volume.SetCurSel(0);
        m_targetLabel.SetWindowText(Tr(L"仅 USB 存储设备支持测速。"));
        m_status.SetWindowText(Tr(L"请选择一个 USB 存储设备节点。"));
    }

    m_chart.RefreshLanguage();
    RebuildTimeoutCombo();
    SetRunningUi(m_running);
    LayoutChildren();
}

void CBenchmarkPane::FocusStart()
{
    if (m_start.IsWindow() && m_start.IsWindowEnabled())
        m_start.SetFocus();
}

void CBenchmarkPane::JoinThread()
{
    if (m_thread.joinable())
        m_thread.join();
}

void CBenchmarkPane::SetRunningUi(bool running)
{
    m_start.EnableWindow(!running && m_hasTarget && m_hasWritableVolume);
    m_cancel.EnableWindow(running);
    m_volume.EnableWindow(!running);
    m_size.EnableWindow(!running);
    m_timeout.EnableWindow(!running);
}

LRESULT CBenchmarkPane::OnStart(WORD, WORD, HWND, BOOL&)
{
    if (m_running || !m_hasTarget || !m_hasWritableVolume)
        return 0;

    int selection = m_volume.GetCurSel();
    if (selection < 0)
        return 0;
    DWORD_PTR volumeIndex = m_volume.GetItemData(selection);
    if (volumeIndex >= m_storage.volumes.size())
        return 0;
    const VolumeInfo& volume = m_storage.volumes[static_cast<size_t>(volumeIndex)];
    if (volume.rootPath.empty() || volume.readOnly)
    {
        ::MessageBoxW(m_hWnd, Tr(L"所选卷没有可用的写路径或为只读。"), Tr(L"USBInfo 测速"), MB_OK | MB_ICONWARNING);
        return 0;
    }

    int sizeSelection = m_size.GetCurSel();
    uint64_t largeMb = 256;
    if (sizeSelection >= 0)
        largeMb = m_size.GetItemData(sizeSelection);
    if (largeMb < 32)
        largeMb = 32;

    uint32_t timeoutSeconds = 0;
    const int timeoutSelection = m_timeout.GetCurSel();
    if (timeoutSelection >= 0)
        timeoutSeconds = static_cast<uint32_t>(m_timeout.GetItemData(timeoutSelection));

    BenchmarkConfig config;
    config.volumeRoot = volume.rootPath;
    config.largeBytes = largeMb * 1024ull * 1024ull;
    config.largeBlock = 1024 * 1024;
    config.smallBytes = std::min<uint64_t>(32ull * 1024 * 1024, config.largeBytes);
    config.smallBlock = 4096;
    config.timeoutSeconds = timeoutSeconds;
    const uint64_t smallMb = config.smallBytes / (1024ull * 1024ull);

    wchar_t question[2048] = {};
    swprintf_s(question,
        Tr(L"将在以下卷创建临时测速文件：\n\n%s\n\n"
           L"大文件：%llu MB × 4 项（1 MiB 块：顺序写/读、随机写/读）\n"
           L"小数据：%llu MB × 4 项（4 KiB 块：顺序写/读、随机写/读）\n\n"
           L"测试只写文件系统，不会写裸盘；完成后自动删除。\n"
           L"测试会产生实际写入并占用空间，是否继续？"),
        volume.rootPath.c_str(),
        static_cast<unsigned long long>(largeMb),
        static_cast<unsigned long long>(smallMb));
    std::wstring questionText = question;
    questionText += L"\n";
    questionText += Tr(L"每项超时:");
    questionText += L" ";
    questionText += TimeoutText(timeoutSeconds);
    if (::MessageBoxW(m_hWnd, questionText.c_str(), Tr(L"USBInfo 测速确认"),
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return 0;

    JoinThread();
    m_benchCancel = false;
    m_running = true;
    m_results.DeleteAllItems();
    m_progress.SetPos(0);
    m_status.SetWindowText(Tr(L"准备测试文件…"));
    ResetChart(L"准备测试文件…", L"正在创建临时测试文件…");
    SetRunningUi(true);

    m_thread = std::thread([this, config]()
    {
        try
        {
            RunBenchmark(config, m_benchCancel, [this](const BenchmarkUpdate& update)
            {
                auto* copy = new BenchmarkUpdate(update);
                if (!::PostMessageW(m_hWnd, WM_APP_BENCH_UPDATE, 0, reinterpret_cast<LPARAM>(copy)))
                    delete copy;
            });
        }
        catch (const std::exception& ex)
        {
            BenchmarkUpdate update;
            update.done = true;
            update.ok = false;
            update.message = Tr(L"测速线程异常：");
            int length = ::MultiByteToWideChar(CP_UTF8, 0, ex.what(), -1, nullptr, 0);
            if (length > 1)
            {
                std::wstring text(static_cast<size_t>(length), L'\0');
                ::MultiByteToWideChar(CP_UTF8, 0, ex.what(), -1, text.data(), length);
                update.message += text.c_str();
            }
            update.percent = 100;
            auto* copy = new BenchmarkUpdate(update);
            if (!::PostMessageW(m_hWnd, WM_APP_BENCH_UPDATE, 0, reinterpret_cast<LPARAM>(copy)))
                delete copy;
        }
    });
    return 0;
}

LRESULT CBenchmarkPane::OnCancel(WORD, WORD, HWND, BOOL&)
{
    if (!m_running)
        return 0;
    m_benchCancel = true;
    m_cancel.EnableWindow(FALSE);
    m_status.SetWindowText(Tr(L"正在取消测试并清理临时文件…"));
    m_chart.SetMessage(L"正在取消测试…");
    return 0;
}

LRESULT CBenchmarkPane::OnBenchUpdate(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    std::unique_ptr<BenchmarkUpdate> update(reinterpret_cast<BenchmarkUpdate*>(lParam));
    if (!update)
        return 0;

    m_progress.SetPos(std::clamp(update->percent, 0, 100));

    // "提示：…" updates only carry advice about buffering; they must not
    // replace the chart title or reset the curve.  The prefix may already be
    // localized by the benchmark layer, so accept both forms.
    const bool isHint = TextStartsWithAny(update->stage, L"提示");
    if (!update->done && !isHint && !update->stage.empty() && update->stage != m_chartStage)
    {
        m_chartStage = update->stage;
        m_chart.Clear(m_chartStage);
    }

    if (!update->stage.empty())
    {
        const std::wstring stageText = Tr(update->stage);
        m_status.SetWindowText(stageText.c_str());
    }

    if (update->sampleMbps.has_value())
        m_chart.AddSample(*update->sampleMbps);

    if (update->result.has_value())
        AddResult(*update->result);

    if (update->done)
    {
        m_running = false;
        SetRunningUi(false);
        // The worker may still be deleting the large temp file. Do not join it
        // on the UI thread; allow an immediate new run.
        if (m_thread.joinable())
            m_thread.detach();
        m_chartStage.clear();
        if (update->ok)
        {
            m_status.SetWindowText(Tr(L"测速完成"));
            m_chart.SetMessage(L"测速完成");
        }
        else
        {
            wchar_t message[2048] = {};
            if (update->message.empty())
                swprintf_s(message, L"%s", Tr(L"测速未完成。"));
            else
                swprintf_s(message, Tr(L"测速未完成。 %s"), update->message.c_str());
            m_status.SetWindowText(message);
            // The benchmark layer may report the cancellation in the current
            // language; the cancel flag is the reliable signal.
            const bool cancelled = m_benchCancel.load() || TextContainsAny(update->message, L"取消");
            m_chart.SetMessage(cancelled ? L"测试已取消" : L"测速未完成");
        }
        m_cancel.EnableWindow(FALSE);
    }
    return 0;
}

LRESULT CBenchmarkPane::OnResultsRightClick(int, LPNMHDR notifyHeader, BOOL& handled)
{
    handled = TRUE;
    if (notifyHeader != nullptr)
    {
        auto* item = reinterpret_cast<LPNMITEMACTIVATE>(notifyHeader);
        if (item->iItem >= 0 && item->iItem < m_results.GetItemCount())
        {
            ListView_SetItemState(m_results.m_hWnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_SetItemState(m_results.m_hWnd, item->iItem,
                LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            m_results.SetFocus();
        }
    }

    POINT screenPoint = {};
    ::GetCursorPos(&screenPoint);
    ShowResultsMenu(screenPoint);
    return 0;
}

void CBenchmarkPane::ShowResultsMenu(const POINT& screenPoint)
{
    const bool hasRows = m_results.GetItemCount() > 0;
    const bool hasSelection = m_results.GetSelectedCount() > 0;

    HMENU menu = ::CreatePopupMenu();
    if (menu == nullptr)
        return;

    UINT rowFlags = MF_STRING;
    if (!hasSelection)
        rowFlags |= MF_GRAYED;
    ::AppendMenuW(menu, rowFlags, kCmdCopyResultRow, Tr(L"复制当前行"));

    UINT allFlags = MF_STRING;
    if (!hasRows)
        allFlags |= MF_GRAYED;
    ::AppendMenuW(menu, allFlags, kCmdCopyAllResults, Tr(L"复制全部结果"));

    ::SetForegroundWindow(m_hWnd);
    const UINT command = static_cast<UINT>(::TrackPopupMenu(menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN,
        screenPoint.x, screenPoint.y, 0, m_hWnd, nullptr));
    ::DestroyMenu(menu);
    ::PostMessageW(m_hWnd, WM_NULL, 0, 0);

    if (command == kCmdCopyResultRow)
        CopySelectedResultRow();
    else if (command == kCmdCopyAllResults)
        CopyAllResultRows();
}

void CBenchmarkPane::CopySelectedResultRow()
{
    const int row = m_results.GetNextItem(-1, LVNI_SELECTED);
    if (row < 0)
        return;
    SetClipboardText(m_hWnd, ResultRowText(m_results.m_hWnd, row));
}

void CBenchmarkPane::CopyAllResultRows()
{
    const int rows = m_results.GetItemCount();
    if (rows <= 0)
        return;
    SetClipboardText(m_hWnd, AllResultsText(m_results.m_hWnd, rows));
}

void CBenchmarkPane::AddResult(const BenchmarkResult& result)
{
    int row = m_results.GetItemCount();
    m_results.InsertItem(row, result.name.c_str());
    m_results.SetItemText(row, 1, FormatBytesShort(result.bytes).c_str());
    m_results.SetItemText(row, 2, FormatSeconds(result.seconds).c_str());
    m_results.SetItemText(row, 3, FormatRate(result.mbps).c_str());
    m_results.SetItemText(row, 4, FormatIops(result.iops).c_str());
    m_results.SetItemText(row, 5, FormatLatency(result.latencyMs).c_str());
    std::wstring status;
    if (result.timedOut)
        status = result.error.empty() ? Tr(L"超时") : result.error;
    else
        status = result.ok ? Tr(L"完成") : (result.error.empty() ? Tr(L"失败") : result.error);
    m_results.SetItemText(row, 6, status.c_str());
    m_results.EnsureVisible(row, FALSE);
}
