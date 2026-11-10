#include "pch.h"
#include "MainFrame.h"
#include "Messages.h"
#include "Model.h"
#include "DeviceProps.h"
#include "Language.h"
#include "Version.h"

#include <commdlg.h>
#include <cstdio>

namespace
{
constexpr wchar_t kWebsiteUrl[] = L"https://github.com/anpho/USBinfo";

void OpenProjectHomepage()
{
    // Ask the already-running Explorer/shell process to open the URL. This is
    // more reliable than starting the URL association directly from a
    // standard-user process; Explorer runs in the interactive user session.
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC;
    info.lpFile = L"explorer.exe";
    info.lpParameters = kWebsiteUrl;
    info.nShow = SW_SHOWNORMAL;
    if (::ShellExecuteExW(&info))
        return;

    // Fallback: direct URL association.
    info = {};
    info.cbSize = sizeof(info);
    info.lpVerb = L"open";
    info.lpFile = kWebsiteUrl;
    info.nShow = SW_SHOWNORMAL;
    ::ShellExecuteExW(&info);
}

struct LanguageMenuEntry
{
    UINT commandId;
    AppLanguage language;
};

const LanguageMenuEntry kLanguageMenuEntries[] =
{
    { ID_VIEW_LANGUAGE_AUTO, AppLanguage::Auto },
    { ID_VIEW_LANGUAGE_ZH,   AppLanguage::ZhCN },
    { ID_VIEW_LANGUAGE_EN,   AppLanguage::EnUS },
    { ID_VIEW_LANGUAGE_FR,   AppLanguage::FrFR },
    { ID_VIEW_LANGUAGE_DE,   AppLanguage::DeDE },
    { ID_VIEW_LANGUAGE_ES,   AppLanguage::EsES },
    { ID_VIEW_LANGUAGE_JA,   AppLanguage::JaJP },
    { ID_VIEW_LANGUAGE_KO,   AppLanguage::KoKR },
    { ID_VIEW_LANGUAGE_RU,   AppLanguage::RuRU },
};

AppLanguage LanguageFromCommandId(int id)
{
    for (const auto& entry : kLanguageMenuEntries)
    {
        if (static_cast<int>(entry.commandId) == id)
            return entry.language;
    }
    return AppLanguage::Auto;
}

UINT CommandIdForLanguage(AppLanguage language)
{
    for (const auto& entry : kLanguageMenuEntries)
    {
        if (entry.language == language)
            return entry.commandId;
    }
    return ID_VIEW_LANGUAGE_AUTO;
}

int ScaleForWindow(HWND window, int value)
{
    UINT dpi = window != nullptr ? ::GetDpiForWindow(window) : 96;
    if (dpi == 0)
        dpi = 96;
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

std::wstring DefaultReportPath()
{
    wchar_t desktop[MAX_PATH] = {};
    if (SUCCEEDED(::SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktop)))
        return std::wstring(desktop) + L"\\USBInfo_Report.txt";
    wchar_t current[MAX_PATH] = {};
    ::GetCurrentDirectoryW(_countof(current), current);
    return std::wstring(current) + L"\\USBInfo_Report.txt";
}

// Build a double-NUL-terminated filter string for the common save dialog.
std::wstring BuildReportFilter()
{
    std::wstring filter;
    auto appendPair = [&filter](const wchar_t* label, const wchar_t* pattern)
    {
        filter.append(label);
        filter.push_back(L'\0');
        filter.append(pattern);
        filter.push_back(L'\0');
    };
    appendPair(Tr(L"文本报告 (*.txt)"), L"*.txt");
    appendPair(Tr(L"所有文件 (*.*)"), L"*.*");
    filter.push_back(L'\0');
    return filter;
}
}

CMainFrame::~CMainFrame()
{
    m_cancel = true;
    if (m_scanThread.joinable())
        m_scanThread.join();
}

