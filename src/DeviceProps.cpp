#include "pch.h"
#include "DeviceProps.h"
#include "Language.h"

#include <initguid.h>
#include <devpkey.h>
#include <devguid.h>

namespace
{
bool GetPropertyRaw(HDEVINFO set, SP_DEVINFO_DATA& data, const DEVPROPKEY& key, DEVPROPTYPE& type, std::vector<BYTE>& buffer)
{
    DWORD required = 0;
    type = 0;
    buffer.clear();
    ::SetLastError(ERROR_SUCCESS);
    BOOL ok = ::SetupDiGetDevicePropertyW(set, &data, &key, &type, nullptr, 0, &required, 0);
    if (!ok && ::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        return false;
    if (required == 0)
        return true;
    buffer.resize(static_cast<size_t>(required) + 16);
    DWORD returned = 0;
    ok = ::SetupDiGetDevicePropertyW(set, &data, &key, &type, buffer.data(), static_cast<DWORD>(buffer.size()), &returned, 0);
    if (!ok)
        return false;
    buffer.resize(returned);
    return true;
}

std::wstring RegValueToString(HKEY key, const wchar_t* name)
{
    DWORD type = 0;
    DWORD bytes = 0;
    if (::RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, &type, nullptr, &bytes) != ERROR_SUCCESS)
        return std::wstring();
    if (bytes == 0)
        return std::wstring();
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 2);
    if (::RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, &type, buffer.data(), &bytes) != ERROR_SUCCESS)
        return std::wstring();
    return std::wstring(buffer.data());
}

std::wstring WindowsDirectory()
{
    wchar_t buffer[MAX_PATH + 4] = {};
    UINT length = ::GetWindowsDirectoryW(buffer, _countof(buffer));
    if (length > 0 && length < _countof(buffer))
        return std::wstring(buffer, length);
    wchar_t environment[1024] = {};
    DWORD environmentLength = ::GetEnvironmentVariableW(L"SystemRoot", environment, _countof(environment));
    if (environmentLength > 0 && environmentLength < _countof(environment))
        return environment;
    return L"C:\\Windows";
}

bool EndsWithNoCase(const std::wstring& value, const std::wstring& suffix)
{
    if (suffix.size() > value.size())
        return false;
    return _wcsnicmp(value.c_str() + value.size() - suffix.size(), suffix.c_str(), suffix.size()) == 0;
}

bool IsRegularFile(const std::wstring& path)
{
    if (path.empty())
        return false;
    DWORD attributes = ::GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring ExpandEnvironmentPath(const std::wstring& path)
{
    if (path.find(L'%') == std::wstring::npos)
        return path;
    DWORD required = ::ExpandEnvironmentStringsW(path.c_str(), nullptr, 0);
    if (required == 0)
        return path;
    std::vector<wchar_t> buffer(static_cast<size_t>(required) + 2);
    DWORD written = ::ExpandEnvironmentStringsW(path.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()));
    if (written == 0 || written > buffer.size())
        return path;
    return std::wstring(buffer.data());
}

std::wstring StripPathDecoration(std::wstring path)
{
    path = Trim(path);
    if (path.size() >= 2 && path.front() == L'"')
    {
        size_t closing = path.find(L'"', 1);
        path = Trim(closing == std::wstring::npos ? path.substr(1) : path.substr(1, closing - 1));
    }
    if (StartsWithNoCase(path, L"\\??\\"))
        path = path.substr(4);
    else if (StartsWithNoCase(path, L"\\\\?\\"))
        path = path.substr(4);
    return path;
}

std::wstring FileNameOf(const std::wstring& path)
{
    size_t position = path.find_last_of(L"\\/");
    if (position == std::wstring::npos)
        return path;
    return path.substr(position + 1);
}

