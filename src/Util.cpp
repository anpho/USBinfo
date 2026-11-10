#include "pch.h"
#include "Model.h"
#include "Language.h"

#include <cwctype>
#include <cstdio>
#include <limits>

std::wstring GetLastErrorMessage(DWORD error)
{
    if (error == ERROR_SUCCESS)
        return Tr(L"成功");
    wchar_t* buffer = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD length = ::FormatMessageW(flags, nullptr, error, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring result;
    if (length != 0 && buffer != nullptr)
    {
        result.assign(buffer, length);
        while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n' || result.back() == L' '))
            result.pop_back();
    }
    if (buffer != nullptr)
        ::LocalFree(buffer);
    if (result.empty())
        result = std::wstring(Tr(L"错误")) + L" " + std::to_wstring(error);
    else
        result += L" (" + std::to_wstring(error) + L")";
    return result;
}

std::wstring Trim(const std::wstring& text)
{
    size_t begin = 0;
    while (begin < text.size() && iswspace(text[begin]))
        ++begin;
    size_t end = text.size();
    while (end > begin && iswspace(text[end - 1]))
        --end;
    return text.substr(begin, end - begin);
}

bool StartsWithNoCase(const std::wstring& value, const std::wstring& prefix)
{
    if (prefix.size() > value.size())
        return false;
    return _wcsnicmp(value.c_str(), prefix.c_str(), prefix.size()) == 0;
}

bool ContainsNoCase(const std::wstring& value, const std::wstring& needle)
{
    if (needle.empty())
        return true;
    std::wstring upperValue = value;
    std::wstring upperNeedle = needle;
    std::transform(upperValue.begin(), upperValue.end(), upperValue.begin(), [](wchar_t ch) { return static_cast<wchar_t>(towupper(ch)); });
    std::transform(upperNeedle.begin(), upperNeedle.end(), upperNeedle.begin(), [](wchar_t ch) { return static_cast<wchar_t>(towupper(ch)); });
    return upperValue.find(upperNeedle) != std::wstring::npos;
}

std::wstring ToUpperNoCase(const std::wstring& value)
{
    std::wstring result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](wchar_t ch) { return static_cast<wchar_t>(towupper(ch)); });
    return result;
}

std::wstring MakeSingleLine(const std::wstring& text)
{
    std::wstring result;
    result.reserve(text.size());
    bool lastSpace = false;
    for (wchar_t ch : text)
    {
        if (ch == L'\r' || ch == L'\n' || ch == L'\t')
            ch = L' ';
        if (ch == L' ')
        {
            if (lastSpace)
                continue;
            lastSpace = true;
        }
        else
        {
            lastSpace = false;
        }
        result.push_back(ch);
    }
    return Trim(result);
}

std::wstring JoinStrings(const std::vector<std::wstring>& items, const wchar_t* separator)
{
    std::wstring result;
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (i != 0)
            result += separator;
        result += items[i];
    }
    return result;
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

bool WriteUtf8Report(const std::wstring& path, const std::wstring& text)
{
    if (text.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        return false;
    int utf8Size = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (utf8Size <= 0 && !text.empty())
        return false;
    std::string utf8(static_cast<size_t>(std::max(0, utf8Size)), '\0');
    if (utf8Size > 0 &&
        ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(), utf8Size, nullptr, nullptr) != utf8Size)
        return false;

    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || file == nullptr)
        return false;
    const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    bool ok = fwrite(bom, 1, sizeof(bom), file) == sizeof(bom);
    if (ok && !utf8.empty())
        ok = fwrite(utf8.data(), 1, utf8.size(), file) == utf8.size();
    if (ok)
        ok = fflush(file) == 0;
    if (fclose(file) != 0)
        ok = false;
    if (!ok)
        ::DeleteFileW(path.c_str());
    return ok;
}

