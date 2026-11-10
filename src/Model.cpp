#include "pch.h"
#include "Model.h"
#include "DeviceProps.h"
#include "Language.h"

#include <cstdio>

namespace
{
std::wstring FormatMbpsShort(uint32_t mbps)
{
    switch (mbps)
    {
    case 1:     return L"1.5 Mbps";
    case 12:    return L"12 Mbps";
    case 480:   return L"480 Mbps";
    case 5000:  return L"5 Gbps";
    case 10000: return L"10 Gbps";
    case 20000: return L"20 Gbps";
    default:
        if (mbps == 0)
            return std::wstring();
        return std::to_wstring(mbps) + L" Mbps";
    }
}

std::wstring InterfaceNumberFromInstance(const std::wstring& instanceId)
{
    size_t position = ToUpperNoCase(instanceId).find(L"&MI_");
    if (position == std::wstring::npos || position + 7 > instanceId.size())
        return std::wstring();
    return instanceId.substr(position + 4, 2);
}

std::wstring ClassText(int classCode, int subClass, int protocol)
{
    wchar_t buffer[128] = {};
    swprintf_s(buffer, L"0x%02X (%s), %s 0x%02X, %s 0x%02X",
        classCode, UsbClassName(classCode).c_str(),
        Tr(L"子类"), subClass, Tr(L"协议"), protocol);
    return buffer;
}

std::wstring CableTitle(const CableInfo& cable)
{
    if (cable.typeC)
        return Tr(L"USB-C 连接器");
    if (!cable.billboard.empty())
        return L"Billboard / Alt Mode";
    if (cable.portNumber > 0)
        return std::wstring(Tr(L"端口 #")) + std::to_wstring(cable.portNumber);
    return Tr(L"线缆信息");
}

std::wstring StorageTitle(const StorageInfo& storage)
{
    std::wstring model = storage.model.empty() ? std::wstring(Tr(L"USB 存储设备")) : storage.model;
    if (storage.capacityBytes > 0)
        return std::wstring(Tr(L"存储")) + L" · " + model + L" (" + FormatBytesShort(storage.capacityBytes) + L")";
    return std::wstring(Tr(L"存储")) + L" · " + model;
}

constexpr size_t kMaxTreeTitleLength = 64;

std::wstring TrimTitle(std::wstring title)
{
    title = Trim(MakeSingleLine(title));
    if (title.size() > kMaxTreeTitleLength)
    {
        size_t cut = kMaxTreeTitleLength - 1;
        // Do not split a surrogate pair when truncating.
        if (cut > 0 && title[cut - 1] >= 0xD800 && title[cut - 1] <= 0xDBFF)
            --cut;
        title = title.substr(0, cut) + L"…";
    }
    return title;
}

// PnP descriptions often carry a driver-style suffix such as
// " - 1.20 (Microsoft)".  Tree titles keep only the short marketing part.
std::wstring ShortenDeviceName(std::wstring name)
{
    name = Trim(MakeSingleLine(name));
    for (size_t position = name.rfind(L" - ");
         position != std::wstring::npos;
         position = position == 0 ? std::wstring::npos : name.rfind(L" - ", position - 1))
    {
        std::wstring tail = name.substr(position + 3);
        if (!tail.empty() && tail[0] >= L'0' && tail[0] <= L'9')
        {
            name = Trim(name.substr(0, position));
            break;
        }
    }
    const std::wstring provider = L" (Microsoft)";
    if (name.size() > provider.size() &&
        _wcsicmp(name.c_str() + name.size() - provider.size(), provider.c_str()) == 0)
    {
        name = Trim(name.substr(0, name.size() - provider.size()));
    }
    return name;
}

std::wstring DeviceDisplayName(const UsbNode& node)
{
    if (!node.usbProduct.empty())
        return ShortenDeviceName(node.usbProduct);
    if (!node.friendlyName.empty())
        return ShortenDeviceName(node.friendlyName);
    if (!node.description.empty())
        return ShortenDeviceName(node.description);
    // Deliberately avoid VID/PID in the tree title; full IDs stay in BuildNodeRows.
    return Tr(L"USB 设备");
}

std::wstring ChipDisplay(const UsbNode& node)
{
    if (node.chipVendor.empty())
        return node.chipModel;
    if (node.chipModel.empty())
        return node.chipVendor;
    if (ContainsNoCase(node.chipModel, node.chipVendor))
        return node.chipModel;
    return node.chipVendor + L" " + node.chipModel;
}

void AddRow(std::vector<InfoRow>& rows, const wchar_t* group, const wchar_t* key, const std::wstring& value)
{
    if (value.empty())
        return;
    std::wstring cleaned = MakeSingleLine(value);
    if (cleaned.empty())
        return;
    // Group and key are fixed labels; values are only translated when the whole
    // value matches a table entry, so dynamic values pass through unchanged.
    rows.push_back({ Tr(group), Tr(key), Tr(cleaned) });
}

void AddRow(std::vector<InfoRow>& rows, const wchar_t* group, const wchar_t* key, const wchar_t* value)
{
    AddRow(rows, group, key, std::wstring(value == nullptr ? L"" : value));
}

std::wstring VolumeSummary(const VolumeInfo& volume)
{
    std::vector<std::wstring> parts;
    if (!volume.label.empty())
        parts.push_back(volume.label);
    if (!volume.fileSystem.empty())
        parts.push_back(volume.fileSystem);
    if (volume.totalBytes > 0)
        parts.push_back(FormatBytes(volume.totalBytes));
    if (volume.freeBytes > 0)
        parts.push_back(std::wstring(Tr(L"可用")) + L" " + FormatBytes(volume.freeBytes));
    if (volume.readOnly)
        parts.push_back(Tr(L"只读"));
    return JoinStrings(parts, Tr(L"，"));
}

std::wstring SmartRawValue(const SmartAttribute& attribute)
{
    if (attribute.id == 194 || attribute.id == 190)
    {
        uint64_t celsius = attribute.raw & 0xFF;
        if (celsius > 0 && celsius < 150)
            return std::to_wstring(celsius) + L" °C";
    }
    return attribute.rawText.empty() ? FormatNumber(attribute.raw) : attribute.rawText;
}

std::wstring NodeTreeText(const UsbSnapshot& snapshot, int nodeId, int depth)
{
    const UsbNode* node = snapshot.Find(nodeId);
    if (node == nullptr)
        return std::wstring();
    std::wstring result(static_cast<size_t>(depth) * 2, L' ');
    result += L"- " + node->title + L"\n";
    for (const auto& child : snapshot.nodes)
    {
        if (child.parent == nodeId)
            result += NodeTreeText(snapshot, child.id, depth + 1);
    }
    return result;
}
}

