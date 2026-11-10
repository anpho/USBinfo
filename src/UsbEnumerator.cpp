#include "pch.h"
#include "UsbEnumerator.h"
#include "StorageEnumerator.h"
#include "DeviceProps.h"
#include "ChipDb.h"

#include <initguid.h>
#include <devguid.h>
#include <devpkey.h>
#include <usbiodef.h>
#include <usbioctl.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <winioctl.h>

#include <cstddef>

namespace
{
struct CaseInsensitiveLess
{
    bool operator()(const std::wstring& left, const std::wstring& right) const
    {
        return _wcsicmp(left.c_str(), right.c_str()) < 0;
    }
};

std::wstring GuidToString(const GUID& guid)
{
    wchar_t buffer[64] = {};
    if (::StringFromGUID2(guid, buffer, _countof(buffer)) <= 0)
        return std::wstring();
    return buffer;
}

int AddNode(UsbSnapshot& snapshot, UsbNode node)
{
    node.id = static_cast<int>(snapshot.nodes.size());
    snapshot.nodes.push_back(std::move(node));
    return static_cast<int>(snapshot.nodes.size()) - 1;
}

void AddWarning(UsbSnapshot& snapshot, const std::wstring& warning)
{
    for (const auto& existing : snapshot.warnings)
        if (existing == warning)
            return;
    snapshot.warnings.push_back(warning);
}

std::wstring FormatGuidProperty(HDEVINFO set, SP_DEVINFO_DATA& data, const DEVPROPKEY& key)
{
    GUID guid = {};
    if (GetDevicePropertyGuid(set, data, key, guid))
        return GuidToString(guid);
    return std::wstring();
}

void FillDeviceProperties(HDEVINFO set, SP_DEVINFO_DATA& data, UsbNode& node)
{
    node.instanceId = GetDeviceInstanceId(set, data);
    node.parentInstanceId = GetDeviceParentId(data.DevInst);

    GetDevicePropertyString(set, data, DEVPKEY_Device_DeviceDesc, node.description);
    if (node.description.empty())
        GetDeviceRegistryString(set, data, SPDRP_DEVICEDESC, node.description);
    node.description = NormalizeDeviceText(node.description);

    GetDevicePropertyString(set, data, DEVPKEY_Device_FriendlyName, node.friendlyName);
    node.friendlyName = NormalizeDeviceText(node.friendlyName);

    GetDevicePropertyString(set, data, DEVPKEY_Device_Manufacturer, node.manufacturer);
    if (node.manufacturer.empty())
        GetDeviceRegistryString(set, data, SPDRP_MFG, node.manufacturer);
    node.manufacturer = NormalizeDeviceText(node.manufacturer);

    GetDevicePropertyString(set, data, DEVPKEY_Device_Model, node.model);
    node.model = NormalizeDeviceText(node.model);

    GetDevicePropertyString(set, data, DEVPKEY_Device_Service, node.service);
    if (node.service.empty())
        GetDeviceRegistryString(set, data, SPDRP_SERVICE, node.service);

    GetDevicePropertyString(set, data, DEVPKEY_Device_Class, node.className);
    if (node.className.empty())
        GetDeviceRegistryString(set, data, SPDRP_CLASS, node.className);

    GetDevicePropertyString(set, data, DEVPKEY_Device_EnumeratorName, node.enumerator);
    GetDevicePropertyString(set, data, DEVPKEY_Device_LocationInfo, node.locationInfo);
    GetDevicePropertyStringList(set, data, DEVPKEY_Device_LocationPaths, node.locationPaths);
    GetDevicePropertyString(set, data, DEVPKEY_Device_Driver, node.driverKey);
    if (node.driverKey.empty())
        GetDeviceRegistryString(set, data, SPDRP_DRIVER, node.driverKey);
    node.driverKey = NormalizeDriverKey(node.driverKey);

    GetDevicePropertyString(set, data, DEVPKEY_Device_DriverDesc, node.driverDesc);
    GetDevicePropertyString(set, data, DEVPKEY_Device_DriverVersion, node.driverVersion);
    GetDevicePropertyString(set, data, DEVPKEY_Device_DriverProvider, node.driverProvider);
    GetDevicePropertyString(set, data, DEVPKEY_Device_DriverInfPath, node.driverInf);
    GetDevicePropertyFileTime(set, data, DEVPKEY_Device_DriverDate, node.driverDate);
    if (node.driverDate.empty())
        node.driverDate = ReadDriverClassValue(node.driverKey, L"DriverDate");

    if (node.driverVersion.empty())
        node.driverVersion = ReadDriverClassValue(node.driverKey, L"DriverVersion");
    if (node.driverDesc.empty())
        node.driverDesc = ReadDriverClassValue(node.driverKey, L"DriverDesc");
    if (node.driverProvider.empty())
        node.driverProvider = ReadDriverClassValue(node.driverKey, L"ProviderName");
    if (node.driverInf.empty())
        node.driverInf = ReadDriverClassValue(node.driverKey, L"InfPath");
    if (node.driverDate.empty())
        node.driverDate = ReadDriverClassValue(node.driverKey, L"DriverDate");

    ResolveDriverFile(node.service, node.driverInf, node.driverFile, node.driverFileKind);

    GetDevicePropertyStringList(set, data, DEVPKEY_Device_HardwareIds, node.hardwareIds);
    if (node.hardwareIds.empty())
        GetDeviceRegistryString(set, data, SPDRP_HARDWAREID, node.hardwareIds);
    GetDevicePropertyStringList(set, data, DEVPKEY_Device_CompatibleIds, node.compatibleIds);
    if (node.compatibleIds.empty())
        GetDeviceRegistryString(set, data, SPDRP_COMPATIBLEIDS, node.compatibleIds);

    node.containerId = FormatGuidProperty(set, data, DEVPKEY_Device_ContainerId);
    GetDevicePropertyString(set, data, DEVPKEY_Device_BusReportedDeviceDesc, node.busReportedDesc);
    node.busReportedDesc = NormalizeDeviceText(node.busReportedDesc);
    node.classGuid = FormatGuidProperty(set, data, DEVPKEY_Device_ClassGuid);

    uint32_t number = 0;
    if (GetDevicePropertyUInt32(set, data, DEVPKEY_Device_Address, number))
    {
        node.address = static_cast<int>(number);
        node.hasAddress = true;
    }
    if (GetDevicePropertyUInt32(set, data, DEVPKEY_Device_BusNumber, number))
    {
        node.busNumber = static_cast<int>(number);
        node.hasBusNumber = true;
    }
    GetDevicePropertyString(set, data, DEVPKEY_Device_DriverRank, node.driverRank);
    GetDevicePropertyFileTime(set, data, DEVPKEY_Device_InstallDate, node.installDate);
    GetDevicePropertyFileTime(set, data, DEVPKEY_Device_LastArrivalDate, node.lastArrivalDate);

    uint32_t value = 0;
    if (GetDevicePropertyUInt32(set, data, DEVPKEY_Device_ProblemCode, value) && value != 0)
        node.problem = L"问题代码 " + std::to_wstring(value);
    if (GetDevicePropertyUInt32(set, data, DEVPKEY_Device_Capabilities, value))
        node.capabilities = FormatHex(value);
    if (GetDevicePropertyUInt32(set, data, DEVPKEY_Device_ConfigFlags, value))
        node.configFlags = FormatHex(value);

    DEVINST status = 0;
    ULONG problem = 0;
    if (::CM_Get_DevNode_Status(&status, &problem, data.DevInst, 0) == CR_SUCCESS)
    {
        if ((status & DN_HAS_PROBLEM) != 0 && node.problem.empty())
            node.problem = L"问题代码 " + std::to_wstring(problem);
    }

    node.connected = true;
    node.isInterface = ContainsNoCase(node.instanceId, L"&MI_");
    node.isRootHub = StartsWithNoCase(node.instanceId, L"USB\\ROOT_HUB");
    node.isUsb4 = StartsWithNoCase(node.instanceId, L"USB4\\");
    node.isHostController = StartsWithNoCase(node.instanceId, L"PCI\\") &&
        (_wcsicmp(node.classGuid.c_str(), GuidToString(GUID_DEVCLASS_USB).c_str()) == 0 ||
         ContainsNoCase(node.service, L"USBXHCI") || ContainsNoCase(node.service, L"USBEHCI") ||
         ContainsNoCase(node.service, L"USBOHCI") || ContainsNoCase(node.service, L"USBUHCI"));
    node.isHub = node.isRootHub || ContainsNoCase(node.service, L"USBHUB3") ||
        _wcsicmp(node.service.c_str(), L"usbhub") == 0;
    node.isUsbStorage = ContainsNoCase(node.service, L"USBSTOR");
    if (ContainsNoCase(node.description, L"Mass Storage") || ContainsNoCase(node.description, L"大容量存储"))
        node.isUsbStorage = true;

    uint16_t vid = 0;
    uint16_t pid = 0;
    bool hasVidPid = false;
    std::wstring upperSource = ToUpperNoCase(node.hardwareIds.empty() ? node.instanceId : node.hardwareIds);
    auto parseId = [&upperSource](const wchar_t* token, uint16_t& value) -> bool
    {
        size_t position = upperSource.find(token);
        while (position != std::wstring::npos)
        {
            size_t digits = position + wcslen(token);
            if (digits < upperSource.size() && upperSource[digits] == L'_')
                ++digits;
            if (digits + 4 <= upperSource.size())
            {
                bool valid = true;
                for (size_t i = 0; i < 4; ++i)
                {
                    wchar_t ch = upperSource[digits + i];
                    bool hex = (ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'F');
                    if (!hex)
                    {
                        valid = false;
                        break;
                    }
                }
                if (valid)
                {
                    value = static_cast<uint16_t>(std::stoul(upperSource.substr(digits, 4), nullptr, 16));
                    return true;
                }
            }
            position = upperSource.find(token, position + 1);
        }
        return false;
    };
    hasVidPid = parseId(L"VID_", vid) && parseId(L"PID_", pid);
    if (!hasVidPid)
    {
        vid = 0;
        pid = 0;
        hasVidPid = parseId(L"VID", vid) && parseId(L"PID", pid);
    }
    if (hasVidPid)
    {
        node.idVendor = vid;
        node.idProduct = pid;
        ChipIdentity chip = LookupUsbChip(vid, pid, node.isHub);
        if (chip.known || !chip.vendor.empty())
        {
            node.chipVendor = chip.vendor;
            node.chipModel = chip.chip;
            node.chipSource = chip.note;
        }
    }

    if (node.isHostController)
    {
        size_t venPosition = ToUpperNoCase(node.instanceId).find(L"VEN_");
        size_t devPosition = ToUpperNoCase(node.instanceId).find(L"DEV_");
        if (venPosition != std::wstring::npos && devPosition != std::wstring::npos &&
            venPosition + 8 <= node.instanceId.size() && devPosition + 8 <= node.instanceId.size())
        {
            try
            {
                uint16_t vendor = static_cast<uint16_t>(std::stoul(node.instanceId.substr(venPosition + 4, 4), nullptr, 16));
                uint16_t device = static_cast<uint16_t>(std::stoul(node.instanceId.substr(devPosition + 4, 4), nullptr, 16));
                node.chipVendor = LookupPciVendor(vendor);
                node.chipModel = LookupPciUsbController(vendor, device);
                node.chipSource = L"PCI 厂商/设备 ID 推断";
            }
            catch (...)
            {
            }
        }
    }
    else if (node.isRootHub && node.idVendor != 0)
    {
        switch (node.idVendor)
        {
        case 0x8086:
            node.chipVendor = L"Intel";
            node.chipModel = L"PCH 集成 USB 根集线器";
            node.chipSource = L"根集线器 VID 推断";
            break;
        case 0x1022:
            node.chipVendor = L"AMD";
            node.chipModel = L"芯片组集成 USB 根集线器";
            node.chipSource = L"根集线器 VID 推断";
            break;
        case 0x1B21:
            node.chipVendor = L"ASMedia";
            node.chipModel = L"USB 根集线器";
            node.chipSource = L"根集线器 VID 推断";
            break;
        case 0x1912:
            node.chipVendor = L"Renesas";
            node.chipModel = L"USB 根集线器";
            node.chipSource = L"根集线器 VID 推断";
            break;
        default:
            node.chipVendor = L"未知";
            node.chipModel = L"USB 根集线器";
            node.chipSource = L"根集线器 VID 推断";
            break;
        }
    }
}

std::wstring CurrentTimestamp()
{
    SYSTEMTIME local = {};
    ::GetLocalTime(&local);
    wchar_t buffer[64] = {};
    swprintf_s(buffer, L"%04u-%02u-%02u %02u:%02u:%02u",
        local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond);
    return buffer;
}

uint32_t SpeedMbpsFromConnection(UCHAR speed, const USB_NODE_CONNECTION_INFORMATION_EX_V2* v2)
{
    if (v2 != nullptr)
    {
        if (v2->Flags.DeviceIsOperatingAtSuperSpeedPlusOrHigher)
            return 10000;
        if (v2->Flags.DeviceIsOperatingAtSuperSpeedOrHigher)
            return 5000;
    }
    switch (speed)
    {
    case UsbLowSpeed:   return 1;
    case UsbFullSpeed:  return 12;
    case UsbHighSpeed:  return 480;
    case UsbSuperSpeed: return 5000;
    default:            return 0;
    }
}

std::wstring SupportedProtocolsText(const USB_NODE_CONNECTION_INFORMATION_EX_V2* v2)
{
    if (v2 == nullptr)
        return std::wstring();
    std::vector<std::wstring> items;
    if (v2->SupportedUsbProtocols.Usb110)
        items.emplace_back(L"USB 1.1");
    if (v2->SupportedUsbProtocols.Usb200)
        items.emplace_back(L"USB 2.0");
    if (v2->SupportedUsbProtocols.Usb300)
        items.emplace_back(L"USB 3.0");
    if (v2->Flags.DeviceIsOperatingAtSuperSpeedPlusOrHigher || v2->Flags.DeviceIsSuperSpeedPlusCapableOrHigher)
        items.emplace_back(L"SuperSpeed+");
    return JoinStrings(items, L" / ");
}

std::wstring HubTypeText(USB_HUB_TYPE type)
{
    switch (type)
    {
    case UsbRootHub: return L"USB 根集线器";
    case Usb20Hub:   return L"USB 2.0 Hub";
    case Usb30Hub:   return L"USB 3.x Hub";
    default:         return L"USB Hub";
    }
}

bool QueryUsbDescriptor(HANDLE hub, ULONG port, BYTE descriptorType, BYTE descriptorIndex, WORD languageId,
    BYTE* output, DWORD outputSize, DWORD* actualSize)
{
    if (hub == nullptr || hub == INVALID_HANDLE_VALUE || output == nullptr || outputSize == 0)
        return false;

    const size_t headerSize = offsetof(USB_DESCRIPTOR_REQUEST, Data);
    std::vector<BYTE> buffer(headerSize + outputSize + 16);
    auto* request = reinterpret_cast<USB_DESCRIPTOR_REQUEST*>(buffer.data());
    request->ConnectionIndex = port;
    request->SetupPacket.bmRequest = 0x80;
    request->SetupPacket.bRequest = 0x06; // GET_DESCRIPTOR
    request->SetupPacket.wValue = static_cast<USHORT>((descriptorType << 8) | descriptorIndex);
    request->SetupPacket.wIndex = languageId;
    request->SetupPacket.wLength = static_cast<USHORT>(outputSize);

    DWORD returned = 0;
    if (!::DeviceIoControl(hub, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
        buffer.data(), static_cast<DWORD>(buffer.size()),
        buffer.data(), static_cast<DWORD>(buffer.size()), &returned, nullptr))
        return false;
    if (returned <= headerSize)
        return false;
    DWORD count = std::min<DWORD>(returned - static_cast<DWORD>(headerSize), outputSize);
    memcpy(output, buffer.data() + headerSize, count);
    if (actualSize != nullptr)
        *actualSize = count;
    return true;
}

std::wstring DecodeStringDescriptor(const BYTE* data, DWORD size)
{
    if (data == nullptr || size < 2)
        return std::wstring();
    BYTE length = std::min<BYTE>(data[0], static_cast<BYTE>(size));
    if (length < 2)
        return std::wstring();
    std::wstring result;
    for (DWORD i = 2; i + 1 < length; i += 2)
    {
        wchar_t ch = static_cast<wchar_t>(data[i] | (data[i + 1] << 8));
        if (ch == L'\0')
            break;
        result.push_back(ch);
    }
    return result;
}

std::wstring QueryStringDescriptor(HANDLE hub, ULONG port, BYTE index)
{
    if (index == 0)
        return std::wstring();
    BYTE buffer[512] = {};
    DWORD actual = 0;
    WORD languageId = 0x0409;
    if (!QueryUsbDescriptor(hub, port, USB_STRING_DESCRIPTOR_TYPE, index, languageId, buffer, sizeof(buffer), &actual))
    {
        BYTE languages[256] = {};
        DWORD languageBytes = 0;
        if (!QueryUsbDescriptor(hub, port, USB_STRING_DESCRIPTOR_TYPE, 0, 0, languages, sizeof(languages), &languageBytes))
            return std::wstring();
        if (languageBytes >= 4)
            languageId = static_cast<WORD>(languages[2] | (languages[3] << 8));
        if (!QueryUsbDescriptor(hub, port, USB_STRING_DESCRIPTOR_TYPE, index, languageId, buffer, sizeof(buffer), &actual))
            return std::wstring();
    }
    return DecodeStringDescriptor(buffer, actual);
}

void QueryConfigurationDescriptor(HANDLE hub, ULONG port, UsbNode& node)
{
    BYTE buffer[4096] = {};
    DWORD actual = 0;
    if (!QueryUsbDescriptor(hub, port, USB_CONFIGURATION_DESCRIPTOR_TYPE, 0, 0, buffer, sizeof(buffer), &actual))
        return;
    if (actual < 9 || buffer[1] != USB_CONFIGURATION_DESCRIPTOR_TYPE)
        return;

    auto* config = reinterpret_cast<USB_CONFIGURATION_DESCRIPTOR*>(buffer);
    WORD totalLength = std::min<WORD>(config->wTotalLength, static_cast<WORD>(actual));
    node.configSummary = L"配置值 " + std::to_wstring(config->bConfigurationValue) +
        L"，" + std::to_wstring(config->bNumInterfaces) + L" 个接口，" +
        ((config->bmAttributes & USB_CONFIG_SELF_POWERED) ? L"自供电" : L"总线供电") +
        L"，最大 " + std::to_wstring(static_cast<unsigned>(config->MaxPower) * 2) + L" mA";

    int interfaceIndex = -1;
    for (DWORD offset = 0; offset + 2 <= totalLength;)
    {
        const BYTE* descriptor = buffer + offset;
        BYTE length = descriptor[0];
        BYTE type = descriptor[1];
        if (length < 2 || offset + length > totalLength)
            break;
        if (type == USB_INTERFACE_DESCRIPTOR_TYPE && length >= 9)
        {
            UsbInterfaceInfo interfaceInfo;
            interfaceInfo.number = descriptor[2];
            interfaceInfo.alternate = descriptor[3];
            interfaceInfo.endpoints = descriptor[4];
            interfaceInfo.classCode = descriptor[5];
            interfaceInfo.subClass = descriptor[6];
            interfaceInfo.protocol = descriptor[7];
            interfaceInfo.text = L"接口 " + std::to_wstring(interfaceInfo.number) +
                L" (Alt " + std::to_wstring(interfaceInfo.alternate) + L"): " +
                UsbClassName(interfaceInfo.classCode);
            if (interfaceInfo.endpoints > 0)
                interfaceInfo.text += L"，端点 " + std::to_wstring(interfaceInfo.endpoints);
            node.interfaces.push_back(interfaceInfo);
            interfaceIndex = static_cast<int>(node.interfaces.size()) - 1;
            if (interfaceInfo.classCode == 0x08)
                node.isUsbStorage = true;
            if (interfaceInfo.classCode == 0x09)
                node.isHub = true;
        }
        else if (type == USB_ENDPOINT_DESCRIPTOR_TYPE && interfaceIndex >= 0 && length >= 7)
        {
            // bNumEndpoints is authoritative; nothing extra needed.
        }
        else if (type == 0x21 && interfaceIndex >= 0 && length >= 9)
        {
            node.interfaces[static_cast<size_t>(interfaceIndex)].text += L"，HID";
        }
        offset += length;
    }

    if (node.deviceClass == 0xEF || node.isInterface)
        node.isComposite = true;
}

std::wstring BillboardSummary(const BYTE* data, DWORD size)
{
    if (data == nullptr || size < 16 || data[0] < 16)
        return std::wstring();
    auto* billboard = reinterpret_cast<const USB_DEVICE_CAPABILITY_BILLBOARD_DESCRIPTOR*>(data);
    int modes = billboard->bNumberOfAlternateModes;
    if (modes <= 0)
        return L"USB Billboard（未报告备用模式）";
    std::vector<std::wstring> items;
    for (int i = 0; i < modes && i < 16; ++i)
    {
        size_t offset = offsetof(USB_DEVICE_CAPABILITY_BILLBOARD_DESCRIPTOR, AlternateMode) + i * sizeof(billboard->AlternateMode[0]);
        if (offset + 4 > data[0] || offset + 4 > size)
            break;
        WORD svid = static_cast<WORD>(data[offset] | (data[offset + 1] << 8));
        wchar_t buffer[64] = {};
        swprintf_s(buffer, L"SVID 0x%04X", svid);
        items.emplace_back(buffer);
    }
    if (items.empty())
        return L"USB Billboard";
    return L"USB Billboard: " + JoinStrings(items, L", ") + L"（Alternate Mode）";
}

void QueryBosDescriptor(HANDLE hub, ULONG port, UsbNode& node)
{
    BYTE buffer[1024] = {};
    DWORD actual = 0;
    if (!QueryUsbDescriptor(hub, port, USB_BOS_DESCRIPTOR_TYPE, 0, 0, buffer, sizeof(buffer), &actual))
        return;
    if (actual < 5 || buffer[1] != USB_BOS_DESCRIPTOR_TYPE)
        return;
    auto* bos = reinterpret_cast<const USB_BOS_DESCRIPTOR*>(buffer);
    WORD total = std::min<WORD>(bos->wTotalLength, static_cast<WORD>(actual));
    std::vector<std::wstring> capabilities;
    for (DWORD offset = bos->bLength; offset + 3 <= total;)
    {
        BYTE length = buffer[offset];
        BYTE capability = buffer[offset + 2];
        if (length < 3 || offset + length > total)
            break;
        std::wstring text;
        switch (capability)
        {
        case USB_DEVICE_CAPABILITY_USB20_EXTENSION: text = L"USB 2.0 Extension"; break;
        case USB_DEVICE_CAPABILITY_SUPERSPEED_USB: text = L"SuperSpeed USB"; break;
        case USB_DEVICE_CAPABILITY_CONTAINER_ID: text = L"Container ID"; break;
        case USB_DEVICE_CAPABILITY_PLATFORM: text = L"Platform"; break;
        case USB_DEVICE_CAPABILITY_POWER_DELIVERY: text = L"USB Power Delivery"; break;
        case USB_DEVICE_CAPABILITY_BATTERY_INFO: text = L"Battery Info"; break;
        case USB_DEVICE_CAPABILITY_PD_CONSUMER_PORT: text = L"PD Consumer Port"; break;
        case USB_DEVICE_CAPABILITY_PD_PROVIDER_PORT: text = L"PD Provider Port"; break;
        case USB_DEVICE_CAPABILITY_SUPERSPEEDPLUS_USB: text = L"SuperSpeed+ USB"; break;
        case USB_DEVICE_CAPABILITY_PRECISION_TIME_MEASUREMENT: text = L"Precision Time Measurement"; break;
        case USB_DEVICE_CAPABILITY_BILLBOARD:
            text = BillboardSummary(buffer + offset, length);
            node.cable.billboard = text;
            if (!text.empty())
            {
                node.cable.available = true;
                node.cable.source = L"USB BOS Billboard Descriptor";
            }
            break;
        case USB_DEVICE_CAPABILITY_FIRMWARE_STATUS: text = L"Firmware Status"; break;
        default:
        {
            wchar_t item[64] = {};
            swprintf_s(item, L"Capability 0x%02X", capability);
            text = item;
            break;
        }
        }
        if (!text.empty())
            capabilities.push_back(text);
        offset += length;
    }
    if (!capabilities.empty())
        node.bosSummary = JoinStrings(capabilities, L"; ");
}

void FillCableFromConnector(HANDLE hub, ULONG port, UsbNode& node)
{
    USB_PORT_CONNECTOR_PROPERTIES header = {};
    header.ConnectionIndex = port;
    DWORD returned = 0;
    if (!::DeviceIoControl(hub, IOCTL_USB_GET_PORT_CONNECTOR_PROPERTIES,
        &header, sizeof(header), &header, sizeof(header), &returned, nullptr))
        return;

    DWORD actualLength = std::max<DWORD>(header.ActualLength, sizeof(USB_PORT_CONNECTOR_PROPERTIES));
    if (actualLength > 4096)
        actualLength = 4096;
    std::vector<BYTE> buffer(actualLength);
    auto* properties = reinterpret_cast<USB_PORT_CONNECTOR_PROPERTIES*>(buffer.data());
    properties->ConnectionIndex = port;
    if (!::DeviceIoControl(hub, IOCTL_USB_GET_PORT_CONNECTOR_PROPERTIES,
        properties, actualLength, properties, actualLength, &returned, nullptr))
        return;

    CableInfo& cable = node.cable;
    cable.portNumber = static_cast<int>(port);
    cable.typeC = properties->UsbPortProperties.PortConnectorIsTypeC != 0;
    cable.userConnectable = properties->UsbPortProperties.PortIsUserConnectable != 0;
    cable.debugCapable = properties->UsbPortProperties.PortIsDebugCapable != 0;
    cable.multipleCompanions = properties->UsbPortProperties.PortHasMultipleCompanions != 0;
    cable.companionPort = static_cast<int>(properties->CompanionPortNumber);

    size_t stringOffset = offsetof(USB_PORT_CONNECTOR_PROPERTIES, CompanionHubSymbolicLinkName);
    if (actualLength > stringOffset)
    {
        const wchar_t* name = reinterpret_cast<const wchar_t*>(buffer.data() + stringOffset);
        if (name[0] != L'\0')
            cable.companionHub = name;
    }

    // Only a real Type-C connector or a Billboard/Alt-Mode descriptor counts as
    // cable information.  A plain USB port (even when it has a companion port)
    // must not produce a "cable" group or placeholder text.
    cable.available = cable.typeC || !cable.billboard.empty();
    if (!cable.available)
        return;

    std::vector<std::wstring> parts;
    if (cable.typeC)
        parts.emplace_back(L"USB Type-C 连接器");
    if (cable.userConnectable)
        parts.emplace_back(L"外部可插拔");
    if (cable.debugCapable)
        parts.emplace_back(L"支持调试");
    if (cable.companionPort != 0)
        parts.emplace_back(L"与端口 #" + std::to_wstring(cable.companionPort) + L" 共用物理连接器");
    if (!cable.companionHub.empty())
        parts.emplace_back(L"共享 Hub: " + cable.companionHub);
    cable.summary = JoinStrings(parts, L"；");

    const std::wstring connectorSource = L"IOCTL_USB_GET_PORT_CONNECTOR_PROPERTIES";
    if (cable.source.empty())
        cable.source = connectorSource;
    else
        cable.source += L"；" + connectorSource;
}

std::wstring GetDriverKeyName(HANDLE hub, ULONG port)
{
    USB_NODE_CONNECTION_DRIVERKEY_NAME header = {};
    header.ConnectionIndex = port;
    DWORD returned = 0;
    if (!::DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
        &header, sizeof(header), &header, sizeof(header), &returned, nullptr))
        return std::wstring();
    if (header.ActualLength == 0 || header.ActualLength > 4096)
        return std::wstring();
    std::vector<BYTE> buffer(offsetof(USB_NODE_CONNECTION_DRIVERKEY_NAME, DriverKeyName) + header.ActualLength + sizeof(wchar_t));
    auto* name = reinterpret_cast<USB_NODE_CONNECTION_DRIVERKEY_NAME*>(buffer.data());
    name->ConnectionIndex = port;
    if (!::DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
        name, static_cast<DWORD>(buffer.size()), name, static_cast<DWORD>(buffer.size()), &returned, nullptr))
        return std::wstring();
    return name->DriverKeyName;
}