bool ResolveDriverImagePath(const std::wstring& imagePath, std::wstring& resolved)
{
    std::wstring candidate = StripPathDecoration(imagePath);
    candidate = ExpandEnvironmentPath(candidate);
    if (candidate.empty())
        return false;

    std::wstring windowsDirectory = WindowsDirectory();
    if (StartsWithNoCase(candidate, L"\\SystemRoot\\"))
        candidate = windowsDirectory + L"\\" + candidate.substr(12);
    else if (StartsWithNoCase(candidate, L"SystemRoot\\"))
        candidate = windowsDirectory + L"\\" + candidate.substr(11);
    else if (StartsWithNoCase(candidate, L"\\DosDevices\\"))
        candidate = candidate.substr(13);
    else if (StartsWithNoCase(candidate, L"\\Device\\"))
        candidate = candidate.substr(8);

    std::vector<std::wstring> candidates;
    auto addCandidate = [&candidates](const std::wstring& value)
    {
        if (value.empty())
            return;
        for (const auto& existing : candidates)
            if (_wcsicmp(existing.c_str(), value.c_str()) == 0)
                return;
        candidates.push_back(value);
    };

    addCandidate(candidate);
    if (candidate.size() >= 2 && candidate[1] == L':')
    {
        // Fully qualified path; the basename fallbacks below still help when a
        // driver was moved between Windows installations.
    }
    else if (!candidate.empty() && (candidate.front() == L'\\' || candidate.front() == L'/'))
    {
        addCandidate(windowsDirectory + candidate);
    }
    else
    {
        addCandidate(windowsDirectory + L"\\" + candidate);
        addCandidate(windowsDirectory + L"\\System32\\" + candidate);
        addCandidate(windowsDirectory + L"\\System32\\drivers\\" + candidate);
    }

    std::wstring fileName = FileNameOf(candidate);
    if (!fileName.empty() && _wcsicmp(fileName.c_str(), candidate.c_str()) != 0)
    {
        addCandidate(windowsDirectory + L"\\System32\\drivers\\" + fileName);
        addCandidate(windowsDirectory + L"\\System32\\" + fileName);
        addCandidate(windowsDirectory + L"\\" + fileName);
    }

    for (const auto& item : candidates)
    {
        if (IsRegularFile(item))
        {
            resolved = item;
            return true;
        }
    }
    return false;
}
}

bool GetDevicePropertyString(HDEVINFO set, SP_DEVINFO_DATA& data, const DEVPROPKEY& key, std::wstring& value)
{
    DEVPROPTYPE type = 0;
    std::vector<BYTE> buffer;
    if (!GetPropertyRaw(set, data, key, type, buffer))
        return false;
    if (type == DEVPROP_TYPE_STRING || type == DEVPROP_TYPE_STRING_INDIRECT)
    {
        if (buffer.size() < sizeof(wchar_t))
            return false;
        std::wstring text(reinterpret_cast<const wchar_t*>(buffer.data()));
        if (type == DEVPROP_TYPE_STRING_INDIRECT)
            text = NormalizeDeviceText(text);
        value = std::move(text);
        return true;
    }
    return false;
}

bool GetDevicePropertyStringList(HDEVINFO set, SP_DEVINFO_DATA& data, const DEVPROPKEY& key, std::wstring& value)
{
    DEVPROPTYPE type = 0;
    std::vector<BYTE> buffer;
    if (!GetPropertyRaw(set, data, key, type, buffer))
        return false;
    if (type != DEVPROP_TYPE_STRING_LIST)
        return false;
    const wchar_t* current = reinterpret_cast<const wchar_t*>(buffer.data());
    const wchar_t* end = reinterpret_cast<const wchar_t*>(buffer.data() + buffer.size());
    std::vector<std::wstring> items;
    while (current < end && *current != L'\0')
    {
        std::wstring item(current);
        if (!item.empty())
            items.push_back(item);
        current += item.size() + 1;
    }
    value = JoinStrings(items, L"; ");
    return true;
}

bool GetDevicePropertyUInt32(HDEVINFO set, SP_DEVINFO_DATA& data, const DEVPROPKEY& key, uint32_t& value)
{
    DEVPROPTYPE type = 0;
    std::vector<BYTE> buffer;
    if (!GetPropertyRaw(set, data, key, type, buffer))
        return false;
    if (type == DEVPROP_TYPE_UINT32 && buffer.size() >= sizeof(uint32_t))
    {
        value = *reinterpret_cast<const uint32_t*>(buffer.data());
        return true;
    }
    if (type == DEVPROP_TYPE_INT32 && buffer.size() >= sizeof(int32_t))
    {
        value = static_cast<uint32_t>(*reinterpret_cast<const int32_t*>(buffer.data()));
        return true;
    }
    if (type == DEVPROP_TYPE_UINT16 && buffer.size() >= sizeof(uint16_t))
    {
        value = *reinterpret_cast<const uint16_t*>(buffer.data());
        return true;
    }
    return false;
}

bool GetDevicePropertyGuid(HDEVINFO set, SP_DEVINFO_DATA& data, const DEVPROPKEY& key, GUID& value)
{
    DEVPROPTYPE type = 0;
    std::vector<BYTE> buffer;
    if (!GetPropertyRaw(set, data, key, type, buffer))
        return false;
    if (type != DEVPROP_TYPE_GUID || buffer.size() < sizeof(GUID))
        return false;
    value = *reinterpret_cast<const GUID*>(buffer.data());
    return true;
}