std::wstring ReadBoundedWideString(const BYTE* data, size_t size, size_t offset)
{
    if (data == nullptr || offset >= size || size - offset < sizeof(wchar_t))
        return std::wstring();
    std::wstring result;
    for (size_t i = offset; i + 1 < size; i += 2)
    {
        wchar_t ch = static_cast<wchar_t>(data[i] | (static_cast<unsigned>(data[i + 1]) << 8));
        if (ch == L'\0')
            return result;
        result.push_back(ch);
    }
    return std::wstring();
}

std::wstring NormalizeDeviceText(const std::wstring& text)
{
    if (text.empty())
        return text;
    if (text[0] == L'@')
    {
        size_t semicolon = text.rfind(L';');
        if (semicolon != std::wstring::npos && semicolon + 1 < text.size())
            return Trim(text.substr(semicolon + 1));

        wchar_t buffer[1024] = {};
        if (SUCCEEDED(::SHLoadIndirectString(text.c_str(), buffer, _countof(buffer), nullptr)))
        {
            std::wstring result = Trim(buffer);
            if (!result.empty())
                return result;
        }
    }
    if (!text.empty() && text[0] == L'%')
        return text;
    return text;
}

std::wstring FormatNumber(uint64_t value)
{
    wchar_t buffer[64] = {};
    wchar_t raw[64] = {};
    swprintf_s(raw, L"%llu", static_cast<unsigned long long>(value));
    std::wstring digits(raw);
    std::wstring result;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it)
    {
        if (count != 0 && count % 3 == 0)
            result.push_back(L',');
        result.push_back(*it);
        ++count;
    }
    std::reverse(result.begin(), result.end());
    (void)buffer;
    return result;
}

std::wstring FormatBytesShort(uint64_t bytes)
{
    const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB", L"PB" };
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 5)
    {
        value /= 1024.0;
        ++unit;
    }
    wchar_t buffer[64] = {};
    if (unit == 0)
        swprintf_s(buffer, L"%llu %s", static_cast<unsigned long long>(bytes), units[unit]);
    else if (value >= 100.0)
        swprintf_s(buffer, L"%.1f %s", value, units[unit]);
    else
        swprintf_s(buffer, L"%.2f %s", value, units[unit]);
    return buffer;
}

std::wstring FormatBytes(uint64_t bytes)
{
    if (bytes == 0)
        return std::wstring(L"0 ") + Tr(L"字节");
    std::wstring shortText = FormatBytesShort(bytes);
    if (bytes < 1024)
        return shortText;
    return shortText + L" (" + FormatNumber(bytes) + L" " + Tr(L"字节") + L")";
}

std::wstring FormatSpeed(uint32_t mbps)
{
    switch (mbps)
    {
    case 0:      return Tr(L"未知");
    case 1:      return L"1.5 Mbps (Low Speed)";
    case 12:     return L"12 Mbps (Full Speed)";
    case 480:    return L"480 Mbps (High Speed)";
    case 5000:   return L"5 Gbps (SuperSpeed / USB 3.2 Gen 1)";
    case 10000:  return L"10 Gbps (SuperSpeed+ / USB 3.2 Gen 2)";
    case 20000:  return L"20 Gbps (SuperSpeed+ 2x2 / USB 3.2 Gen 2x2)";
    default:
    {
        wchar_t buffer[64] = {};
        swprintf_s(buffer, L"%u Mbps", mbps);
        return buffer;
    }
    }
}

std::wstring FormatHex(uint32_t value, int width)
{
    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"0x%0*X", width, value);
    return buffer;
}

std::wstring FormatDateString(const std::wstring& text)
{
    return Trim(text);
}