int FindNodeForPort(UsbSnapshot& snapshot, const std::wstring& hubInstanceId, ULONG port,
    const USB_DEVICE_DESCRIPTOR& descriptor, const std::wstring& driverKey)
{
    std::wstring normalized = NormalizeDriverKey(driverKey);
    if (!normalized.empty())
    {
        for (const auto& node : snapshot.nodes)
        {
            if (_wcsicmp(NormalizeDriverKey(node.driverKey).c_str(), normalized.c_str()) == 0)
                return node.id;
        }
    }

    for (const auto& node : snapshot.nodes)
    {
        if (_wcsicmp(node.parentInstanceId.c_str(), hubInstanceId.c_str()) == 0 &&
            node.hasAddress && node.address == static_cast<int>(port) && !node.isInterface)
            return node.id;
    }

    for (const auto& node : snapshot.nodes)
    {
        if (_wcsicmp(node.parentInstanceId.c_str(), hubInstanceId.c_str()) == 0 &&
            node.idVendor == descriptor.idVendor && node.idProduct == descriptor.idProduct && !node.isInterface)
            return node.id;
    }
    return -1;
}

void EnumerateHubs(UsbSnapshot& snapshot, std::atomic_bool* cancel,
    const std::function<void(const std::wstring&)>& progress)
{
    std::map<std::wstring, std::wstring, CaseInsensitiveLess> interfacePaths;
    HDEVINFO hubSet = ::SetupDiGetClassDevsW(&GUID_DEVINTERFACE_USB_HUB, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hubSet != INVALID_HANDLE_VALUE)
    {
        SP_DEVICE_INTERFACE_DATA interfaceData = {};
        interfaceData.cbSize = sizeof(interfaceData);
        for (DWORD index = 0; ::SetupDiEnumDeviceInterfaces(hubSet, nullptr, &GUID_DEVINTERFACE_USB_HUB, index, &interfaceData); ++index)
        {
            DWORD required = 0;
            ::SetupDiGetDeviceInterfaceDetailW(hubSet, &interfaceData, nullptr, 0, &required, nullptr);
            if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER || required == 0)
                continue;
            std::vector<BYTE> detailBuffer(required);
            auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detailBuffer.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            SP_DEVINFO_DATA deviceData = {};
            deviceData.cbSize = sizeof(deviceData);
            if (!::SetupDiGetDeviceInterfaceDetailW(hubSet, &interfaceData, detail, required, &required, &deviceData))
                continue;
            std::wstring instanceId = GetDeviceInstanceId(hubSet, deviceData);
            if (!instanceId.empty())
                interfacePaths[instanceId] = detail->DevicePath;
        }
        ::SetupDiDestroyDeviceInfoList(hubSet);
    }

    if (progress)
        progress(L"读取 HUB 端口与 USB 描述符…");

    std::vector<int> hubIds;
    for (const auto& node : snapshot.nodes)
    {
        if (node.isRootHub || node.isHub)
            hubIds.push_back(node.id);
    }

    for (int hubId : hubIds)
    {
        if (cancel != nullptr && cancel->load())
            return;
        UsbNode* hubNode = snapshot.Find(hubId);
        if (hubNode == nullptr)
            continue;
        auto pathIt = interfacePaths.find(hubNode->instanceId);
        if (pathIt == interfacePaths.end())
        {
            AddWarning(snapshot, L"无法打开 HUB 接口：" + hubNode->instanceId + L"（端口信息可能不完整）");
            continue;
        }

        HANDLE hub = ::CreateFileW(pathIt->second.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hub == INVALID_HANDLE_VALUE)
        {
            hub = ::CreateFileW(pathIt->second.c_str(), GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        }
        if (hub == INVALID_HANDLE_VALUE)
        {
            AddWarning(snapshot, L"无法打开 HUB 接口：" + hubNode->instanceId + L"（" + GetLastErrorMessage(::GetLastError()) + L"）");
            continue;
        }

        snapshot.hubAccessOk = true;

        USB_NODE_INFORMATION nodeInfo = {};
        DWORD returned = 0;
        if (::DeviceIoControl(hub, IOCTL_USB_GET_NODE_INFORMATION, &nodeInfo, sizeof(nodeInfo),
            &nodeInfo, sizeof(nodeInfo), &returned, nullptr))
        {
            hubNode->hubPorts = nodeInfo.u.HubInformation.HubDescriptor.bNumberOfPorts;
            hubNode->hubPower = L"上电延迟 " + std::to_wstring(nodeInfo.u.HubInformation.HubDescriptor.bPowerOnToPowerGood * 2) +
                L" ms，集线器电流 " + std::to_wstring(nodeInfo.u.HubInformation.HubDescriptor.bHubControlCurrent) + L" mA";
        }

        USB_HUB_INFORMATION_EX hubInfo = {};
        if (::DeviceIoControl(hub, IOCTL_USB_GET_HUB_INFORMATION_EX, &hubInfo, sizeof(hubInfo),
            &hubInfo, sizeof(hubInfo), &returned, nullptr))
        {
            hubNode->hubType = HubTypeText(hubInfo.HubType);
            if (hubInfo.HighestPortNumber > 0)
                hubNode->hubPorts = hubInfo.HighestPortNumber;
            if (hubInfo.HubType != Usb30Hub)
            {
                hubNode->hubPower = L"上电延迟 " + std::to_wstring(hubInfo.u.UsbHubDescriptor.bPowerOnToPowerGood * 2) +
                    L" ms，集线器电流 " + std::to_wstring(hubInfo.u.UsbHubDescriptor.bHubControlCurrent) + L" mA";
            }
        }
        else
        {
            hubNode->hubType = hubNode->isRootHub ? L"USB 根集线器" : L"USB Hub";
        }

        USB_HUB_CAPABILITIES_EX capabilities = {};
        if (::DeviceIoControl(hub, IOCTL_USB_GET_HUB_CAPABILITIES_EX, &capabilities, sizeof(capabilities),
            &capabilities, sizeof(capabilities), &returned, nullptr))
        {
            std::vector<std::wstring> flags;
            if (capabilities.CapabilityFlags.HubIsHighSpeedCapable)
                flags.emplace_back(L"HighSpeed 能力");
            if (capabilities.CapabilityFlags.HubIsHighSpeed)
                flags.emplace_back(L"HighSpeed 运行");
            if (capabilities.CapabilityFlags.HubIsMultiTtCapable)
                flags.emplace_back(L"Multi-TT 能力");
            if (capabilities.CapabilityFlags.HubIsMultiTt)
                flags.emplace_back(L"Multi-TT 运行");
            if (capabilities.CapabilityFlags.HubIsBusPowered)
                flags.emplace_back(L"总线供电");
            if (capabilities.CapabilityFlags.HubIsArmedWakeOnConnect)
                flags.emplace_back(L"连接唤醒");
            hubNode->hubCapabilities = JoinStrings(flags, L"；");
        }

        int typeCPorts = 0;
        for (ULONG port = 1; port <= static_cast<ULONG>(std::max(hubNode->hubPorts, 0)); ++port)
        {
            if (cancel != nullptr && cancel->load())
                break;

            USB_NODE_CONNECTION_INFORMATION_EX_V2 infoV2 = {};
            infoV2.ConnectionIndex = port;
            infoV2.Length = sizeof(infoV2);
            infoV2.SupportedUsbProtocols.Usb300 = 1;
            bool hasV2 = ::DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX_V2,
                &infoV2, sizeof(infoV2), &infoV2, sizeof(infoV2), &returned, nullptr) != FALSE;

            DWORD infoSize = sizeof(USB_NODE_CONNECTION_INFORMATION_EX) + sizeof(USB_PIPE_INFO) * 30;
            std::vector<BYTE> infoBuffer(infoSize);
            auto* info = reinterpret_cast<USB_NODE_CONNECTION_INFORMATION_EX*>(infoBuffer.data());
            info->ConnectionIndex = port;
            if (!::DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
                info, infoSize, info, infoSize, &returned, nullptr))
                continue;
            if (info->ConnectionStatus != DeviceConnected)
                continue;

            std::wstring driverKey = GetDriverKeyName(hub, port);
            int nodeId = FindNodeForPort(snapshot, hubNode->instanceId, port, info->DeviceDescriptor, driverKey);
            if (nodeId < 0)
            {
                AddWarning(snapshot, L"HUB 端口 " + std::to_wstring(port) + L" 上的设备未匹配到 PnP 节点（" +
                    hubNode->instanceId + L"）");
                continue;
            }

            UsbNode* device = snapshot.Find(nodeId);
            if (device == nullptr)
                continue;

            device->descriptorsValid = true;
            device->bcdUSB = info->DeviceDescriptor.bcdUSB;
            device->idVendor = info->DeviceDescriptor.idVendor;
            device->idProduct = info->DeviceDescriptor.idProduct;
            device->bcdDevice = info->DeviceDescriptor.bcdDevice;
            device->deviceClass = info->DeviceDescriptor.bDeviceClass;
            device->deviceSubClass = info->DeviceDescriptor.bDeviceSubClass;
            device->deviceProtocol = info->DeviceDescriptor.bDeviceProtocol;
            device->numConfigurations = info->DeviceDescriptor.bNumConfigurations;
            device->iManufacturer = info->DeviceDescriptor.iManufacturer;
            device->iProduct = info->DeviceDescriptor.iProduct;
            device->iSerialNumber = info->DeviceDescriptor.iSerialNumber;
            device->deviceClassName = UsbClassName(device->deviceClass);
            device->usbVersion = UsbProtocolName(device->bcdUSB);
            device->speedMbps = SpeedMbpsFromConnection(info->Speed, hasV2 ? &infoV2 : nullptr);
            device->speedText = FormatSpeed(device->speedMbps);
            device->protocolText = UsbProtocolName(device->bcdUSB);
            device->supportedProtocols = SupportedProtocolsText(hasV2 ? &infoV2 : nullptr);
            device->isHub = info->DeviceIsHub != FALSE || device->isRootHub;
            device->isComposite = device->deviceClass == 0xEF || device->isComposite;

            if (hasV2 && infoV2.Flags.DeviceIsOperatingAtSuperSpeedPlusOrHigher)
                device->speedMbps = 10000;
            else if (hasV2 && infoV2.Flags.DeviceIsOperatingAtSuperSpeedOrHigher)
                device->speedMbps = 5000;
            device->speedText = FormatSpeed(device->speedMbps);

            QueryConfigurationDescriptor(hub, port, *device);
            QueryBosDescriptor(hub, port, *device);
            FillCableFromConnector(hub, port, *device);

            device->usbManufacturer = QueryStringDescriptor(hub, port, info->DeviceDescriptor.iManufacturer);
            device->usbProduct = QueryStringDescriptor(hub, port, info->DeviceDescriptor.iProduct);
            device->usbSerial = QueryStringDescriptor(hub, port, info->DeviceDescriptor.iSerialNumber);

            if (device->isHub)
                hubNode->hubCapabilities += (hubNode->hubCapabilities.empty() ? L"" : L"；") + std::wstring(L"下游 Hub");
            if (device->cable.typeC)
                ++typeCPorts;
        }

        if (typeCPorts > 0)
        {
            hubNode->extraRows.push_back({ L"端口 / 线缆", L"USB Type-C 端口", std::to_wstring(typeCPorts) + L" 个" });
        }

        ::CloseHandle(hub);
    }
}