std::wstring BuildNodeTitle(const UsbNode& node)
{
    if (node.kind == NodeKind::Computer)
        return Tr(L"本机 USB 拓扑");

    if (node.kind == NodeKind::Storage)
        return TrimTitle(StorageTitle(node.storage));

    if (node.kind == NodeKind::Cable)
    {
        std::wstring title = std::wstring(Tr(L"线缆")) + L" · " + CableTitle(node.cable);
        if (node.cable.companionPort != 0)
            title += std::wstring(L" · ") + Tr(L"共用") + L" #" + std::to_wstring(node.cable.companionPort);
        return TrimTitle(title);
    }

    if (node.kind == NodeKind::HostController)
    {
        // Short form only: "USB 主机控制器 · Intel 500 Series Chipset xHCI".
        std::wstring title = Tr(L"USB 主机控制器");
        std::wstring chip;
        if (!node.chipModel.empty())
            chip = ChipDisplay(node);
        else if (!node.chipVendor.empty())
            chip = node.chipVendor;
        else
            chip = ShortenDeviceName(node.friendlyName.empty() ? node.description : node.friendlyName);
        if (ContainsNoCase(chip, L"未知"))
            chip.clear();
        if (!chip.empty() && !ContainsNoCase(title, chip))
            title += L" · " + chip;
        return TrimTitle(title);
    }

    if (node.kind == NodeKind::RootHub || node.kind == NodeKind::Hub)
    {
        // Hub titles only carry type / port count / short chip name.
        std::wstring type = node.hubType;
        if (type.empty())
            type = node.kind == NodeKind::RootHub ? std::wstring(Tr(L"USB 根集线器")) : std::wstring(Tr(L"USB Hub"));
        else
            type = Tr(type.c_str());
        std::wstring title = type;

        std::wstring chip;
        if (node.kind == NodeKind::RootHub)
            chip = !node.chipVendor.empty() ? node.chipVendor : node.chipModel;
        else
            chip = !node.chipModel.empty() ? node.chipModel : node.chipVendor;
        if (ContainsNoCase(chip, L"未知") || ContainsNoCase(chip, type))
            chip.clear();

        if (node.hubPorts > 0)
            title += std::wstring(L" · ") + std::to_wstring(node.hubPorts) + L" " + Tr(L"端口");
        if (!chip.empty() && !ContainsNoCase(title, chip))
            title += L" · " + chip;
        return TrimTitle(title);
    }

    if (node.kind == NodeKind::Interface)
    {
        // "接口 00 · 描述"
        std::wstring title = Tr(L"接口");
        std::wstring number = InterfaceNumberFromInstance(node.instanceId);
        if (!number.empty())
            title += L" " + number;
        std::wstring description = !node.description.empty() ? node.description : node.friendlyName;
        if (description.empty())
            description = node.model;
        description = ShortenDeviceName(description);
        if (!description.empty() && !ContainsNoCase(title, description))
            title += L" · " + description;
        return TrimTitle(title);
    }

    if (node.kind == NodeKind::Usb4)
    {
        return TrimTitle(std::wstring(Tr(L"USB4")) + L" · " + DeviceDisplayName(node));
    }

    std::wstring title;
    if (node.isUsbStorage && !node.storage.model.empty())
        title = ShortenDeviceName(node.storage.model);
    else
        title = DeviceDisplayName(node);
    if (node.isUsbStorage)
        title += std::wstring(L" · ") + Tr(L"存储");
    if (node.speedMbps > 0)
        title += L" · " + FormatMbpsShort(node.speedMbps);
    return TrimTitle(title);
}