bool GetDevicePropertyFileTime(HDEVINFO set, SP_DEVINFO_DATA& data, const DEVPROPKEY& key, std::wstring& value)
{
    DEVPROPTYPE type = 0;
    std::vector<BYTE> buffer;
    if (!GetPropertyRaw(set, data, key, type, buffer))
        return false;
    if (type != DEVPROP_TYPE_FILETIME || buffer.size() < sizeof(FILETIME))
        return false;
    value = GetFileTimeString(*reinterpret_cast<const FILETIME*>(buffer.data()));
    return true;
}

bool GetDeviceRegistryString(HDEVINFO set, SP_DEVINFO_DATA& data, DWORD property, std::wstring& value)
{
    DWORD type = 0;
    DWORD required = 0;
    ::SetupDiGetDeviceRegistryPropertyW(set, &data, property, &type, nullptr, 0, &required);
    if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER || required == 0)
        return false;
    std::vector<BYTE> buffer(static_cast<size_t>(required) + sizeof(wchar_t) * 2);
    if (!::SetupDiGetDeviceRegistryPropertyW(set, &data, property, &type, buffer.data(), static_cast<DWORD>(buffer.size()), &required))
        return false;
    if (type == REG_SZ || type == REG_EXPAND_SZ)
    {
        value.assign(reinterpret_cast<const wchar_t*>(buffer.data()));
        value = NormalizeDeviceText(value);
        return true;
    }
    if (type == REG_MULTI_SZ)
    {
        const wchar_t* current = reinterpret_cast<const wchar_t*>(buffer.data());
        std::vector<std::wstring> items;
        for (int i = 0; i < 64 && *current != L'\0'; ++i)
        {
            std::wstring item(current);
            if (!item.empty())
                items.push_back(item);
            current += item.size() + 1;
        }
        value = JoinStrings(items, L"; ");
        return true;
    }
    return false;
}

bool GetDeviceRegistryGuid(HDEVINFO set, SP_DEVINFO_DATA& data, DWORD property, GUID& value)
{
    DWORD type = 0;
    DWORD required = 0;
    ::SetupDiGetDeviceRegistryPropertyW(set, &data, property, &type, nullptr, 0, &required);
    if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER || required < sizeof(GUID))
        return false;
    std::vector<BYTE> buffer(required);
    if (!::SetupDiGetDeviceRegistryPropertyW(set, &data, property, &type, buffer.data(), required, &required))
        return false;
    if (type != REG_BINARY || buffer.size() < sizeof(GUID))
        return false;
    value = *reinterpret_cast<const GUID*>(buffer.data());
    return true;
}

bool GetDeviceRegistryDword(HDEVINFO set, SP_DEVINFO_DATA& data, DWORD property, DWORD& value)
{
    DWORD type = 0;
    DWORD required = 0;
    ::SetupDiGetDeviceRegistryPropertyW(set, &data, property, &type, nullptr, 0, &required);
    if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER || required < sizeof(DWORD))
        return false;
    std::vector<BYTE> buffer(required);
    if (!::SetupDiGetDeviceRegistryPropertyW(set, &data, property, &type, buffer.data(), required, &required))
        return false;
    if (type != REG_DWORD || buffer.size() < sizeof(DWORD))
        return false;
    value = *reinterpret_cast<const DWORD*>(buffer.data());
    return true;
}

std::wstring GetDeviceInstanceId(HDEVINFO set, const SP_DEVINFO_DATA& data)
{
    DWORD required = 0;
    ::SetupDiGetDeviceInstanceIdW(set, const_cast<SP_DEVINFO_DATA*>(&data), nullptr, 0, &required);
    if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER || required == 0)
        return std::wstring();
    std::vector<wchar_t> buffer(required + 2);
    if (!::SetupDiGetDeviceInstanceIdW(set, const_cast<SP_DEVINFO_DATA*>(&data), buffer.data(), static_cast<DWORD>(buffer.size()), &required))
        return std::wstring();
    return buffer.data();
}

