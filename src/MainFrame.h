#pragma once

#include "pch.h"
#include "Messages.h"
#include "TreePane.h"
#include "DetailPane.h"
#include "UsbEnumerator.h"

class CMainFrame : public CFrameWindowImpl<CMainFrame>
{
public:
    DECLARE_FRAME_WND_CLASS(L"USBInfoMainFrame", 0)

    BEGIN_MSG_MAP(CMainFrame)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_SIZE(OnSize)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_CLOSE(OnClose)
        MSG_WM_GETMINMAXINFO(OnGetMinMaxInfo)
        COMMAND_ID_HANDLER(ID_FILE_REFRESH, OnFileRefresh)
        COMMAND_ID_HANDLER(ID_FILE_EXPORT, OnFileExport)
        COMMAND_ID_HANDLER(ID_FILE_EXIT, OnFileExit)
        COMMAND_ID_HANDLER(ID_ACTION_BENCH, OnActionBench)
        COMMAND_ID_HANDLER(ID_ACTION_COPY, OnActionCopy)
        COMMAND_ID_HANDLER(ID_VIEW_EXPAND_ALL, OnExpandAll)
        COMMAND_ID_HANDLER(ID_VIEW_COLLAPSE_ALL, OnCollapseAll)
        COMMAND_ID_HANDLER(ID_VIEW_ELEVATE, OnElevate)
        COMMAND_ID_HANDLER(ID_HELP_ABOUT, OnAbout)
        MESSAGE_HANDLER(WM_APP_SCAN_DONE, OnScanDone)
        MESSAGE_HANDLER(WM_APP_NODE_SELECTED, OnNodeSelected)
        MESSAGE_HANDLER(WM_APP_REQUEST_REFRESH, OnRequestRefresh)
        CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
    END_MSG_MAP()

    ~CMainFrame();

private:
    int OnCreate(LPCREATESTRUCT createStruct);
    void OnSize(UINT type, CSize size);
    void OnDestroy();
    void OnClose();
    void OnGetMinMaxInfo(LPMINMAXINFO info);
    LRESULT OnFileRefresh(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnFileExport(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnFileExit(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnActionBench(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnActionCopy(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnExpandAll(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnCollapseAll(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnElevate(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnAbout(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnScanDone(UINT message, WPARAM wParam, LPARAM lParam, BOOL& handled);
    LRESULT OnNodeSelected(UINT message, WPARAM wParam, LPARAM lParam, BOOL& handled);
    LRESULT OnRequestRefresh(UINT message, WPARAM wParam, LPARAM lParam, BOOL& handled);

    void BuildMenu();
    void UpdateMenuState();
    void LayoutChildren();
    void StartScan();
    void UpdateStatus();
    void SelectPreferredNode();

    CSplitterWindow m_split;
    CTreePane m_treePane;
    CDetailPane m_detailPane;
    CStatusBarCtrl m_status;

    std::shared_ptr<const UsbSnapshot> m_snapshot;
    std::thread m_scanThread;
    std::atomic_bool m_cancel = false;
    bool m_scanning = false;
    bool m_closing = false;
};

