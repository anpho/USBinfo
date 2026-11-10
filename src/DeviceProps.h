#pragma once

#include "pch.h"
#include "Model.h"

bool GetDevicePropertyString(HDEVINFO set, SP_DEVINFO_DATA& data, const DEVPROPKEY& key, std::wstring& value);
bool GetDevicePropertyStringList(HDEVINFO set, SP_DEVINFO_DATA& data, const DEVPROPKEY& key, std::wstring& value);
bool GetDevicePropertyUInt32(HDEVINFO set, SP_DEVINFO_DATA& data, const DEVPROPKEY& key, uint32_t& value);
bool GetDevicePropertyGuid(HDEVINFO set, SP_DEVINFO_DATA& data, const DEVPROPKEY& key, GUID& value);
bool GetDevicePropertyFileTime(HDEVINFO set, SP_DEVINFO_DATA& data, const DEVPROPKEY& key, std::wstring& value);
bool GetDeviceRegistryString(HDEVINFO set, SP_DEVINFO_DATA& data, DWORD property, std::wstring& value);
bool GetDeviceRegistryGuid(HDEVINFO set, SP_DEVINFO_DATA& data, DWORD property, GUID& value);
bool GetDeviceRegistryDword(HDEVINFO set, SP_DEVINFO_DATA& data, DWORD property, DWORD& value);

std::wstring GetDeviceInstanceId(HDEVINFO set, const SP_DEVINFO_DATA& data);
std::wstring GetDeviceId(DEVINST devInst);
std::wstring GetDeviceParentId(DEVINST devInst);
std::vector<std::wstring> GetDeviceChildren(DEVINST devInst);
std::wstring GetRegistryString(HKEY root, const std::wstring& subKey, const wchar_t* name);
DWORD GetRegistryDword(HKEY root, const std::wstring& subKey, const wchar_t* name, DWORD defaultValue = 0);

std::wstring NormalizeDriverKey(const std::wstring& key);
std::wstring ReadDriverClassValue(const std::wstring& driverKey, const wchar_t* valueName);
std::wstring GetDriverClassKeyPath(const std::wstring& driverKey);

// Resolve the on-disk driver file for a PnP device.  The service ImagePath is
// preferred (with \SystemRoot\, \??\, System32\drivers\ and environment
// variable handling); when that yields no existing file the INF path is used.
// path/kind are cleared when nothing could be found.
bool ResolveDriverFile(const std::wstring& service, const std::wstring& driverInf,
    std::wstring& path, std::wstring& kind);

bool IsProcessElevated();
bool RelaunchElevated(const std::wstring& arguments = std::wstring());
std::wstring GetWindowsBuildString();
