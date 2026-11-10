#include "pch.h"
#include "TreePane.h"
#include "Messages.h"
#include "Language.h"

#include <uxtheme.h>

namespace
{
int ScaleForWindow(HWND window, int value)
{
    UINT dpi = 96;
    if (window != nullptr)
        dpi = ::GetDpiForWindow(window);
    if (dpi == 0)
        dpi = 96;
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

void AddNodeIcon(HIMAGELIST images, NodeKind kind)
{
    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = 16;
    bitmapInfo.bmiHeader.biHeight = -16;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = ::CreateDIBSection(nullptr, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap == nullptr)
        return;
    HDC dc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ oldBitmap = ::SelectObject(dc, bitmap);
    RECT area = { 0, 0, 16, 16 };
    HBRUSH background = ::CreateSolidBrush(RGB(255, 0, 255));
    ::FillRect(dc, &area, background);
    ::DeleteObject(background);
    ::SetBkMode(dc, TRANSPARENT);

    auto pen = [](COLORREF color, int width = 1)
    {
        return ::CreatePen(PS_SOLID, width, color);
    };
    auto brush = [](COLORREF color)
    {
        return ::CreateSolidBrush(color);
    };

    switch (kind)
    {
    case NodeKind::Computer:
    {
        HBRUSH frame = brush(RGB(52, 73, 94));
        HPEN framePen = pen(RGB(30, 42, 54));
        HGDIOBJ oldBrush = ::SelectObject(dc, frame);
        HGDIOBJ oldPen = ::SelectObject(dc, framePen);
        ::RoundRect(dc, 2, 2, 14, 11, 2, 2);
        ::SelectObject(dc, ::GetStockObject(WHITE_BRUSH));
        ::Rectangle(dc, 4, 4, 12, 9);
        ::SelectObject(dc, oldPen);
        ::SelectObject(dc, oldBrush);
        ::DeleteObject(frame);
        ::DeleteObject(framePen);
        HPEN standPen = pen(RGB(52, 73, 94), 2);
        oldPen = ::SelectObject(dc, standPen);
        ::MoveToEx(dc, 8, 11, nullptr);
        ::LineTo(dc, 8, 13);
        ::MoveToEx(dc, 5, 14, nullptr);
        ::LineTo(dc, 11, 14);
        ::SelectObject(dc, oldPen);
        ::DeleteObject(standPen);
        break;
    }
    case NodeKind::HostController:
    {
        HBRUSH body = brush(RGB(41, 128, 185));
        HPEN outline = pen(RGB(22, 82, 122));
        HGDIOBJ oldBrush = ::SelectObject(dc, body);
        HGDIOBJ oldPen = ::SelectObject(dc, outline);
        ::RoundRect(dc, 3, 3, 13, 12, 2, 2);
        ::SelectObject(dc, outline);
        for (int y : { 5, 8, 11 })
        {
            ::MoveToEx(dc, 0, y, nullptr);
            ::LineTo(dc, 3, y);
            ::MoveToEx(dc, 13, y, nullptr);
            ::LineTo(dc, 16, y);
        }
        ::SelectObject(dc, oldPen);
        ::SelectObject(dc, oldBrush);
        ::DeleteObject(body);
        ::DeleteObject(outline);
        break;
    }
    case NodeKind::RootHub:
    case NodeKind::Hub:
    {
        HBRUSH body = brush(RGB(26, 188, 156));
        HPEN outline = pen(RGB(15, 112, 94));
        HGDIOBJ oldBrush = ::SelectObject(dc, body);
        HGDIOBJ oldPen = ::SelectObject(dc, outline);
        ::Ellipse(dc, 3, 2, 13, 12);
        ::SelectObject(dc, ::GetStockObject(WHITE_BRUSH));
        for (int x : { 5, 8, 11 })
            ::Rectangle(dc, x, 11, x + 2, 15);
        ::SelectObject(dc, oldPen);
        ::SelectObject(dc, oldBrush);
        ::DeleteObject(body);
        ::DeleteObject(outline);
        break;
    }
    case NodeKind::Interface:
    {
        HBRUSH body = brush(RGB(149, 165, 166));
        HPEN outline = pen(RGB(90, 105, 106));
        HGDIOBJ oldBrush = ::SelectObject(dc, body);
        HGDIOBJ oldPen = ::SelectObject(dc, outline);
        ::Rectangle(dc, 3, 4, 13, 11);
        ::SelectObject(dc, ::GetStockObject(WHITE_BRUSH));
        ::Rectangle(dc, 6, 6, 10, 9);
        ::SelectObject(dc, oldPen);
        ::SelectObject(dc, oldBrush);
        ::DeleteObject(body);
        ::DeleteObject(outline);
        break;
    }
    case NodeKind::Storage:
    {
        HBRUSH body = brush(RGB(230, 126, 34));
        HPEN outline = pen(RGB(155, 78, 12));
        HGDIOBJ oldBrush = ::SelectObject(dc, body);
        HGDIOBJ oldPen = ::SelectObject(dc, outline);
        ::RoundRect(dc, 2, 4, 14, 12, 2, 2);
        ::SelectObject(dc, ::GetStockObject(WHITE_BRUSH));
        ::Rectangle(dc, 4, 6, 12, 8);
        ::SelectObject(dc, oldPen);
        ::SelectObject(dc, oldBrush);
        ::DeleteObject(body);
        ::DeleteObject(outline);
        break;
    }
    case NodeKind::Cable:
    {
        HPEN cable = pen(RGB(142, 68, 173), 2);
        HGDIOBJ oldPen = ::SelectObject(dc, cable);
        ::MoveToEx(dc, 2, 13, nullptr);
        ::LineTo(dc, 6, 3);
        ::LineTo(dc, 10, 13);
        ::LineTo(dc, 14, 3);
        ::SelectObject(dc, oldPen);
        ::DeleteObject(cable);
        HBRUSH plug = brush(RGB(142, 68, 173));
        HGDIOBJ oldBrush = ::SelectObject(dc, plug);
        ::Ellipse(dc, 0, 11, 5, 16);
        ::Ellipse(dc, 11, 11, 16, 16);
        ::SelectObject(dc, oldBrush);
        ::DeleteObject(plug);
        break;
    }
    case NodeKind::Usb4:
    {
        HBRUSH body = brush(RGB(155, 89, 182));
        HPEN outline = pen(RGB(93, 45, 112));
        HGDIOBJ oldBrush = ::SelectObject(dc, body);
        HGDIOBJ oldPen = ::SelectObject(dc, outline);
        POINT diamond[] = { { 8, 1 }, { 15, 8 }, { 8, 15 }, { 1, 8 } };
        ::Polygon(dc, diamond, 4);
        ::SelectObject(dc, oldPen);
        ::SelectObject(dc, oldBrush);
        ::DeleteObject(body);
        ::DeleteObject(outline);
        break;
    }
    case NodeKind::Device:
    default:
    {
        HBRUSH body = brush(RGB(46, 204, 113));
        HPEN outline = pen(RGB(27, 128, 68));
        HGDIOBJ oldBrush = ::SelectObject(dc, body);
        HGDIOBJ oldPen = ::SelectObject(dc, outline);
        ::RoundRect(dc, 4, 5, 12, 12, 2, 2);
        ::SelectObject(dc, outline);
        ::MoveToEx(dc, 8, 5, nullptr);
        ::LineTo(dc, 8, 2);
        ::MoveToEx(dc, 6, 2, nullptr);
        ::LineTo(dc, 10, 2);
        ::SelectObject(dc, oldPen);
        ::SelectObject(dc, oldBrush);
        ::DeleteObject(body);
        ::DeleteObject(outline);
        break;
    }
    }

    ::SelectObject(dc, oldBitmap);
    ::DeleteDC(dc);
    ::ImageList_AddMasked(images, bitmap, RGB(255, 0, 255));
    ::DeleteObject(bitmap);
}

int ImageIndexForNode(NodeKind kind)
{
    switch (kind)
    {
    case NodeKind::Computer:       return 0;
    case NodeKind::HostController: return 1;
    case NodeKind::RootHub:
    case NodeKind::Hub:            return 2;
    case NodeKind::Interface:      return 3;
    case NodeKind::Storage:        return 4;
    case NodeKind::Cable:          return 5;
    case NodeKind::Usb4:           return 6;
    case NodeKind::Device:
    default:                       return 7;
    }
}
}

int CTreePane::OnCreate(LPCREATESTRUCT)
{
    m_font = AtlCreateControlFont();
    m_boldFont = AtlCreateBoldFont(m_font);

    m_header.Create(m_hWnd, rcDefault, Tr(L"USB 拓扑"), WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, 0);
    m_header.SetFont(m_boldFont);

    m_refresh.Create(m_hWnd, rcDefault, Tr(L"刷新"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, ID_FILE_REFRESH);
    m_refresh.SetFont(m_font);

    m_tree.Create(m_hWnd, rcDefault, nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT |
        TVS_SHOWSELALWAYS | TVS_FULLROWSELECT | TVS_DISABLEDRAGDROP,
        WS_EX_CLIENTEDGE, IDC_TREE_USB);
    m_tree.SetFont(m_font);
    ::SetWindowTheme(m_tree, L"Explorer", nullptr);
    TreeView_SetExtendedStyle(m_tree, TVS_EX_DOUBLEBUFFER | TVS_EX_NOSINGLECOLLAPSE,
        TVS_EX_DOUBLEBUFFER | TVS_EX_NOSINGLECOLLAPSE);
    TreeView_SetItemHeight(m_tree, ScaleForWindow(m_hWnd, 23));
    TreeView_SetUnicodeFormat(m_tree, TRUE);

    CreateImageList();
    if (m_images != nullptr)
        m_tree.SetImageList(m_images, TVSIL_NORMAL);

    LayoutChildren();
    return 0;
}

void CTreePane::OnSize(UINT, CSize)
{
    LayoutChildren();
}

LRESULT CTreePane::OnEraseBkgnd(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    // Paint the pane background white so no grey window-class brush shows
    // through around the header and the tree control.
    HDC dc = reinterpret_cast<HDC>(wParam);
    RECT client = {};
    ::GetClientRect(m_hWnd, &client);
    ::FillRect(dc, &client, ::GetSysColorBrush(COLOR_WINDOW));
    return 1;
}

LRESULT CTreePane::OnCtlColorStatic(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    // The "USB 拓扑" header (and any other label) sits on a white surface.
    HDC dc = reinterpret_cast<HDC>(wParam);
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, ::GetSysColor(COLOR_WINDOWTEXT));
    return reinterpret_cast<LRESULT>(::GetSysColorBrush(COLOR_WINDOW));
}

void CTreePane::OnDestroy()
{
    if (m_images != nullptr)
    {
        ::ImageList_Destroy(m_images);
        m_images = nullptr;
    }
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

void CTreePane::LayoutChildren()
{
    if (!::IsWindow(m_hWnd))
        return;
    RECT client = {};
    ::GetClientRect(m_hWnd, &client);
    const int headerHeight = ScaleForWindow(m_hWnd, 38);
    const int margin = ScaleForWindow(m_hWnd, 8);
    // Wider than the original 72 px so translated button labels such as
    // "Aktualisieren" / "Actualizar" are not clipped.
    const int buttonWidth = ScaleForWindow(m_hWnd, 100);
    const int buttonHeight = ScaleForWindow(m_hWnd, 26);

    m_header.MoveWindow(margin, 0, std::max(10, static_cast<int>(client.right) - buttonWidth - margin * 2), headerHeight);
    m_refresh.MoveWindow(std::max(0, static_cast<int>(client.right) - buttonWidth - margin),
        (headerHeight - buttonHeight) / 2, buttonWidth, buttonHeight);
    m_tree.MoveWindow(0, headerHeight, client.right, std::max(0, static_cast<int>(client.bottom) - headerHeight));
}

void CTreePane::CreateImageList()
{
    m_images = ::ImageList_Create(16, 16, ILC_COLOR24 | ILC_MASK, 0, 9);
    if (m_images == nullptr)
        return;

    AddNodeIcon(m_images, NodeKind::Computer);
    AddNodeIcon(m_images, NodeKind::HostController);
    AddNodeIcon(m_images, NodeKind::Hub);
    AddNodeIcon(m_images, NodeKind::Interface);
    AddNodeIcon(m_images, NodeKind::Storage);
    AddNodeIcon(m_images, NodeKind::Cable);
    AddNodeIcon(m_images, NodeKind::Usb4);
    AddNodeIcon(m_images, NodeKind::Device);
}

void CTreePane::SetSnapshot(std::shared_ptr<const UsbSnapshot> snapshot)
{
    m_snapshot = std::move(snapshot);
    m_selectedId = -1;
    RebuildTree();

    if (m_snapshot != nullptr && m_snapshot->computerId >= 0)
        SelectNode(m_snapshot->computerId);
    else
        m_header.SetWindowText(Tr(L"未发现 USB 设备"));
}

void CTreePane::SetScanning(bool scanning)
{
    m_scanning = scanning;
    if (scanning)
    {
        m_header.SetWindowText(Tr(L"正在扫描 USB 拓扑…"));
        return;
    }
    if (m_snapshot != nullptr)
    {
        m_header.SetWindowText(Tr(L"USB 拓扑"));
    }
    else
    {
        m_header.SetWindowText(Tr(L"未发现 USB 设备"));
    }
}

void CTreePane::RefreshLanguage()
{
    if (!::IsWindow(m_hWnd))
        return;

    if (::IsWindow(m_refresh))
        m_refresh.SetWindowText(Tr(L"刷新"));

    if (m_scanning)
    {
        m_header.SetWindowText(Tr(L"正在扫描 USB 拓扑…"));
        return;
    }
    if (m_snapshot == nullptr)
    {
        m_header.SetWindowText(Tr(L"未发现 USB 设备"));
        return;
    }

    // Re-translate the existing items in place. Rebuilding the whole tree here
    // would destroy and recreate every HTREEITEM and can upset the common
    // control while a selection change is in flight.
    if (m_snapshot == nullptr)
        return;
    for (const auto& entry : m_items)
    {
        const UsbNode* node = m_snapshot->Find(entry.first);
        if (node == nullptr)
            continue;
        std::wstring title = node->kind == NodeKind::Computer ? Tr(L"本机 USB 拓扑") : BuildNodeTitle(*node);
        m_tree.SetItemText(entry.second, title.c_str());
    }

    if (m_tree.GetSelectedItem() == nullptr && m_snapshot->computerId >= 0)
        SelectNode(m_snapshot->computerId);

    m_header.SetWindowText(Tr(L"USB 拓扑"));
    m_tree.Invalidate();
}

void CTreePane::RebuildTree()
{
    if (!::IsWindow(m_tree))
        return;
    m_tree.SetRedraw(FALSE);
    m_tree.DeleteAllItems();
    m_items.clear();

    if (m_snapshot != nullptr && m_snapshot->computerId >= 0)
    {
        std::set<int> visited;
        HTREEITEM root = AddNodeItem(TVI_ROOT, m_snapshot->computerId, visited);
        if (root != nullptr)
        {
            m_tree.Expand(root);
            m_tree.SelectItem(root);
        }
    }

    m_tree.SetRedraw(TRUE);
    m_tree.Invalidate();

    if (m_snapshot != nullptr)
    {
        m_header.SetWindowText(Tr(L"USB 拓扑"));
    }
}

HTREEITEM CTreePane::AddNodeItem(HTREEITEM parent, int nodeId, std::set<int>& visited)
{
    if (m_snapshot == nullptr || visited.count(nodeId) != 0)
        return nullptr;
    const UsbNode* node = m_snapshot->Find(nodeId);
    if (node == nullptr)
        return nullptr;
    visited.insert(nodeId);

    int image = ImageIndexForNode(node->kind);
    HTREEITEM item = m_tree.InsertItem(node->title.c_str(), image, image, parent, TVI_LAST);
    if (item == nullptr)
        return nullptr;
    m_tree.SetItemData(item, static_cast<DWORD_PTR>(nodeId));
    m_items[nodeId] = item;

    bool expand = node->kind == NodeKind::Computer || node->kind == NodeKind::HostController ||
        node->kind == NodeKind::RootHub || node->kind == NodeKind::Hub;
    if (expand)
        m_tree.Expand(item);

    for (const auto& child : m_snapshot->nodes)
    {
        if (child.parent == nodeId)
            AddNodeItem(item, child.id, visited);
    }
    return item;
}

void CTreePane::SelectNode(int nodeId)
{
    m_selectedId = nodeId;
    auto it = m_items.find(nodeId);
    if (it == m_items.end())
        return;
    m_tree.SelectItem(it->second);
    m_tree.EnsureVisible(it->second);
    ::PostMessage(::GetAncestor(m_hWnd, GA_ROOT), WM_APP_NODE_SELECTED, static_cast<WPARAM>(nodeId), 0);
}

int CTreePane::SelectedNodeId() const
{
    if (!::IsWindow(m_tree))
        return m_selectedId;
    HTREEITEM item = m_tree.GetSelectedItem();
    if (item == nullptr)
        return m_selectedId;
    return static_cast<int>(m_tree.GetItemData(item));
}

void CTreePane::ExpandAll()
{
    std::function<void(HTREEITEM)> expand = [&](HTREEITEM item)
    {
        while (item != nullptr)
        {
            m_tree.Expand(item, TVE_EXPAND);
            HTREEITEM child = m_tree.GetChildItem(item);
            if (child != nullptr)
                expand(child);
            item = m_tree.GetNextSiblingItem(item);
        }
    };
    expand(m_tree.GetRootItem());
}

void CTreePane::CollapseAll()
{
    HTREEITEM root = m_tree.GetRootItem();
    if (root == nullptr)
        return;
    std::function<void(HTREEITEM, bool)> collapse = [&](HTREEITEM item, bool isRoot)
    {
        while (item != nullptr)
        {
            HTREEITEM child = m_tree.GetChildItem(item);
            if (child != nullptr)
                collapse(child, false);
            if (!isRoot)
                m_tree.Expand(item, TVE_COLLAPSE);
            item = m_tree.GetNextSiblingItem(item);
        }
    };
    collapse(root, true);
    m_tree.Expand(root, TVE_EXPAND);
}

LRESULT CTreePane::OnRefresh(WORD, WORD, HWND, BOOL&)
{
    ::PostMessage(::GetAncestor(m_hWnd, GA_ROOT), WM_APP_REQUEST_REFRESH, 0, 0);
    return 0;
}

LRESULT CTreePane::OnTreeSelChanged(int, LPNMHDR notifyHeader, BOOL&)
{
    auto* notify = reinterpret_cast<LPNMTREEVIEW>(notifyHeader);
    int nodeId = static_cast<int>(notify->itemNew.lParam);
    if (nodeId >= 0)
    {
        m_selectedId = nodeId;
        ::PostMessage(::GetAncestor(m_hWnd, GA_ROOT), WM_APP_NODE_SELECTED, static_cast<WPARAM>(nodeId), 0);
    }
    return 0;
}