std::vector<InfoRow> BuildNodeRows(const UsbNode& node)
{
    std::vector<InfoRow> rows;

    AddRow(rows, L"概要", L"名称", BuildNodeTitle(node));
    AddRow(rows, L"概要", L"类型", NodeKindName(node.kind));
    AddRow(rows, L"概要", L"状态", node.problem.empty() ? L"正常" : node.problem);
    AddRow(rows, L"概要", L"实例 ID", node.instanceId);
    AddRow(rows, L"概要", L"父设备 ID", node.parentInstanceId);
    AddRow(rows, L"概要", L"位置信息", node.locationInfo);
    AddRow(rows, L"概要", L"位置路径", node.locationPaths);
    if (node.hasAddress && !node.isHostController && node.kind != NodeKind::Computer)
        AddRow(rows, L"概要", L"端口地址", std::to_wstring(node.address));
    if (node.hasBusNumber)
        AddRow(rows, L"概要", L"总线号", std::to_wstring(node.busNumber));
    AddRow(rows, L"概要", L"容器 ID", node.containerId);
    AddRow(rows, L"概要", L"设备描述", node.description);
    AddRow(rows, L"概要", L"友好名称", node.friendlyName);
    AddRow(rows, L"概要", L"制造商", node.manufacturer);
    AddRow(rows, L"概要", L"型号", node.model);
    AddRow(rows, L"概要", L"总线报告描述", node.busReportedDesc);

    if (node.kind == NodeKind::Storage)
    {
        // Storage-specific rows are added below via node.storage.
    }
    else if (node.kind == NodeKind::Cable)
    {
        // Cable nodes are only created for real Type-C / Billboard data, so
        // every row here reflects something that was actually read.
        AddRow(rows, L"线缆 / 端口", L"端口号", node.cable.portNumber > 0 ? std::to_wstring(node.cable.portNumber) : L"");
        AddRow(rows, L"线缆 / 端口", L"连接器类型", node.cable.typeC ? L"USB Type-C" : L"");
        AddRow(rows, L"线缆 / 端口", L"可插拔", node.cable.userConnectable ? L"是" : L"");
        AddRow(rows, L"线缆 / 端口", L"调试能力", node.cable.debugCapable ? L"支持" : L"");
        AddRow(rows, L"线缆 / 端口", L"共享端口", node.cable.companionPort > 0 ? L"#" + std::to_wstring(node.cable.companionPort) : L"");
        AddRow(rows, L"线缆 / 端口", L"共享 Hub", node.cable.companionHub);
        AddRow(rows, L"线缆 / 端口", L"Billboard / Alt Mode", node.cable.billboard);
        AddRow(rows, L"线缆 / 端口", L"数据来源", node.cable.source);
        return rows;
    }
    else
    {
        if (node.descriptorsValid)
        {
            wchar_t vidPid[64] = {};
            swprintf_s(vidPid, L"VID_%04X / PID_%04X (0x%04X:0x%04X)", node.idVendor, node.idProduct, node.idVendor, node.idProduct);
            AddRow(rows, L"USB", L"VID / PID", vidPid);
            AddRow(rows, L"USB", L"厂商字符串", node.usbManufacturer);
            AddRow(rows, L"USB", L"产品字符串", node.usbProduct);
            AddRow(rows, L"USB", L"序列号", node.usbSerial);
            AddRow(rows, L"USB", L"USB 版本", node.usbVersion.empty() ? UsbProtocolName(node.bcdUSB) : node.usbVersion);
            AddRow(rows, L"USB", L"设备版本", FormatHex(node.bcdDevice));
            AddRow(rows, L"USB", L"设备类", ClassText(node.deviceClass, node.deviceSubClass, node.deviceProtocol));
            AddRow(rows, L"USB", L"配置数量", node.numConfigurations > 0 ? std::to_wstring(node.numConfigurations) : L"");
            AddRow(rows, L"USB", L"配置", node.configSummary);
            AddRow(rows, L"USB", L"BOS 能力", node.bosSummary);
            AddRow(rows, L"USB", L"速度", node.speedText);
            AddRow(rows, L"USB", L"协议", node.protocolText);
            AddRow(rows, L"USB", L"支持的协议", node.supportedProtocols);
            if (!node.interfaces.empty())
            {
                std::vector<std::wstring> interfaces;
                for (const auto& interfaceInfo : node.interfaces)
                    interfaces.push_back(interfaceInfo.text);
                AddRow(rows, L"USB", L"接口 / 端点", JoinStrings(interfaces, L"\n"));
            }
        }

        if (node.isHub || node.isRootHub)
        {
            AddRow(rows, L"HUB", L"Hub 类型", node.hubType);
            AddRow(rows, L"HUB", L"下行端口数", node.hubPorts > 0 ? std::to_wstring(node.hubPorts) : L"");
            AddRow(rows, L"HUB", L"电源参数", node.hubPower);
            AddRow(rows, L"HUB", L"Hub 能力", node.hubCapabilities);
            AddRow(rows, L"HUB", L"Hub 芯片", ChipDisplay(node));
            AddRow(rows, L"HUB", L"芯片来源", node.chipSource);
        }
        else
        {
            AddRow(rows, L"芯片", L"厂商/芯片", ChipDisplay(node));
            AddRow(rows, L"芯片", L"识别来源", node.chipSource);
        }

        // Only show the cable/connector group when a Type-C connector or a
        // Billboard descriptor was actually read for this device.
        if (node.kind != NodeKind::HostController && node.cable.available)
        {
            AddRow(rows, L"线缆 / 端口", L"端口号", node.hasAddress ? std::to_wstring(node.address) : L"");
            AddRow(rows, L"线缆 / 端口", L"连接器类型", node.cable.typeC ? L"USB Type-C" : L"");
            AddRow(rows, L"线缆 / 端口", L"可插拔", node.cable.userConnectable ? L"是" : L"");
            AddRow(rows, L"线缆 / 端口", L"调试能力", node.cable.debugCapable ? L"支持" : L"");
            AddRow(rows, L"线缆 / 端口", L"共享端口", node.cable.companionPort > 0 ? L"#" + std::to_wstring(node.cable.companionPort) : L"");
            AddRow(rows, L"线缆 / 端口", L"共享 Hub", node.cable.companionHub);
            AddRow(rows, L"线缆 / 端口", L"Billboard / Alt Mode", node.cable.billboard);
            AddRow(rows, L"线缆 / 端口", L"数据来源", node.cable.source);
        }
    }

    AddRow(rows, L"驱动", L"服务", node.service);
    AddRow(rows, L"驱动", L"设备类", node.className);
    AddRow(rows, L"驱动", L"驱动键", node.driverKey);
    AddRow(rows, L"驱动", L"驱动描述", node.driverDesc);
    AddRow(rows, L"驱动", L"驱动版本", node.driverVersion);
    AddRow(rows, L"驱动", L"驱动日期", FormatDateString(node.driverDate));
    AddRow(rows, L"驱动", L"驱动提供商", node.driverProvider);
    AddRow(rows, L"驱动", L"INF 文件", node.driverInf);
    AddRow(rows, L"驱动", L"驱动文件", node.driverFile);
    AddRow(rows, L"驱动", L"硬件 ID", node.hardwareIds);
    AddRow(rows, L"驱动", L"兼容 ID", node.compatibleIds);
    AddRow(rows, L"驱动", L"安装时间", node.installDate);
    AddRow(rows, L"驱动", L"最后到达", node.lastArrivalDate);
    AddRow(rows, L"驱动", L"问题代码", node.problem);
    AddRow(rows, L"驱动", L"设备能力", node.capabilities);
    AddRow(rows, L"驱动", L"配置标志", node.configFlags);

    if (node.storage.present)
    {
        const StorageInfo& storage = node.storage;
        AddRow(rows, L"存储", L"磁盘型号", storage.model);
        AddRow(rows, L"存储", L"厂商", storage.vendor);
        AddRow(rows, L"存储", L"固件/版本", storage.revision);
        AddRow(rows, L"存储", L"序列号", storage.serial);
        AddRow(rows, L"存储", L"容量", FormatBytes(storage.capacityBytes));
        AddRow(rows, L"存储", L"可用空间", FormatBytes(storage.freeBytes));
        AddRow(rows, L"存储", L"磁盘号", storage.diskNumber >= 0 ? std::to_wstring(storage.diskNumber) : L"");
        AddRow(rows, L"存储", L"磁盘路径", storage.diskPath);
        AddRow(rows, L"存储", L"总线类型", storage.busType);
        AddRow(rows, L"存储", L"可移动介质", storage.removable ? L"是" : L"否");
        AddRow(rows, L"存储", L"热插拔", storage.hotplug ? L"是" : L"未知");
        AddRow(rows, L"存储", L"分区样式", storage.partitionStyle);
        AddRow(rows, L"存储", L"逻辑扇区", storage.logicalSector ? std::to_wstring(storage.logicalSector) + L" " + Tr(L"字节") : L"");
        AddRow(rows, L"存储", L"物理扇区", storage.physicalSector ? std::to_wstring(storage.physicalSector) + L" " + Tr(L"字节") : L"");
        AddRow(rows, L"存储", L"桥接芯片厂商", storage.bridgeVendor);
        AddRow(rows, L"存储", L"桥接芯片", storage.bridgeChip);
        AddRow(rows, L"存储", L"主控/型号", storage.controllerChip);
        AddRow(rows, L"存储", L"存储颗粒", storage.flashChip);
        AddRow(rows, L"存储", L"健康状态", storage.health);
        AddRow(rows, L"存储", L"SMART 状态", storage.smartStatus);
        AddRow(rows, L"存储", L"SMART 说明", storage.smartMessage);
        for (size_t i = 0; i < storage.volumes.size(); ++i)
        {
            const VolumeInfo& volume = storage.volumes[i];
            std::wstring root = volume.rootPath.empty() ? volume.volumeName : volume.rootPath;
            AddRow(rows, L"存储", (std::wstring(Tr(L"卷")) + L" " + std::to_wstring(i + 1) + L" " + root).c_str(), VolumeSummary(volume));
        }
        if (!storage.smart.empty())
            AddRow(rows, L"SMART", L"属性数量", std::to_wstring(storage.smart.size()));
        for (const auto& extra : storage.smartExtra)
            AddRow(rows, L"SMART", extra.first.c_str(), extra.second);
    }

    for (const auto& extra : node.extraRows)
        AddRow(rows, extra.group.c_str(), extra.key.c_str(), extra.value);

    return rows;
}

