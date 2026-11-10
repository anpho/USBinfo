#pragma once

#include "pch.h"
#include "Model.h"
#include "Messages.h"
#include "BenchmarkPane.h"

class CDetailPane : public CWindowImpl<CDetailPane>
{
public:
    DECLARE_WND_CLASS(L"USBInfoDetailPane")

    BEGIN_MSG_MAP(CDetailPane)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_SIZE(OnSize)
        MSG_WM_DESTROY(OnDestroy)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColorStatic)
        COMMAND_ID_HANDLER(IDC_BTN_COPY, OnCopy)
        COMMAND_ID_HANDLER(IDC_BTN_BENCH, OnBench)
        COMMAND_ID_HANDLER(IDC_BTN_EXPORT, OnExport)
        NOTIFY_HANDLER(IDC_TABS, TCN_SELCHANGE, OnTabChanged)
        NOTIFY_HANDLER(IDC_INFO_LIST, NM_RCLICK, OnInfoRightClick)
        NOTIFY_HANDLER(IDC_SMART_LIST, NM_RCLICK, OnSmartRightClick)
    END_MSG_MAP()

    void SetSnapshot(std::shared_ptr<const UsbSnapshot> snapshot);
    void ShowNode(int nodeId);
    int SelectedNodeId() const { return m_nodeId; }
    void SetScanning();

private:
    // Page identifiers stored in TCITEM.lParam. Keep in sync with ShowPage().
    enum PageId
    {
        PageInfo = 0,
        PageSmart = 1,
        PageBench = 2
    };

    int OnCreate(LPCREATESTRUCT createStruct);
    void OnSize(UINT type, CSize size);
    void OnDestroy();
    LRESULT OnEraseBkgnd(UINT message, WPARAM wParam, LPARAM lParam, BOOL& handled);
    LRESULT OnCtlColorStatic(UINT message, WPARAM wParam, LPARAM lParam, BOOL& handled);
    LRESULT OnCopy(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnBench(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnExport(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnTabChanged(int idCtrl, LPNMHDR notifyHeader, BOOL& handled);
    LRESULT OnInfoRightClick(int idCtrl, LPNMHDR notifyHeader, BOOL& handled);
    LRESULT OnSmartRightClick(int idCtrl, LPNMHDR notifyHeader, BOOL& handled);

    void LayoutChildren();
    void RebuildTabs();
    void ShowPage(int pageId);
    bool PageAvailable(int pageId) const;
    int IndexOfPage(int pageId) const;
    int PageIdFromIndex(int index) const;
    void UpdateTabsAvailability(const UsbNode* storageNode);
    void FillInfo(const UsbNode& node);
    void FillSmart(const UsbNode& node, const UsbNode* storageNode);
    void UpdateWarning();

    void ShowInfoContextMenu(int index);
    void ShowSmartContextMenu(int index);
    std::wstring InfoRowText(int index) const;
    std::wstring InfoAllText() const;
    std::wstring SmartRowText(int index) const;
    std::wstring SmartAllText() const;

    CStatic m_title;
    CStatic m_meta;
    CStatic m_warning;
    CButton m_copy;
    CButton m_bench;
    CButton m_export;
    CTabCtrl m_tabs;
    CListViewCtrl m_info;
    CListViewCtrl m_smart;
    CBenchmarkPane m_benchPane;

    std::shared_ptr<const UsbSnapshot> m_snapshot;
    int m_nodeId = -1;
    HFONT m_font = nullptr;
    HFONT m_boldFont = nullptr;
    HFONT m_titleFont = nullptr;
    int m_currentPage = PageInfo;
    bool m_smartAvailable = false;
    bool m_benchAvailable = false;
    std::vector<InfoRow> m_infoRows;   // item index == row index (no header rows)
};