int CMainFrame::OnCreate(LPCREATESTRUCT)
{
    BuildMenu();

    m_split.Create(m_hWnd, rcDefault, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    m_treePane.Create(m_split, rcDefault, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    m_detailPane.Create(m_split, rcDefault, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    m_split.SetSplitterPanes(m_treePane, m_detailPane);

    m_status.Create(m_hWnd, rcDefault, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP | WS_CLIPCHILDREN);

    int parts[] = { 500, 700, -1 };
    m_status.SetParts(3, parts);
    m_status.SetText(0, Tr(L"就绪"));
    m_status.SetText(1, IsProcessElevated() ? Tr(L"管理员") : Tr(L"标准用户"));
    m_status.SetText(2, L"");

    SetWindowText(Tr(L"USBInfo - USB 设备、HUB、存储与线缆信息"));
    LayoutChildren();
    UpdateMenuState();
    PostMessage(WM_APP_REQUEST_REFRESH, 0, 0);
    return 0;
}

void CMainFrame::BuildMenu()
{
    CMenu menu;
    menu.CreateMenu();

    CMenu fileMenu;
    fileMenu.CreatePopupMenu();
    fileMenu.AppendMenuW(MF_STRING, ID_FILE_REFRESH, Tr(L"刷新(&R)"));
    fileMenu.AppendMenuW(MF_STRING, ID_FILE_EXPORT, Tr(L"导出报告(&E)..."));
    fileMenu.AppendMenuW(MF_SEPARATOR);
    fileMenu.AppendMenuW(MF_STRING, ID_FILE_EXIT, Tr(L"退出(&X)"));
    menu.AppendMenuW(MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu.Detach()), Tr(L"文件(&F)"));

    CMenu actionMenu;
    actionMenu.CreatePopupMenu();
    actionMenu.AppendMenuW(MF_STRING, ID_ACTION_BENCH, Tr(L"测速(&T)..."));
    actionMenu.AppendMenuW(MF_STRING, ID_ACTION_COPY, Tr(L"复制当前节点信息(&C)"));
    actionMenu.AppendMenuW(MF_SEPARATOR);
    actionMenu.AppendMenuW(MF_STRING, ID_FILE_EXPORT, Tr(L"导出报告(&E)..."));
    menu.AppendMenuW(MF_POPUP, reinterpret_cast<UINT_PTR>(actionMenu.Detach()), Tr(L"操作(&O)"));

    CMenu viewMenu;
    viewMenu.CreatePopupMenu();
    viewMenu.AppendMenuW(MF_STRING, ID_VIEW_EXPAND_ALL, Tr(L"展开全部"));
    viewMenu.AppendMenuW(MF_STRING, ID_VIEW_COLLAPSE_ALL, Tr(L"折叠全部"));
    viewMenu.AppendMenuW(MF_SEPARATOR);
    viewMenu.AppendMenuW(MF_STRING, ID_VIEW_ELEVATE, Tr(L"以管理员身份重启"));
    viewMenu.AppendMenuW(MF_SEPARATOR);

    CMenu languageMenu;
    languageMenu.CreatePopupMenu();
    for (const auto& entry : kLanguageMenuEntries)
        languageMenu.AppendMenuW(MF_STRING, entry.commandId, LanguageName(entry.language));
    languageMenu.CheckMenuRadioItem(ID_VIEW_LANGUAGE_AUTO, ID_VIEW_LANGUAGE_RU,
        CommandIdForLanguage(LanguageGetConfigured()), MF_BYCOMMAND);
    viewMenu.AppendMenuW(MF_POPUP, reinterpret_cast<UINT_PTR>(languageMenu.Detach()), Tr(L"语言(&L)"));
    menu.AppendMenuW(MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu.Detach()), Tr(L"查看(&V)"));

    CMenu helpMenu;
    helpMenu.CreatePopupMenu();
    helpMenu.AppendMenuW(MF_STRING, ID_HELP_WEBSITE, Tr(L"项目主页(&W)"));
    helpMenu.AppendMenuW(MF_SEPARATOR);
    helpMenu.AppendMenuW(MF_STRING, ID_HELP_ABOUT, Tr(L"关于(&A)"));
    menu.AppendMenuW(MF_POPUP, reinterpret_cast<UINT_PTR>(helpMenu.Detach()), Tr(L"帮助(&H)"));

    HMENU previousMenu = GetMenu();
    SetMenu(menu.Detach());
    if (previousMenu != nullptr)
        ::DestroyMenu(previousMenu);
}

void CMainFrame::ApplyLanguage()
{
    // Rebuild the cached node titles/status summary in the new language before
    // refreshing the panes. No USB rescan is performed here.
    if (m_snapshot != nullptr)
        RefreshSnapshotText(*m_snapshot);

    BuildMenu();
    SetWindowText(Tr(L"USBInfo - USB 设备、HUB、存储与线缆信息"));
    m_treePane.RefreshLanguage();
    m_detailPane.RefreshLanguage();
    UpdateStatus();
    UpdateMenuState();
    Invalidate(TRUE);
}

void CMainFrame::UpdateMenuState()
{
    HMENU menu = GetMenu();
    if (menu == nullptr)
        return;

    const bool hasSnapshot = m_snapshot != nullptr && !m_scanning;
    const bool canCopy = hasSnapshot && m_detailPane.CanCopySelected();
    const bool canBench = hasSnapshot && m_detailPane.CanBenchmarkSelected();
    ::EnableMenuItem(menu, ID_ACTION_BENCH, MF_BYCOMMAND | (canBench ? MF_ENABLED : MF_GRAYED));
    ::EnableMenuItem(menu, ID_ACTION_COPY, MF_BYCOMMAND | (canCopy ? MF_ENABLED : MF_GRAYED));
    ::EnableMenuItem(menu, ID_FILE_EXPORT, MF_BYCOMMAND | (hasSnapshot ? MF_ENABLED : MF_GRAYED));
    ::DrawMenuBar(m_hWnd);
}

void CMainFrame::OnSize(UINT, CSize)
{
    LayoutChildren();
}

void CMainFrame::LayoutChildren()
{
    if (!::IsWindow(m_hWnd))
        return;
    RECT client = {};
    ::GetClientRect(m_hWnd, &client);

    if (::IsWindow(m_status))
    {
        m_status.SendMessage(WM_SIZE, 0, 0);
        RECT statusRect = {};
        m_status.GetWindowRect(&statusRect);
        int statusHeight = statusRect.bottom - statusRect.top;
        int splitHeight = std::max(0, static_cast<int>(client.bottom) - statusHeight);
        m_split.MoveWindow(0, 0, client.right, splitHeight);
        // Force the golden-ratio split on every layout: left tree 38.2%,
        // right content pane 61.8%. Setting the absolute splitter position
        // (instead of relying on one-shot proportional state) guarantees the
        // ratio after creation, resize and any stale splitter bookkeeping.
        if (client.right > 0)
        {
            const int splitterBar = 3;
            int left = ::MulDiv(client.right, 382, 1000) - splitterBar;
            m_split.SetSplitterPos(left);
        }

        int part0 = std::max(240, static_cast<int>(client.right) * 45 / 100);
        int part1 = std::max(420, static_cast<int>(client.right) * 72 / 100);
        int parts[] = { part0, part1, -1 };
        m_status.SetParts(3, parts);
    }
}

void CMainFrame::StartScan()
{
    if (m_scanning || m_closing)
        return;
    if (m_scanThread.joinable())
        m_scanThread.join();

    m_cancel = false;
    m_scanning = true;
    UpdateMenuState();
    m_treePane.SetScanning(true);
    m_detailPane.SetScanning();
    m_status.SetText(0, Tr(L"正在扫描 USB 拓扑…"));
    m_status.SetText(2, Tr(L"HUB: 读取中"));

    m_scanThread = std::thread([this]()
    {
        auto snapshot = UsbEnumerator::Scan(&m_cancel);
        auto* payload = new std::shared_ptr<UsbSnapshot>(std::move(snapshot));
        if (!::PostMessageW(m_hWnd, WM_APP_SCAN_DONE, 0, reinterpret_cast<LPARAM>(payload)))
            delete payload;
    });
}

void CMainFrame::UpdateStatus()
{
    if (!::IsWindow(m_status))
        return;
    if (m_scanning)
    {
        m_status.SetText(0, Tr(L"正在扫描 USB 拓扑…"));
        m_status.SetText(1, IsProcessElevated() ? Tr(L"管理员") : Tr(L"标准用户"));
        m_status.SetText(2, Tr(L"HUB: 读取中"));
        return;
    }
    if (m_snapshot != nullptr)
    {
        m_status.SetText(0, m_snapshot->summary.c_str());
        m_status.SetText(1, m_snapshot->elevated ? Tr(L"管理员") : Tr(L"标准用户（部分信息受限）"));
        std::wstring hub = m_snapshot->hubAccessOk ? Tr(L"HUB: 可读") : Tr(L"HUB: 受限");
        if (!m_snapshot->warnings.empty())
        {
            wchar_t buffer[64] = {};
            swprintf_s(buffer, Tr(L"%d 条提示"), static_cast<int>(m_snapshot->warnings.size()));
            hub += L" · ";
            hub += buffer;
        }
        m_status.SetText(2, hub.c_str());
    }
    else
    {
        m_status.SetText(0, Tr(L"就绪"));
        m_status.SetText(1, IsProcessElevated() ? Tr(L"管理员") : Tr(L"标准用户"));
        m_status.SetText(2, L"");
    }
}

void CMainFrame::SelectPreferredNode()
{
    if (m_snapshot == nullptr)
        return;
    int preferred = m_snapshot->computerId;
    for (const auto& node : m_snapshot->nodes)
    {
        if (node.kind == NodeKind::Storage)
        {
            preferred = node.id;
            break;
        }
    }
    if (preferred >= 0)
    {
        m_treePane.SelectNode(preferred);
        m_detailPane.ShowNode(preferred);
    }
}

LRESULT CMainFrame::OnScanDone(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    std::unique_ptr<std::shared_ptr<UsbSnapshot>> payload(
        reinterpret_cast<std::shared_ptr<UsbSnapshot>*>(lParam));
    if (m_scanThread.joinable())
        m_scanThread.join();
    m_scanning = false;

    if (payload && *payload)
    {
        m_snapshot = *payload;
        m_treePane.SetScanning(false);
        m_treePane.SetSnapshot(m_snapshot);
        m_detailPane.SetSnapshot(m_snapshot);
        SelectPreferredNode();
    }
    else
    {
        m_treePane.SetScanning(false);
        m_status.SetText(0, Tr(L"扫描失败"));
    }
    UpdateStatus();
    UpdateMenuState();
    return 0;
}

LRESULT CMainFrame::OnNodeSelected(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    m_detailPane.ShowNode(static_cast<int>(wParam));
    UpdateMenuState();
    return 0;
}

LRESULT CMainFrame::OnRequestRefresh(UINT, WPARAM, LPARAM, BOOL&)
{
    StartScan();
    return 0;
}

LRESULT CMainFrame::OnLanguageCommand(UINT, int id, HWND)
{
    LanguageSet(LanguageFromCommandId(id), true);
    ApplyLanguage();
    return 0;
}

LRESULT CMainFrame::OnHelpWebsite(WORD, WORD, HWND, BOOL&)
{
    OpenProjectHomepage();
    return 0;
}

LRESULT CMainFrame::OnFileRefresh(WORD, WORD, HWND, BOOL&)
{
    StartScan();
    return 0;
}

LRESULT CMainFrame::OnFileExport(WORD, WORD, HWND, BOOL&)
{
    if (m_snapshot == nullptr)
    {
        ::MessageBoxW(m_hWnd, Tr(L"还没有可导出的设备数据，请先刷新。"), L"USBInfo", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    std::wstring initial = DefaultReportPath();
    wchar_t fileName[MAX_PATH] = {};
    wcscpy_s(fileName, initial.c_str());
    std::wstring filter = BuildReportFilter();
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = m_hWnd;
    dialog.lpstrFilter = filter.c_str();
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = _countof(fileName);
    dialog.lpstrDefExt = L"txt";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!::GetSaveFileNameW(&dialog))
        return 0;

    std::wstring report = BuildFullReport(*m_snapshot);
    int utf8Size = ::WideCharToMultiByte(CP_UTF8, 0, report.c_str(), static_cast<int>(report.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<size_t>(std::max(0, utf8Size)), '\0');
    if (utf8Size > 0)
        ::WideCharToMultiByte(CP_UTF8, 0, report.c_str(), static_cast<int>(report.size()), utf8.data(), utf8Size, nullptr, nullptr);

    FILE* file = nullptr;
    if (_wfopen_s(&file, fileName, L"wb") != 0 || file == nullptr)
    {
        ::MessageBoxW(m_hWnd, Tr(L"无法写入报告文件。"), L"USBInfo", MB_OK | MB_ICONERROR);
        return 0;
    }
    const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    fwrite(bom, 1, sizeof(bom), file);
    if (!utf8.empty())
        fwrite(utf8.data(), 1, utf8.size(), file);
    fclose(file);

    std::wstring message = Tr(L"报告已导出：");
    message += L"\n";
    message += fileName;
    message += L"\n\n";
    message += Tr(L"是否现在打开？");
    if (::MessageBoxW(m_hWnd, message.c_str(), L"USBInfo", MB_YESNO | MB_ICONINFORMATION) == IDYES)
        ::ShellExecuteW(m_hWnd, L"open", fileName, nullptr, nullptr, SW_SHOWNORMAL);
    return 0;
}

LRESULT CMainFrame::OnFileExit(WORD, WORD, HWND, BOOL&)
{
    PostMessage(WM_CLOSE, 0, 0);
    return 0;
}

LRESULT CMainFrame::OnActionBench(WORD, WORD, HWND, BOOL&)
{
    m_detailPane.OpenBenchmarkPage();
    UpdateMenuState();
    return 0;
}

LRESULT CMainFrame::OnActionCopy(WORD, WORD, HWND, BOOL&)
{
    m_detailPane.CopyCurrentInfo();
    return 0;
}

LRESULT CMainFrame::OnExpandAll(WORD, WORD, HWND, BOOL&)
{
    m_treePane.ExpandAll();
    return 0;
}

LRESULT CMainFrame::OnCollapseAll(WORD, WORD, HWND, BOOL&)
{
    m_treePane.CollapseAll();
    return 0;
}

LRESULT CMainFrame::OnElevate(WORD, WORD, HWND, BOOL&)
{
    if (IsProcessElevated())
    {
        ::MessageBoxW(m_hWnd, Tr(L"当前已经以管理员身份运行。"), L"USBInfo", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    if (!RelaunchElevated())
    {
        ::MessageBoxW(m_hWnd,
            Tr(L"无法以管理员身份重启。可能取消了 UAC 提示，或系统策略禁止提升。\n"
               L"标准用户模式下仍可查看基本 USB 拓扑，但 HUB 描述符、物理磁盘 IOCTL 和 SMART 可能不可读。"),
            L"USBInfo", MB_OK | MB_ICONWARNING);
        return 0;
    }
    PostMessage(WM_CLOSE, 0, 0);
    return 0;
}

LRESULT CMainFrame::OnAbout(WORD, WORD, HWND, BOOL&)
{
    std::wstring message = std::wstring(L"USBInfo ") + USBINFO_VERSION_WSTRING + L"\n\n";
    message += Tr(L"基于 WTL / C++20 / Win32 的 USB 拓扑、HUB、存储与测速工具。");
    message += L"\n\n";
    message += Tr(L"• 左侧树展示 USB 主机控制器 → HUB → 设备 → 接口/存储/端口的级联关系");
    message += L"\n";
    message += Tr(L"• 读取 USB 描述符、Hub 芯片、驱动、速度、协议和 USB-C/复合端口信息");
    message += L"\n";
    message += Tr(L"• USB 存储容量、卷、桥接芯片、SMART（设备/驱动支持时）");
    message += L"\n";
    message += Tr(L"• 大/小数据顺序/随机读写测速（只写临时文件，不写裸盘）");
    message += L"\n\n";
    message += Tr(L"限制：标准 USB 协议不公开线缆 E-Marker、CC 线序、线材阻抗等信息；\n"
                   L"芯片型号根据 VID/PID 推断，NAND 闪存颗粒通常无法通过 USB 桥接器读取。");
    message += L"\n\n";
    message += Tr(L"作者：");
    message += L"anpho & Deepseek\n";
    message += Tr(L"网页：");
    message += L"https://github.com/anpho/USBinfo";
    message += L"\n\n";
    message += Tr(L"是否现在打开项目主页？");
    if (::MessageBoxW(m_hWnd, message.c_str(), Tr(L"关于 USBInfo"), MB_YESNO | MB_ICONINFORMATION) == IDYES)
        OpenProjectHomepage();
    return 0;
}

void CMainFrame::OnGetMinMaxInfo(LPMINMAXINFO info)
{
    info->ptMinTrackSize.x = ScaleForWindow(m_hWnd, 1000);
    info->ptMinTrackSize.y = ScaleForWindow(m_hWnd, 650);
}

void CMainFrame::OnClose()
{
    m_closing = true;
    m_cancel = true;
    if (m_scanThread.joinable())
        m_scanThread.join();
    DestroyWindow();
}

void CMainFrame::OnDestroy()
{
    m_closing = true;
    m_cancel = true;
    if (m_scanThread.joinable())
        m_scanThread.join();
    ::PostQuitMessage(0);
}