void AddCableNodes(UsbSnapshot& snapshot)
{
    const int count = static_cast<int>(snapshot.nodes.size());
    for (int i = 0; i < count; ++i)
    {
        const UsbNode* node = snapshot.Find(i);
        if (node == nullptr)
            continue;
        if (node->kind == NodeKind::Cable || node->kind == NodeKind::Storage || node->kind == NodeKind::Interface)
            continue;
        if (node->kind == NodeKind::Computer || node->kind == NodeKind::HostController)
            continue;
        // Never create a placeholder node: only real Type-C / Billboard data.
        if (!node->cable.available)
            continue;

        UsbNode cable;
        cable.kind = NodeKind::Cable;
        cable.parent = node->id;
        cable.instanceId = node->instanceId + L"#cable";
        cable.cable = node->cable;
        cable.shortName = L"端口/线缆";
        cable.expanded = false;
        AddNode(snapshot, std::move(cable));
    }
}

void FinalizeSnapshot(UsbSnapshot& snapshot)
{
    snapshot.generatedAt = CurrentTimestamp();
    for (auto& node : snapshot.nodes)
    {
        if (node.parent < 0 || node.parent >= static_cast<int>(snapshot.nodes.size()) || node.parent == node.id)
        {
            if (node.kind != NodeKind::Computer)
                node.parent = snapshot.computerId;
        }
    }

    snapshot.roots.clear();
    for (const auto& node : snapshot.nodes)
    {
        if (node.kind != NodeKind::Computer && node.parent == snapshot.computerId)
            snapshot.roots.push_back(node.id);
    }

    snapshot.hostCount = 0;
    snapshot.hubCount = 0;
    snapshot.deviceCount = 0;
    snapshot.storageCount = 0;
    snapshot.interfaceCount = 0;
    snapshot.anySmartAvailable = false;
    for (const auto& node : snapshot.nodes)
    {
        switch (node.kind)
        {
        case NodeKind::HostController: ++snapshot.hostCount; break;
        case NodeKind::RootHub:
        case NodeKind::Hub: ++snapshot.hubCount; ++snapshot.deviceCount; break;
        case NodeKind::Device: ++snapshot.deviceCount; break;
        case NodeKind::Interface: ++snapshot.interfaceCount; break;
        case NodeKind::Storage: ++snapshot.storageCount; break;
        case NodeKind::Usb4: ++snapshot.deviceCount; break;
        case NodeKind::Cable:
        case NodeKind::Computer: break;
        }
        if (node.storage.present && node.storage.smartAvailable)
            snapshot.anySmartAvailable = true;
    }

    for (auto& node : snapshot.nodes)
    {
        if (node.kind != NodeKind::Computer && node.title.empty())
            node.title = BuildNodeTitle(node);
    }
    if (!snapshot.nodes.empty() && snapshot.computerId >= 0)
        snapshot.nodes[static_cast<size_t>(snapshot.computerId)].title = L"本机 USB 拓扑";

    wchar_t summary[256] = {};
    swprintf_s(summary, L"主机控制器 %d · Hub %d · USB 设备 %d · 存储 %d",
        snapshot.hostCount, snapshot.hubCount, snapshot.deviceCount, snapshot.storageCount);
    snapshot.summary = summary;

    if (snapshot.deviceCount == 0)
        AddWarning(snapshot, L"未发现已连接的 USB 设备。请检查设备连接，或尝试以管理员身份运行。");
}
}

