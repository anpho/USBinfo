#include "pch.h"
#include "DetailPane.h"
#include "Messages.h"

namespace
{
int ScaleForWindow(HWND window, int value)
{
    UINT dpi = window != nullptr ? ::GetDpiForWindow(window) : 96;
    if (dpi == 0)
        dpi = 96;
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

std::wstring NodeMeta(const UsbNode& node)
{
    std::wstring meta = NodeKindName(node.kind);
    if (!node.instanceId.empty())
        meta += L"  ·  " + node.instanceId;
    if (node.speedMbps > 0)
        meta += L"  ·  " + FormatSpeed(node.speedMbps);
    return meta;
}

std::wstring ListItemText(HWND list, int item, int column)
{
    wchar_t buffer[2048] = {};
    ListView_GetItemText(list, item, column, buffer, _countof(buffer));
    return buffer;
}

std::wstring JoinListColumns(HWND list, int item, int columnCount)
{
    std::vector<std::wstring> cells;
    cells.reserve(static_cast<size_t>(columnCount));
    for (int column = 0; column < columnCount; ++column)
        cells.push_back(ListItemText(list, item, column));
    while (!cells.empty() && cells.back().empty())
        cells.pop_back();
    std::wstring text;
    for (size_t i = 0; i < cells.size(); ++i)
    {
        if (i != 0)
            text += L'\t';
        text += cells[i];
    }
    return text;
}

bool CopyTextToClipboard(HWND owner, const std::wstring& text)
{
    if (text.empty())
        return false;
    if (!::OpenClipboard(owner))
        return false;
    ::EmptyClipboard();
    bool stored = false;
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory != nullptr)
    {
        void* target = ::GlobalLock(memory);
        if (target != nullptr)
        {
            ::CopyMemory(target, text.c_str(), bytes);
            ::GlobalUnlock(memory);
            stored = ::SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
        }
        if (!stored)
            ::GlobalFree(memory);
    }
    ::CloseClipboard();
    return stored;
}

std::wstring ExpandEnvironment(const std::wstring& value)
{
    if (value.empty())
        return value;
    DWORD needed = ::ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);
    if (needed == 0)
        return value;
    std::wstring expanded(static_cast<size_t>(needed), L'\0');
    DWORD written = ::ExpandEnvironmentStringsW(value.c_str(), expanded.data(), needed);
    if (written == 0 || written > needed)
        return value;
    expanded.resize(static_cast<size_t>(written - 1));
    return expanded;
}

bool IsAbsolutePath(const std::wstring& path)
{
    if (path.empty())
        return false;
    if (path[0] == L'\\' || path[0] == L'/')
        return true;
    return path.size() >= 2 && path[1] == L':';
}

// node.driverInf is often a bare file name such as "oem12.inf"; resolve it
// against %SystemRoot%\INF the same way the PnP properties are stored.
std::wstring InfFullPath(const UsbNode& node)
{
    std::wstring inf = ExpandEnvironment(node.driverInf);
    if (inf.empty())
        return std::wstring();
    if (IsAbsolutePath(inf))
        return inf;
    wchar_t windows[MAX_PATH] = {};
    UINT length = ::GetWindowsDirectoryW(windows, _countof(windows));
    if (length == 0 || length >= _countof(windows))
        return inf;
    return std::wstring(windows) + L"\\INF\\" + inf;
}

std::wstring DriverFilePath(const UsbNode& node)
{
    if (!node.driverFile.empty())
        return ExpandEnvironment(node.driverFile);
    return InfFullPath(node);
}

bool EndsWithNoCase(const std::wstring& value, const std::wstring& suffix)
{
    if (suffix.size() > value.size())
        return false;
    return _wcsicmp(value.c_str() + (value.size() - suffix.size()), suffix.c_str()) == 0;
}

std::wstring DirectoryName(const std::wstring& path)
{
    size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return std::wstring();
    if (slash == 0)
        return path.substr(0, 1);
    return path.substr(0, slash);
}

bool IsDriverRow(const InfoRow& row)
{
    if (row.group == L"驱动")
        return true;
    return ContainsNoCase(row.key, L"驱动") || ContainsNoCase(row.key, L"服务") ||
        ContainsNoCase(row.key, L"INF");
}

