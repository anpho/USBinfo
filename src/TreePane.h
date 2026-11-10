#pragma once

#include "pch.h"
#include "Model.h"
#include "Messages.h"

class CTreePane : public CWindowImpl<CTreePane>
{
public:
    DECLARE_WND_CLASS(L"USBInfoTreePane")

    BEGIN_MSG_MAP(CTreePane)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_SIZE(OnSize)
        MSG_WM_DESTROY(OnDestroy)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColorStatic)
        COMMAND_ID_HANDLER(ID_FILE_REFRESH, OnRefresh)
        NOTIFY_HANDLER(IDC_TREE_USB, TVN_SELCHANGED, OnTreeSelChanged)
    END_MSG_MAP()

    void SetSnapshot(std::shared_ptr<const UsbSnapshot> snapshot);
    void SelectNode(int nodeId);
    int SelectedNodeId() const;
    void ExpandAll();
    void CollapseAll();
    void SetScanning(bool scanning);
    // Re-applies translated text (header, refresh button, header state) and
    // rebuilds the tree while keeping the currently selected node.
    void RefreshLanguage();

private:
    int OnCreate(LPCREATESTRUCT createStruct);
    void OnSize(UINT type, CSize size);
    void OnDestroy();
    LRESULT OnEraseBkgnd(UINT message, WPARAM wParam, LPARAM lParam, BOOL& handled);
    LRESULT OnCtlColorStatic(UINT message, WPARAM wParam, LPARAM lParam, BOOL& handled);
    LRESULT OnRefresh(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnTreeSelChanged(int idCtrl, LPNMHDR notifyHeader, BOOL& handled);

    HTREEITEM AddNodeItem(HTREEITEM parent, int nodeId, std::set<int>& visited);
    void RebuildTree();
    void LayoutChildren();
    void CreateImageList();
    HICON CreateNodeIcon(NodeKind kind);

    CTreeViewCtrl m_tree;
    CStatic m_header;
    CButton m_refresh;
    HIMAGELIST m_images = nullptr;
    HFONT m_font = nullptr;
    HFONT m_boldFont = nullptr;
    std::shared_ptr<const UsbSnapshot> m_snapshot;
    std::map<int, HTREEITEM> m_items;
    int m_selectedId = -1;
    bool m_scanning = false;
};

