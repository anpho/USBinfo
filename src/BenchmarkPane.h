#pragma once

#include "pch.h"
#include "Model.h"
#include "Messages.h"
#include "Benchmark.h"
#include "ThroughputChart.h"

class CBenchmarkPane : public CWindowImpl<CBenchmarkPane>
{
public:
    DECLARE_WND_CLASS(L"USBInfoBenchmarkPane")

    BEGIN_MSG_MAP(CBenchmarkPane)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_SIZE(OnSize)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_ERASEBKGND(OnEraseBackground)
        MSG_WM_CTLCOLORSTATIC(OnCtlColorStatic)
        COMMAND_ID_HANDLER(IDC_BENCH_START, OnStart)
        COMMAND_ID_HANDLER(IDC_BENCH_CANCEL, OnCancel)
        MESSAGE_HANDLER(WM_APP_BENCH_UPDATE, OnBenchUpdate)
        NOTIFY_HANDLER(IDC_BENCH_RESULTS, NM_RCLICK, OnResultsRightClick)
    END_MSG_MAP()

    void SetTarget(const StorageInfo* storage);
    void ClearTarget();
    void FocusStart();
    bool IsRunning() const { return m_running; }

private:
    int OnCreate(LPCREATESTRUCT createStruct);
    void OnSize(UINT type, CSize size);
    void OnDestroy();
    BOOL OnEraseBackground(HDC dc);
    HBRUSH OnCtlColorStatic(HDC dc, HWND window);
    LRESULT OnStart(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnCancel(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnBenchUpdate(UINT message, WPARAM wParam, LPARAM lParam, BOOL& handled);
    LRESULT OnResultsRightClick(int idCtrl, LPNMHDR notifyHeader, BOOL& handled);
    void ShowResultsMenu(const POINT& screenPoint);
    void CopySelectedResultRow();
    void CopyAllResultRows();
    void LayoutChildren();
    void JoinThread();
    void AddResult(const BenchmarkResult& result);
    void SetRunningUi(bool running);
    void ResetChart(const std::wstring& title, const std::wstring& message);

    CStatic m_targetLabel;
    CStatic m_volumeLabel;
    CComboBox m_volume;
    CStatic m_sizeLabel;
    CComboBox m_size;
    CButton m_start;
    CButton m_cancel;
    CProgressBarCtrl m_progress;
    CStatic m_status;
    CStatic m_warning;
    CThroughputChart m_chart;
    CListViewCtrl m_results;

    HFONT m_font = nullptr;
    HFONT m_boldFont = nullptr;
    StorageInfo m_storage;
    bool m_hasTarget = false;
    bool m_hasWritableVolume = false;
    bool m_running = false;
    std::atomic_bool m_benchCancel = false;
    std::thread m_thread;
    std::wstring m_chartStage;
};
