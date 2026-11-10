#include "pch.h"
#include "MainFrame.h"
#include "Model.h"
#include "Smart.h"
#include "UsbEnumerator.h"
#include "Language.h"
#include "resource.h"

#include <cstdio>

WTL::CAppModule _Module;

namespace
{
bool ReportSelfTest()
{
    wchar_t tempDirectory[MAX_PATH] = {};
    if (::GetTempPathW(_countof(tempDirectory), tempDirectory) == 0)
        return false;
    wchar_t tempFile[MAX_PATH] = {};
    if (::GetTempFileNameW(tempDirectory, L"USB", 0, tempFile) == 0)
        return false;

    const std::wstring path = tempFile;
    bool ok = WriteUtf8Report(path, L"USBInfo self-test");
    if (ok)
    {
        FILE* file = nullptr;
        if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || file == nullptr)
        {
            ok = false;
        }
        else
        {
            fseek(file, 0, SEEK_END);
            long size = ftell(file);
            fseek(file, 0, SEEK_SET);
            std::string bytes(static_cast<size_t>(std::max(0L, size)), '\0');
            if (size > 0)
                fread(bytes.data(), 1, bytes.size(), file);
            fclose(file);
            const std::string expected = "\xEF\xBB\xBFUSBInfo self-test";
            ok = bytes == expected;
        }
    }
    ::DeleteFileW(path.c_str());
    return ok;
}

int RunReportMode(const std::wstring& outputPath)
{
    auto snapshot = UsbEnumerator::Scan();
    std::wstring report = BuildFullReport(*snapshot);
    if (!WriteUtf8Report(outputPath, report))
    {
        std::wstring message = Tr(L"无法写入报告：");
        message += outputPath;
        ::MessageBoxW(nullptr, message.c_str(), L"USBInfo", MB_OK | MB_ICONERROR);
        return 2;
    }
    return 0;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int showCommand)
{
    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    std::wstring reportPath;
    bool reportMode = false;
    bool showHelp = false;
    bool selfTest = false;
    if (argv != nullptr)
    {
        for (int i = 1; i < argc; ++i)
        {
            std::wstring argument = argv[i];
            if (_wcsicmp(argument.c_str(), L"--report") == 0)
            {
                reportMode = true;
                if (i + 1 < argc && argv[i + 1][0] != L'-')
                {
                    reportPath = argv[i + 1];
                    ++i;
                }
            }
            else if (_wcsicmp(argument.c_str(), L"--self-test") == 0)
            {
                selfTest = true;
            }
            else if (_wcsicmp(argument.c_str(), L"--help") == 0 || _wcsicmp(argument.c_str(), L"/?") == 0)
            {
                showHelp = true;
            }
        }
        ::LocalFree(argv);
    }

    LanguageInitialize();

    if (selfTest)
    {
        if (!SmartSelfTest())
            return 2;
        if (!ReportSelfTest())
            return 3;
        return 0;
    }

    if (showHelp)
    {
        ::MessageBoxW(nullptr,
            Tr(L"USBInfo 使用说明\n\n"
               L"直接运行：打开图形界面。\n"
               L"USBInfo.exe --report [路径]：无界面扫描并导出 UTF-8 文本报告。\n\n"
               L"图形界面中可刷新、复制节点信息、导出报告，并在 USB 存储设备节点上测速。"),
            L"USBInfo", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    if (reportMode)
    {
        if (reportPath.empty())
            reportPath = DefaultReportPath();
        return RunReportMode(reportPath);
    }

    HRESULT comResult = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    INITCOMMONCONTROLSEX controls = {};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES |
        ICC_BAR_CLASSES | ICC_PROGRESS_CLASS | ICC_TAB_CLASSES | ICC_STANDARD_CLASSES;
    ::InitCommonControlsEx(&controls);

    _Module.Init(nullptr, instance);
    WTL::CMessageLoop messageLoop;
    _Module.AddMessageLoop(&messageLoop);

    CMainFrame frame;
    RECT windowRect = { 0, 0, 1200, 760 };
    if (frame.CreateEx(nullptr, &windowRect, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, WS_EX_APPWINDOW) == nullptr)
    {
        _Module.RemoveMessageLoop();
        _Module.Term();
        if (SUCCEEDED(comResult))
            ::CoUninitialize();
        return 1;
    }

    HICON icon = static_cast<HICON>(::LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
        0, 0, LR_DEFAULTSIZE | LR_SHARED));
    if (icon != nullptr)
    {
        ::SendMessageW(frame.m_hWnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
        ::SendMessageW(frame.m_hWnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
    }

    frame.CenterWindow();
    frame.ShowWindow(showCommand);
    frame.UpdateWindow();

    int result = messageLoop.Run();
    _Module.RemoveMessageLoop();
    _Module.Term();
    if (SUCCEEDED(comResult))
        ::CoUninitialize();
    return result;
}