void ShowExplorerSelect(HWND owner, const std::wstring& path)
{
    if (path.empty())
        return;
    std::wstring arguments = L"/select,\"" + path + L"\"";
    ::ShellExecuteW(owner, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
}

void ShowExplorerDirectory(HWND owner, const std::wstring& directory)
{
    if (directory.empty())
        return;
    std::wstring arguments = L"\"" + directory + L"\"";
    ::ShellExecuteW(owner, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
}
}

int CDetailPane::OnCreate(LPCREATESTRUCT)
{
    m_font = AtlCreateControlFont();
    LOGFONT logFont = {};
    ::GetObject(m_font, sizeof(logFont), &logFont);
    logFont.lfHeight = -ScaleForWindow(m_hWnd, 18);
    logFont.lfWeight = FW_SEMIBOLD;
    wcscpy_s(logFont.lfFaceName, L"Segoe UI");
    m_titleFont = ::CreateFontIndirect(&logFont);

    m_title.Create(m_hWnd, rcDefault, L"选择一个设备", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, 0);
    m_title.SetFont(m_titleFont);
    m_meta.Create(m_hWnd, rcDefault, L"左侧树中显示 USB 级联关系", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, 0);
    m_meta.SetFont(m_font);

    m_warning.Create(m_hWnd, rcDefault, L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, 0);
    m_warning.SetFont(m_font);
    m_warning.ShowWindow(SW_HIDE);

    m_tabs.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_FOCUSNEVER, 0, IDC_TABS);
    m_tabs.SetFont(m_font);

    m_info.Create(m_hWnd, rcDefault, nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        WS_EX_CLIENTEDGE, IDC_INFO_LIST);
    m_info.SetFont(m_font);
    m_info.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    m_info.InsertColumn(0, L"属性", LVCFMT_LEFT, ScaleForWindow(m_hWnd, 250));
    m_info.InsertColumn(1, L"值", LVCFMT_LEFT, ScaleForWindow(m_hWnd, 560));
    ListView_EnableGroupView(m_info, TRUE);

    m_smart.Create(m_hWnd, rcDefault, nullptr,
        WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        WS_EX_CLIENTEDGE, IDC_SMART_LIST);
    m_smart.SetFont(m_font);
    m_smart.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    m_smart.InsertColumn(0, L"ID", LVCFMT_RIGHT, ScaleForWindow(m_hWnd, 50));
    m_smart.InsertColumn(1, L"属性", LVCFMT_LEFT, ScaleForWindow(m_hWnd, 230));
    m_smart.InsertColumn(2, L"当前", LVCFMT_RIGHT, ScaleForWindow(m_hWnd, 65));
    m_smart.InsertColumn(3, L"最差", LVCFMT_RIGHT, ScaleForWindow(m_hWnd, 65));
    m_smart.InsertColumn(4, L"阈值", LVCFMT_RIGHT, ScaleForWindow(m_hWnd, 65));
    m_smart.InsertColumn(5, L"原始值", LVCFMT_RIGHT, ScaleForWindow(m_hWnd, 150));
    m_smart.InsertColumn(6, L"状态", LVCFMT_LEFT, ScaleForWindow(m_hWnd, 90));

    m_benchPane.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, IDC_BENCH_PANE);
    m_benchPane.ShowWindow(SW_HIDE);

    RebuildTabs();
    LayoutChildren();
    return 0;
}

void CDetailPane::OnSize(UINT, CSize)
{
    LayoutChildren();
}

void CDetailPane::OnDestroy()
{
    if (m_titleFont != nullptr)
    {
        ::DeleteObject(m_titleFont);
        m_titleFont = nullptr;
    }
    if (m_font != nullptr)
    {
        ::DeleteObject(m_font);
        m_font = nullptr;
    }
}

LRESULT CDetailPane::OnEraseBkgnd(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    // White detail surface: no grey window-class background around the header,
    // the tab control or the list views.
    HDC dc = reinterpret_cast<HDC>(wParam);
    RECT client = {};
    ::GetClientRect(m_hWnd, &client);
    ::FillRect(dc, &client, ::GetSysColorBrush(COLOR_WINDOW));
    return 1;
}

LRESULT CDetailPane::OnCtlColorStatic(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    // Title / subtitle / warning labels share the white pane background.
    HDC dc = reinterpret_cast<HDC>(wParam);
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, ::GetSysColor(COLOR_WINDOWTEXT));
    return reinterpret_cast<LRESULT>(::GetSysColorBrush(COLOR_WINDOW));
}