std::shared_ptr<UsbSnapshot> UsbEnumerator::Scan(std::atomic_bool* cancel,
    const std::function<void(const std::wstring&)>& progress)
{
    auto snapshot = std::make_shared<UsbSnapshot>();
    snapshot->elevated = IsProcessElevated();

    if (progress)
        progress(L"枚举 USB 主机控制器与设备…");

    UsbNode computer;
    computer.kind = NodeKind::Computer;
    computer.instanceId = L"<computer>";
    computer.title = L"本机 USB 拓扑";
    computer.shortName = L"计算机";
    snapshot->computerId = AddNode(*snapshot, std::move(computer));

    HDEVINFO set = ::SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE)
    {
        AddWarning(*snapshot, L"SetupAPI 枚举失败：" + GetLastErrorMessage(::GetLastError()));
        snapshot->setupApiOk = false;
        FinalizeSnapshot(*snapshot);
        return snapshot;
    }
    snapshot->setupApiOk = true;

    std::map<std::wstring, int, CaseInsensitiveLess> index;
    SP_DEVINFO_DATA deviceData = {};
    deviceData.cbSize = sizeof(deviceData);

    for (DWORD i = 0; ::SetupDiEnumDeviceInfo(set, i, &deviceData); ++i)
    {
        if (cancel != nullptr && cancel->load())
            break;

        std::wstring instanceId = GetDeviceInstanceId(set, deviceData);
        if (instanceId.empty())
            continue;

        GUID classGuid = {};
        std::wstring classGuidString;
        if (GetDevicePropertyGuid(set, deviceData, DEVPKEY_Device_ClassGuid, classGuid))
            classGuidString = GuidToString(classGuid);

        std::wstring service;
        GetDevicePropertyString(set, deviceData, DEVPKEY_Device_Service, service);
        if (service.empty())
            GetDeviceRegistryString(set, deviceData, SPDRP_SERVICE, service);

        bool isUsbEnumerator = StartsWithNoCase(instanceId, L"USB\\") || StartsWithNoCase(instanceId, L"USB4\\");
        bool isUsbHostController = StartsWithNoCase(instanceId, L"PCI\\") &&
            (_wcsicmp(classGuidString.c_str(), GuidToString(GUID_DEVCLASS_USB).c_str()) == 0 ||
             ContainsNoCase(service, L"USBXHCI") || ContainsNoCase(service, L"USBEHCI") ||
             ContainsNoCase(service, L"USBOHCI") || ContainsNoCase(service, L"USBUHCI"));
        if (!isUsbEnumerator && !isUsbHostController)
            continue;

        UsbNode node;
        FillDeviceProperties(set, deviceData, node);
        if (node.instanceId.empty())
            continue;
        if (node.isInterface)
            node.kind = NodeKind::Interface;
        else if (node.isHostController)
            node.kind = NodeKind::HostController;
        else if (node.isRootHub)
            node.kind = NodeKind::RootHub;
        else if (node.isUsb4)
            node.kind = NodeKind::Usb4;
        else if (node.isHub)
            node.kind = NodeKind::Hub;
        else
            node.kind = NodeKind::Device;

        int id = AddNode(*snapshot, std::move(node));
        index[snapshot->nodes[static_cast<size_t>(id)].instanceId] = id;
    }
    ::SetupDiDestroyDeviceInfoList(set);

    // Resolve parents.  Most USB parents are direct PnP parents, but root hubs
    // can be attached to a host controller that is not in the filtered set.
    for (auto& node : snapshot->nodes)
    {
        if (node.kind == NodeKind::Computer)
            continue;
        int parent = -1;
        auto it = index.find(node.parentInstanceId);
        if (it != index.end())
            parent = it->second;

        if (parent < 0 && node.isHostController)
            parent = snapshot->computerId;

        if (parent < 0 && node.isRootHub)
        {
            for (const auto& candidate : snapshot->nodes)
            {
                if (candidate.kind == NodeKind::HostController)
                {
                    parent = candidate.id;
                    break;
                }
            }
        }

        if (parent < 0 && !node.parentInstanceId.empty())
        {
            DEVINST devInst = 0;
            if (::CM_Locate_DevNodeW(&devInst, const_cast<wchar_t*>(node.instanceId.c_str()), CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS)
            {
                DEVINST current = devInst;
                for (int depth = 0; depth < 12; ++depth)
                {
                    DEVINST parentInst = 0;
                    if (::CM_Get_Parent(&parentInst, current, 0) != CR_SUCCESS)
                        break;
                    std::wstring parentId = GetDeviceId(parentInst);
                    auto parentIt = index.find(parentId);
                    if (parentIt != index.end())
                    {
                        parent = parentIt->second;
                        break;
                    }
                    current = parentInst;
                }
            }
        }

        if (parent < 0)
            parent = snapshot->computerId;
        if (parent != node.id)
            node.parent = parent;
    }

    // Mark composite parents when they have MI children.
    for (const auto& node : snapshot->nodes)
    {
        if (node.parent < 0)
            continue;
        UsbNode* parent = snapshot->Find(node.parent);
        if (parent != nullptr && node.isInterface)
            parent->isComposite = true;
    }

    // Detect USB storage from the presence of a USBSTOR/disk child.
    for (const auto& node : snapshot->nodes)
    {
        if (node.parent < 0)
            continue;
        UsbNode* parent = snapshot->Find(node.parent);
        if (parent == nullptr)
            continue;
        if (StartsWithNoCase(node.instanceId, L"USBSTOR\\") || ContainsNoCase(node.description, L"Mass Storage"))
            parent->isUsbStorage = true;
    }

    EnumerateHubs(*snapshot, cancel, progress);

    if (progress)
        progress(L"读取存储设备与容量信息…");
    EnumerateUsbStorage(*snapshot, cancel, progress);

    AddCableNodes(*snapshot);
    FinalizeSnapshot(*snapshot);

    if (!snapshot->elevated)
        AddWarning(*snapshot, L"标准用户：HUB 描述符、物理磁盘与 SMART 可能受限，可用“查看 → 以管理员身份重启”提权。");
    if (!snapshot->hubAccessOk)
        AddWarning(*snapshot, L"没有成功打开任何 USB HUB 接口，端口速度/描述符/线缆信息可能缺失。");

    return snapshot;
}