std::wstring GetFileTimeString(const FILETIME& ft)
{
    if (ft.dwLowDateTime == 0 && ft.dwHighDateTime == 0)
        return std::wstring();
    FILETIME local = {};
    SYSTEMTIME st = {};
    if (!::FileTimeToLocalFileTime(&ft, &local) || !::FileTimeToSystemTime(&local, &st))
        return std::wstring();
    wchar_t buffer[64] = {};
    swprintf_s(buffer, L"%04u-%02u-%02u %02u:%02u:%02u",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buffer;
}

std::wstring UsbClassName(int classCode)
{
    switch (classCode)
    {
    case 0x00: return Tr(L"由接口定义");
    case 0x01: return Tr(L"音频 (Audio)");
    case 0x02: return Tr(L"通信 (CDC)");
    case 0x03: return Tr(L"人机接口 (HID)");
    case 0x05: return Tr(L"物理设备");
    case 0x06: return Tr(L"静态图像 (PTP/摄像头)");
    case 0x07: return Tr(L"打印机");
    case 0x08: return Tr(L"大容量存储 (Mass Storage)");
    case 0x09: return Tr(L"USB HUB");
    case 0x0A: return Tr(L"CDC 数据");
    case 0x0B: return Tr(L"智能卡");
    case 0x0D: return Tr(L"内容安全");
    case 0x0E: return Tr(L"视频 (UVC)");
    case 0x0F: return Tr(L"个人健康");
    case 0x10: return Tr(L"音视频");
    case 0x11: return Tr(L"USB Billboard");
    case 0x12: return Tr(L"USB Type-C 桥接");
    case 0xDC: return Tr(L"诊断设备");
    case 0xE0: return Tr(L"无线控制器");
    case 0xEF: return Tr(L"杂项 / 复合设备");
    case 0xFE: return Tr(L"应用专用");
    case 0xFF: return Tr(L"厂商自定义");
    default:
    {
        return std::wstring(Tr(L"类")) + L" " + FormatHex(classCode, 2);
    }
    }
}

std::wstring UsbProtocolName(uint16_t bcdUSB)
{
    switch (bcdUSB)
    {
    case 0x0100: return L"USB 1.0";
    case 0x0110: return L"USB 1.1";
    case 0x0200: return L"USB 2.0";
    case 0x0201: return L"USB 2.1";
    case 0x0210: return L"USB 2.1 (BOS)";
    case 0x0300: return L"USB 3.0 / 3.2 Gen 1";
    case 0x0310: return L"USB 3.1 / 3.2 Gen 2";
    case 0x0320: return L"USB 3.2 Gen 2x2";
    case 0x0400: return L"USB4";
    default:
    {
        wchar_t buffer[64] = {};
        swprintf_s(buffer, L"USB BCD 0x%04X", bcdUSB);
        return buffer;
    }
    }
}

std::wstring NodeKindName(NodeKind kind)
{
    switch (kind)
    {
    case NodeKind::Computer:       return Tr(L"计算机");
    case NodeKind::HostController: return Tr(L"USB 主机控制器");
    case NodeKind::RootHub:        return Tr(L"USB 根集线器");
    case NodeKind::Hub:            return Tr(L"USB Hub");
    case NodeKind::Device:         return Tr(L"USB 设备");
    case NodeKind::Interface:      return Tr(L"USB 复合接口");
    case NodeKind::Storage:        return Tr(L"USB 存储设备");
    case NodeKind::Cable:          return Tr(L"端口 / 线缆");
    case NodeKind::Usb4:           return Tr(L"USB4 设备");
    default:                       return Tr(L"设备");
    }
}

std::wstring NodeKindShortName(NodeKind kind)
{
    switch (kind)
    {
    case NodeKind::Computer:       return Tr(L"计算机");
    case NodeKind::HostController: return Tr(L"主控制器");
    case NodeKind::RootHub:        return Tr(L"根集线器");
    case NodeKind::Hub:            return Tr(L"Hub");
    case NodeKind::Device:         return Tr(L"设备");
    case NodeKind::Interface:      return Tr(L"接口");
    case NodeKind::Storage:        return Tr(L"存储");
    case NodeKind::Cable:          return Tr(L"连线");
    case NodeKind::Usb4:           return Tr(L"USB4");
    default:                       return Tr(L"设备");
    }
}