void CDetailPane::LayoutChildren()
{
    if (!::IsWindow(m_hWnd))
        return;
    RECT client = {};
    ::GetClientRect(m_hWnd, &client);
    const int margin = ScaleForWindow(m_hWnd, 14);
    const int fullWidth = std::max(100, static_cast<int>(client.right) - margin * 2);

    const int titleY = ScaleForWindow(m_hWnd, 8);
    const int titleHeight = ScaleForWindow(m_hWnd, 32);
    const int metaY = titleY + titleHeight;
    const int metaHeight = ScaleForWindow(m_hWnd, 22);
    const int warningY = metaY + metaHeight + ScaleForWindow(m_hWnd, 8);

    m_title.MoveWindow(margin, titleY, fullWidth, titleHeight);
    m_meta.MoveWindow(margin, metaY, fullWidth, metaHeight);

    bool warningVisible = m_warning.IsWindowVisible() != FALSE;
    if (warningVisible)
        m_warning.MoveWindow(margin, warningY, fullWidth, ScaleForWindow(m_hWnd, 24));
    int tabsY = warningVisible ? warningY + ScaleForWindow(m_hWnd, 30)
                               : warningY;
    m_tabs.MoveWindow(margin, tabsY, fullWidth,
        std::max(10, static_cast<int>(client.bottom) - tabsY - margin));

    RECT content = {};
    m_tabs.GetClientRect(&content);
    m_tabs.AdjustRect(FALSE, &content);
    POINT offset = { 0, 0 };
    m_tabs.ClientToScreen(&offset);
    ::ScreenToClient(m_hWnd, &offset);
    ::OffsetRect(&content, offset.x, offset.y);

    m_info.MoveWindow(&content);
    m_smart.MoveWindow(&content);
    m_benchPane.MoveWindow(&content);

    // Size columns from the actual list client width (after the vertical
    // scrollbar is accounted for) so no horizontal scrollbar is ever needed.
    RECT infoClient = {};
    m_info.GetClientRect(&infoClient);
    const int infoWidth = std::max(120, static_cast<int>(infoClient.right));
    const int infoKeyWidth = std::min(ScaleForWindow(m_hWnd, 240), infoWidth / 3);
    m_info.SetColumnWidth(0, infoKeyWidth);
    m_info.SetColumnWidth(1, std::max(80, infoWidth - infoKeyWidth));

    RECT smartClient = {};
    m_smart.GetClientRect(&smartClient);
    const int smartWidth = std::max(120, static_cast<int>(smartClient.right));
    const int smartWeights[] = { 1, 5, 2, 2, 2, 4, 3 };
    const int smartTotal = 19;
    int assigned = 0;
    for (int column = 0; column < 7; ++column)
    {
        int columnWidth = column == 6
            ? std::max(40, smartWidth - assigned)
            : std::max(24, smartWidth * smartWeights[column] / smartTotal);
        if (assigned + columnWidth > smartWidth)
            columnWidth = std::max(24, smartWidth - assigned);
        m_smart.SetColumnWidth(column, columnWidth);
        assigned += columnWidth;
    }
    ::ShowScrollBar(m_info, SB_HORZ, FALSE);
    ::ShowScrollBar(m_smart, SB_HORZ, FALSE);
}

bool CDetailPane::PageAvailable(int pageId) const
{
    switch (pageId)
    {
    case PageInfo:
        return true;
    case PageSmart:
        return m_smartAvailable;
    case PageBench:
        return m_benchAvailable;
    default:
        return false;
    }
}

int CDetailPane::PageIdFromIndex(int index) const
{
    if (index < 0)
        return PageInfo;
    TCITEM item = {};
    item.mask = TCIF_PARAM;
    if (!m_tabs.GetItem(index, &item))
        return PageInfo;
    return static_cast<int>(item.lParam);
}

int CDetailPane::IndexOfPage(int pageId) const
{
    int count = m_tabs.GetItemCount();
    for (int index = 0; index < count; ++index)
    {
        if (PageIdFromIndex(index) == pageId)
            return index;
    }
    return -1;
}