std::wstring BuildNodePlainText(const UsbNode& node)
{
    std::wstring result = BuildNodeTitle(node) + L"\r\n";
    result.append(80, L'=');
    result += L"\r\n";
    std::wstring currentGroup;
    for (const auto& row : BuildNodeRows(node))
    {
        if (row.group != currentGroup)
        {
            result += L"\r\n[" + row.group + L"]\r\n";
            currentGroup = row.group;
        }
        result += L"  " + row.key + L": " + row.value + L"\r\n";
    }
    return result;
}

std::wstring BuildSnapshotSummary(const UsbSnapshot& snapshot)
{
    wchar_t summary[256] = {};
    swprintf_s(summary, Tr(L"主机控制器 %d · Hub %d · USB 设备 %d · 存储 %d"),
        snapshot.hostCount, snapshot.hubCount, snapshot.deviceCount, snapshot.storageCount);
    return summary;
}

void RefreshSnapshotText(UsbSnapshot& snapshot)
{
    for (auto& node : snapshot.nodes)
    {
        if (node.kind == NodeKind::Computer)
            node.title = Tr(L"本机 USB 拓扑");
        else
            node.title = BuildNodeTitle(node);
    }
    snapshot.summary = BuildSnapshotSummary(snapshot);
}

std::wstring BuildFullReport(const UsbSnapshot& snapshot)
{
    std::wstring report;
    report += std::wstring(Tr(L"USBInfo - USB 设备、HUB、存储与线缆信息报告")) + L"\r\n";
    report += std::wstring(Tr(L"生成时间")) + L": " + snapshot.generatedAt + L"\r\n";
    const wchar_t* role = snapshot.elevated ? Tr(L"管理员") : Tr(L"标准用户");
    report += std::wstring(Tr(L"运行权限")) + L": " + role + L"\r\n";
    report += std::wstring(Tr(L"系统")) + L": " + GetWindowsBuildString() + L"\r\n";
    report += std::wstring(Tr(L"摘要")) + L": " + snapshot.summary + L"\r\n";
    if (!snapshot.warnings.empty())
    {
        report += L"\r\n" + std::wstring(Tr(L"警告")) + L":\r\n";
        for (const auto& warning : snapshot.warnings)
            report += L"  - " + warning + L"\r\n";
    }

    report += L"\r\n";
    report.append(80, L'=');
    report += L"\r\n" + std::wstring(Tr(L"USB 拓扑树")) + L"\r\n";
    report.append(80, L'=');
    report += L"\r\n";
    if (snapshot.computerId >= 0)
        report += NodeTreeText(snapshot, snapshot.computerId, 0);

    report += L"\r\n";
    report.append(80, L'=');
    report += L"\r\n" + std::wstring(Tr(L"设备详情")) + L"\r\n";
    report.append(80, L'=');
    report += L"\r\n";
    for (const auto& node : snapshot.nodes)
    {
        report += L"\r\n" + BuildNodePlainText(node) + L"\r\n";
    }
    return report;
}