std::wstring GetDeviceId(DEVINST devInst)
{
    wchar_t buffer[MAX_DEVICE_ID_LEN + 1] = {};
    if (::CM_Get_Device_IDW(devInst, buffer, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS)
        return std::wstring();
    return buffer;
}

std::wstring GetDeviceParentId(DEVINST devInst)
{
    DEVINST parent = 0;
    if (::CM_Get_Parent(&parent, devInst, 0) != CR_SUCCESS)
        return std::wstring();
    return GetDeviceId(parent);
}

std::vector<std::wstring> GetDeviceChildren(DEVINST devInst)
{
    std::vector<std::wstring> result;
    DEVINST child = 0;
    if (::CM_Get_Child(&child, devInst, 0) != CR_SUCCESS)
        return result;
    for (int i = 0; i < 256; ++i)
    {
        std::wstring id = GetDeviceId(child);
        if (!id.empty())
            result.push_back(id);
        DEVINST sibling = 0;
        if (::CM_Get_Sibling(&sibling, child, 0) != CR_SUCCESS)
            break;
        child = sibling;
    }
    return result;
}

std::wstring GetRegistryString(HKEY root, const std::wstring& subKey, const wchar_t* name)
{
    HKEY key = nullptr;
    if (::RegOpenKeyExW(root, subKey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS)
        return std::wstring();
    std::wstring value = RegValueToString(key, name);
    ::RegCloseKey(key);
    return value;
}

DWORD GetRegistryDword(HKEY root, const std::wstring& subKey, const wchar_t* name, DWORD defaultValue)
{
    HKEY key = nullptr;
    if (::RegOpenKeyExW(root, subKey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS)
        return defaultValue;
    DWORD value = defaultValue;
    DWORD bytes = sizeof(value);
    ::RegGetValueW(key, nullptr, name, RRF_RT_REG_DWORD, nullptr, &value, &bytes);
    ::RegCloseKey(key);
    return value;
}

std::wstring NormalizeDriverKey(const std::wstring& key)
{
    std::wstring result = key;
    const std::wstring prefix = L"\\REGISTRY\\MACHINE\\SYSTEM\\CURRENTCONTROLSET\\CONTROL\\CLASS\\";
    std::wstring upper = ToUpperNoCase(result);
    size_t position = upper.find(prefix);
    if (position != std::wstring::npos)
        result = result.substr(position + prefix.size());
    while (!result.empty() && result.front() == L'\\')
        result.erase(result.begin());
    return result;
}

std::wstring GetDriverClassKeyPath(const std::wstring& driverKey)
{
    std::wstring normalized = NormalizeDriverKey(driverKey);
    if (normalized.empty())
        return std::wstring();
    return L"SYSTEM\\CurrentControlSet\\Control\\Class\\" + normalized;
}

std::wstring ReadDriverClassValue(const std::wstring& driverKey, const wchar_t* valueName)
{
    std::wstring path = GetDriverClassKeyPath(driverKey);
    if (path.empty())
        return std::wstring();
    return GetRegistryString(HKEY_LOCAL_MACHINE, path, valueName);
}

bool ResolveDriverFile(const std::wstring& service, const std::wstring& driverInf,
    std::wstring& path, std::wstring& kind)
{
    path.clear();
    kind.clear();

    if (!service.empty())
    {
        std::wstring imagePath = GetRegistryString(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Services\\" + service, L"ImagePath");
        std::wstring resolved;
        if (ResolveDriverImagePath(imagePath, resolved))
        {
            path = resolved;
            kind = EndsWithNoCase(path, L".inf") ? Tr(L"INF 文件") : Tr(L"驱动文件");
            return true;
        }
    }

    std::wstring infName = Trim(driverInf);
    if (!infName.empty())
    {
        if (!EndsWithNoCase(infName, L".inf"))
            infName += L".inf";

        std::wstring windowsDirectory = WindowsDirectory();
        std::wstring infPath;
        if (infName.size() >= 2 && infName[1] == L':')
            infPath = infName;
        else if (infName.find(L'\\') != std::wstring::npos || infName.find(L'/') != std::wstring::npos)
            infPath = windowsDirectory + L"\\" + infName;
        else
            infPath = windowsDirectory + L"\\INF\\" + infName;

        if (IsRegularFile(infPath))
        {
            path = infPath;
            kind = Tr(L"INF 文件");
            return true;
        }
    }

    return false;
}

bool IsProcessElevated()
{
    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;
    TOKEN_ELEVATION elevation = {};
    DWORD length = 0;
    BOOL ok = ::GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &length);
    ::CloseHandle(token);
    return ok != FALSE && elevation.TokenIsElevated != 0;
}

bool RelaunchElevated(const std::wstring& arguments)
{
    wchar_t exePath[MAX_PATH * 2] = {};
    if (::GetModuleFileNameW(nullptr, exePath, _countof(exePath)) == 0)
        return false;

    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = exePath;
    info.lpParameters = arguments.empty() ? nullptr : arguments.c_str();
    info.lpDirectory = nullptr;
    info.nShow = SW_SHOWNORMAL;
    if (!::ShellExecuteExW(&info))
        return false;
    if (info.hProcess != nullptr)
        ::CloseHandle(info.hProcess);
    return true;
}

std::wstring GetWindowsBuildString()
{
    OSVERSIONINFOEXW version = {};
    version.dwOSVersionInfoSize = sizeof(version);
#pragma warning(push)
#pragma warning(disable: 4996)
    if (::GetVersionExW(reinterpret_cast<OSVERSIONINFOW*>(&version)))
    {
        wchar_t buffer[128] = {};
        swprintf_s(buffer, L"Windows %u.%u.%u", version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber);
        return buffer;
    }
#pragma warning(pop)
    return L"Windows";
}