void CDetailPane::ShowPage(int pageId)
{
    if (!PageAvailable(pageId))
        pageId = PageInfo;
    m_currentPage = pageId;

    int index = IndexOfPage(pageId);
    if (index >= 0 && m_tabs.GetCurSel() != index)
        m_tabs.SetCurSel(index);

    m_info.ShowWindow(pageId == PageInfo ? SW_SHOW : SW_HIDE);
    m_smart.ShowWindow(pageId == PageSmart ? SW_SHOW : SW_HIDE);
    if (pageId == PageBench)
        m_benchPane.ShowWindow(SW_SHOW);
    else
        m_benchPane.ShowWindow(SW_HIDE);

    // Raise the active page to the top of the sibling z-order and force a
    // show; otherwise switching tabs (especially the benchmark page) can leave
    // a blank client area because another child still covers it.
    HWND active = nullptr;
    if (pageId == PageInfo)
        active = m_info.m_hWnd;
    else if (pageId == PageSmart)
        active = m_smart.m_hWnd;
    else if (m_benchAvailable)
        active = m_benchPane.m_hWnd;
    if (active != nullptr)
    {
        ::SetWindowPos(active, HWND_TOP, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}

void CDetailPane::UpdateTabsAvailability(const UsbNode* storageNode)
{
    m_smartAvailable = false;
    m_benchAvailable = false;
    if (storageNode != nullptr)
    {
        const StorageInfo& storage = storageNode->storage;
        m_smartAvailable = storage.smartAvailable || !storage.smart.empty() || !storage.smartExtra.empty();
        if (storage.present)
        {
            for (const auto& volume : storage.volumes)
            {
                if (!volume.rootPath.empty() && !volume.readOnly && volume.totalBytes > 0)
                {
                    m_benchAvailable = true;
                    break;
                }
            }
        }
    }
}

void CDetailPane::RebuildTabs()
{
    int keepPage = m_currentPage;
    m_tabs.SetRedraw(FALSE);
    m_tabs.DeleteAllItems();
    int index = 0;
    m_tabs.InsertItem(index++, TCIF_TEXT | TCIF_PARAM, L"设备信息", 0, static_cast<LPARAM>(PageInfo));
    if (m_smartAvailable)
        m_tabs.InsertItem(index++, TCIF_TEXT | TCIF_PARAM, L"SMART / 健康", 0, static_cast<LPARAM>(PageSmart));
    if (m_benchAvailable)
        m_tabs.InsertItem(index++, TCIF_TEXT | TCIF_PARAM, L"测速", 0, static_cast<LPARAM>(PageBench));
    m_tabs.SetRedraw(TRUE);

    if (!PageAvailable(keepPage))
        keepPage = PageInfo;
    ShowPage(keepPage);
}

void CDetailPane::SetSnapshot(std::shared_ptr<const UsbSnapshot> snapshot)
{
    m_snapshot = std::move(snapshot);
    ShowNode(m_snapshot != nullptr && m_snapshot->computerId >= 0 ? m_snapshot->computerId : -1);
}

void CDetailPane::SetScanning()
{
    m_title.SetWindowText(L"正在扫描 USB 设备…");
    m_meta.SetWindowText(L"正在读取 HUB 端口、描述符、存储与 SMART 信息");
    m_infoRows.clear();
    m_info.DeleteAllItems();
    m_info.RemoveAllGroups();
    m_smart.DeleteAllItems();
    m_benchPane.ClearTarget();
    m_smartAvailable = false;
    m_benchAvailable = false;
    RebuildTabs();
    LayoutChildren();
}

void CDetailPane::ShowNode(int nodeId)
{
    m_nodeId = nodeId;
    const UsbNode* node = m_snapshot != nullptr ? m_snapshot->Find(nodeId) : nullptr;
    if (node == nullptr)
    {
        m_title.SetWindowText(L"未选择设备");
        m_meta.SetWindowText(L"在左侧树中选择一个节点");
        m_infoRows.clear();
        m_info.DeleteAllItems();
        m_info.RemoveAllGroups();
        m_smart.DeleteAllItems();
        m_benchPane.ClearTarget();
        m_smartAvailable = false;
        m_benchAvailable = false;
        RebuildTabs();
        UpdateWarning();
        LayoutChildren();
        return;
    }

    m_title.SetWindowText(node->title.c_str());
    std::wstring meta = NodeMeta(*node);
    m_meta.SetWindowText(meta.c_str());

    const UsbNode* storageNode = m_snapshot->StorageNodeFor(nodeId);
    if (storageNode != nullptr)
        m_benchPane.SetTarget(&storageNode->storage);
    else
        m_benchPane.ClearTarget();

    FillInfo(*node);
    FillSmart(*node, storageNode);
    UpdateTabsAvailability(storageNode);
    RebuildTabs();
    UpdateWarning();
    LayoutChildren();
}

void CDetailPane::UpdateWarning()
{
    if (m_snapshot == nullptr || m_snapshot->warnings.empty())
    {
        m_warning.ShowWindow(SW_HIDE);
        LayoutChildren();
        return;
    }

    std::wstring warning;
    for (size_t i = 0; i < m_snapshot->warnings.size() && i < 2; ++i)
    {
        if (!warning.empty())
            warning += L"  |  ";
        warning += m_snapshot->warnings[i];
    }
    if (m_snapshot->warnings.size() > 2)
        warning += L"  |  另有 " + std::to_wstring(m_snapshot->warnings.size() - 2) + L" 条提示，见导出报告。";
    m_warning.SetWindowText(warning.c_str());
    m_warning.ShowWindow(SW_SHOW);
    LayoutChildren();
}

void CDetailPane::FillInfo(const UsbNode& node)
{
    m_infoRows = BuildNodeRows(node);

    m_info.SetRedraw(FALSE);
    m_info.DeleteAllItems();
    m_info.RemoveAllGroups();

    std::map<std::wstring, int> groups;
    int nextGroup = 1;
    int item = 0;
    for (const auto& row : m_infoRows)
    {
        int groupId = 0;
        auto groupIt = groups.find(row.group);
        if (groupIt == groups.end())
        {
            groupId = nextGroup++;
            groups[row.group] = groupId;
            LVGROUP group = {};
            group.cbSize = sizeof(group);
            group.mask = LVGF_HEADER | LVGF_GROUPID | LVGF_STATE;
            group.state = LVGS_NORMAL;
            group.pszHeader = const_cast<LPWSTR>(row.group.c_str());
            group.iGroupId = groupId;
            m_info.InsertGroup(-1, &group);
        }
        else
        {
            groupId = groupIt->second;
        }

        LVITEM listItem = {};
        listItem.mask = LVIF_TEXT | LVIF_GROUPID;
        listItem.iItem = item;
        listItem.pszText = const_cast<LPWSTR>(row.key.c_str());
        listItem.iGroupId = groupId;
        int inserted = m_info.InsertItem(&listItem);
        if (inserted >= 0)
        {
            m_info.SetItemText(inserted, 1, row.value.c_str());
            ++item;
        }
    }

    m_info.SetRedraw(TRUE);
    m_info.Invalidate();
}

void CDetailPane::FillSmart(const UsbNode& node, const UsbNode* storageNode)
{
    m_smart.SetRedraw(FALSE);
    m_smart.DeleteAllItems();

    const StorageInfo& storage = storageNode != nullptr ? storageNode->storage : node.storage;
    if (!storage.present)
    {
        m_smart.InsertItem(0, L"—");
        m_smart.SetItemText(0, 1, L"仅 USB 存储设备提供 SMART / 健康数据");
        m_smart.SetItemText(0, 6, L"—");
        m_smart.SetRedraw(TRUE);
        return;
    }

    int row = 0;
    if (!storage.smartMessage.empty())
    {
        m_smart.InsertItem(row, L"说明");
        m_smart.SetItemText(row, 1, storage.smartMessage.c_str());
        m_smart.SetItemText(row, 6, storage.smartStatus.c_str());
        ++row;
    }

    for (const auto& extra : storage.smartExtra)
    {
        m_smart.InsertItem(row, L"—");
        m_smart.SetItemText(row, 1, extra.first.c_str());
        m_smart.SetItemText(row, 5, extra.second.c_str());
        m_smart.SetItemText(row, 6, L"信息");
        ++row;
    }

    for (const auto& attribute : storage.smart)
    {
        wchar_t id[16] = {};
        swprintf_s(id, L"%d", attribute.id);
        m_smart.InsertItem(row, id);
        m_smart.SetItemText(row, 1, attribute.name.c_str());
        m_smart.SetItemText(row, 2, std::to_wstring(attribute.current).c_str());
        m_smart.SetItemText(row, 3, std::to_wstring(attribute.worst).c_str());
        m_smart.SetItemText(row, 4, attribute.threshold >= 0 ? std::to_wstring(attribute.threshold).c_str() : L"—");
        m_smart.SetItemText(row, 5, attribute.rawText.c_str());
        m_smart.SetItemText(row, 6, attribute.status.c_str());
        ++row;
    }

    if (row == 0)
    {
        m_smart.InsertItem(0, L"—");
        m_smart.SetItemText(0, 1, L"没有可显示的 SMART 属性");
        m_smart.SetItemText(0, 6, storage.smartStatus.c_str());
    }
    m_smart.SetRedraw(TRUE);
    m_smart.Invalidate();
}

bool CDetailPane::CanCopySelected() const
{
    return m_snapshot != nullptr && m_snapshot->Find(m_nodeId) != nullptr;
}

void CDetailPane::CopyCurrentInfo()
{
    if (m_snapshot == nullptr)
        return;
    const UsbNode* node = m_snapshot->Find(m_nodeId);
    if (node == nullptr)
        return;
    CopyTextToClipboard(m_hWnd, BuildNodePlainText(*node));
}

void CDetailPane::OpenBenchmarkPage()
{
    if (!m_benchAvailable)
        return;
    ShowPage(PageBench);
    LayoutChildren();
    m_benchPane.FocusStart();
}

LRESULT CDetailPane::OnTabChanged(int, LPNMHDR, BOOL&)
{
    int index = m_tabs.GetCurSel();
    ShowPage(PageIdFromIndex(index));
    LayoutChildren();
    return 0;
}

LRESULT CDetailPane::OnInfoRightClick(int, LPNMHDR notifyHeader, BOOL&)
{
    auto* notify = reinterpret_cast<LPNMITEMACTIVATE>(notifyHeader);
    int index = notify != nullptr ? notify->iItem : -1;
    if (index < 0 || static_cast<size_t>(index) >= m_infoRows.size())
        return 0;
    m_info.SelectItem(index);
    ShowInfoContextMenu(index);
    return 0;
}

LRESULT CDetailPane::OnSmartRightClick(int, LPNMHDR notifyHeader, BOOL&)
{
    auto* notify = reinterpret_cast<LPNMITEMACTIVATE>(notifyHeader);
    int index = notify != nullptr ? notify->iItem : -1;
    if (index < 0 || index >= m_smart.GetItemCount())
        return 0;
    m_smart.SelectItem(index);
    ShowSmartContextMenu(index);
    return 0;
}

void CDetailPane::ShowInfoContextMenu(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_infoRows.size())
        return;
    const InfoRow& row = m_infoRows[static_cast<size_t>(index)];
    const UsbNode* node = m_snapshot != nullptr ? m_snapshot->Find(m_nodeId) : nullptr;

    // The popup menu pumps messages, so a background refresh can replace the
    // snapshot / rows while it is open. Resolve everything up front.
    const bool driverRow = node != nullptr && IsDriverRow(row);
    const std::wstring copyValue = row.value;
    const std::wstring copyRow = InfoRowText(index);
    const std::wstring copyAll = InfoAllText();
    std::wstring driverPath = node != nullptr ? DriverFilePath(*node) : std::wstring();
    const std::wstring infPath = node != nullptr ? InfFullPath(*node) : std::wstring();
    const bool driverPathIsInf = EndsWithNoCase(driverPath, L".inf") ||
        (node != nullptr && !node->driverFileKind.empty() && ContainsNoCase(node->driverFileKind, L"INF"));

    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenuW(MF_STRING, IDM_CONTEXT_COPY_VALUE, L"复制值");
    menu.AppendMenuW(MF_STRING, IDM_CONTEXT_COPY_ROW, L"复制整行");
    menu.AppendMenuW(MF_STRING, IDM_CONTEXT_COPY_ALL, L"复制全部");
    if (driverRow && (!driverPath.empty() || !infPath.empty()))
    {
        menu.AppendMenuW(MF_SEPARATOR);
        menu.AppendMenuW(MF_STRING, IDM_CONTEXT_LOCATE_DRIVER, L"定位到驱动文件");
        if (!infPath.empty())
            menu.AppendMenuW(MF_STRING, IDM_CONTEXT_OPEN_DRIVER_INF, L"打开 INF 所在目录");
    }

    POINT point = {};
    ::GetCursorPos(&point);
    int command = static_cast<int>(::TrackPopupMenu(menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,
        point.x, point.y, 0, m_hWnd, nullptr));
    switch (command)
    {
    case IDM_CONTEXT_COPY_VALUE:
        CopyTextToClipboard(m_hWnd, copyValue);
        break;
    case IDM_CONTEXT_COPY_ROW:
        CopyTextToClipboard(m_hWnd, copyRow);
        break;
    case IDM_CONTEXT_COPY_ALL:
        CopyTextToClipboard(m_hWnd, copyAll);
        break;
    case IDM_CONTEXT_LOCATE_DRIVER:
        if (!driverPath.empty())
        {
            if (driverPathIsInf)
            {
                std::wstring directory = DirectoryName(driverPath);
                ShowExplorerDirectory(m_hWnd, directory.empty() ? driverPath : directory);
            }
            else
            {
                ShowExplorerSelect(m_hWnd, driverPath);
            }
        }
        break;
    case IDM_CONTEXT_OPEN_DRIVER_INF:
        if (!infPath.empty())
        {
            std::wstring directory = DirectoryName(infPath);
            ShowExplorerDirectory(m_hWnd, directory.empty() ? infPath : directory);
        }
        break;
    default:
        break;
    }
}

void CDetailPane::ShowSmartContextMenu(int index)
{
    if (index < 0 || index >= m_smart.GetItemCount())
        return;
    std::wstring copyValue = ListItemText(m_smart.m_hWnd, index, 5);
    if (copyValue.empty())
        copyValue = ListItemText(m_smart.m_hWnd, index, 6);
    if (copyValue.empty())
        copyValue = ListItemText(m_smart.m_hWnd, index, 1);
    const std::wstring copyRow = SmartRowText(index);
    const std::wstring copyAll = SmartAllText();

    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenuW(MF_STRING, IDM_CONTEXT_COPY_VALUE, L"复制值");
    menu.AppendMenuW(MF_STRING, IDM_CONTEXT_COPY_ROW, L"复制整行");
    menu.AppendMenuW(MF_STRING, IDM_CONTEXT_COPY_ALL, L"复制全部");

    POINT point = {};
    ::GetCursorPos(&point);
    int command = static_cast<int>(::TrackPopupMenu(menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,
        point.x, point.y, 0, m_hWnd, nullptr));
    switch (command)
    {
    case IDM_CONTEXT_COPY_VALUE:
        CopyTextToClipboard(m_hWnd, copyValue);
        break;
    case IDM_CONTEXT_COPY_ROW:
        CopyTextToClipboard(m_hWnd, copyRow);
        break;
    case IDM_CONTEXT_COPY_ALL:
        CopyTextToClipboard(m_hWnd, copyAll);
        break;
    default:
        break;
    }
}

std::wstring CDetailPane::InfoRowText(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= m_infoRows.size())
        return std::wstring();
    const InfoRow& row = m_infoRows[static_cast<size_t>(index)];
    return row.key + L"\t" + row.value;
}

std::wstring CDetailPane::InfoAllText() const
{
    std::wstring text;
    std::wstring currentGroup;
    for (const auto& row : m_infoRows)
    {
        if (row.group != currentGroup)
        {
            if (!text.empty())
                text += L"\r\n";
            text += L"[" + row.group + L"]\r\n";
            currentGroup = row.group;
        }
        text += row.key + L"\t" + row.value + L"\r\n";
    }
    return text;
}

std::wstring CDetailPane::SmartRowText(int index) const
{
    if (index < 0 || index >= m_smart.GetItemCount())
        return std::wstring();
    return JoinListColumns(m_smart.m_hWnd, index, 7);
}

std::wstring CDetailPane::SmartAllText() const
{
    std::wstring text = L"ID\t属性\t当前\t最差\t阈值\t原始值\t状态\r\n";
    int count = m_smart.GetItemCount();
    for (int index = 0; index < count; ++index)
        text += JoinListColumns(m_smart.m_hWnd, index, 7) + L"\r\n";
    return text;
}
