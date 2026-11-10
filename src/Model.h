#pragma once

#include "pch.h"

enum class NodeKind
{
    Computer,
    HostController,
    RootHub,
    Hub,
    Device,
    Interface,
    Storage,
    Cable,
    Usb4
};

struct SmartAttribute
{
    int id = 0;
    std::wstring name;
    int current = 0;
    int worst = 0;
    int threshold = -1;
    uint64_t raw = 0;
    std::wstring rawText;
    std::wstring status;
};

struct VolumeInfo
{
    std::wstring volumeName;      // volume GUID path
    std::wstring mountPoints;     // "D:\; C:\mount\usb"
    std::wstring rootPath;        // "D:\" when available, otherwise the volume path
    std::wstring label;
    std::wstring fileSystem;
    uint64_t totalBytes = 0;
    uint64_t freeBytes = 0;
    bool readOnly = false;
    int diskNumber = -1;
};

struct CableInfo
{
    bool available = false;
    bool typeC = false;
    bool userConnectable = false;
    bool debugCapable = false;
    bool multipleCompanions = false;
    int portNumber = 0;
    int companionPort = 0;
    std::wstring companionHub;
    std::wstring billboard;
    std::wstring summary;
    std::wstring source;
    std::wstring limitation;
};

struct StorageInfo
{
    bool present = false;
    bool rawAccess = false;
    bool smartAvailable = false;
    bool smartQueried = false;
    bool removable = false;
    bool hotplug = false;
    int diskNumber = -1;
    std::wstring diskPath;
    std::wstring pnpInstance;
    std::wstring vendor;
    std::wstring model;
    std::wstring revision;
    std::wstring serial;
    std::wstring adapter;
    std::wstring busType;
    uint64_t capacityBytes = 0;
    uint64_t freeBytes = 0;
    DWORD logicalSector = 0;
    DWORD physicalSector = 0;
    std::wstring partitionStyle;
    std::wstring health;
    std::wstring smartStatus;
    std::wstring smartMessage;
    std::wstring bridgeVendor;
    std::wstring bridgeChip;
    std::wstring controllerChip;
    std::wstring flashChip;
    std::vector<VolumeInfo> volumes;
    std::vector<SmartAttribute> smart;
    std::vector<std::pair<std::wstring, std::wstring>> smartExtra;
};

struct UsbInterfaceInfo
{
    int number = 0;
    int alternate = 0;
    int classCode = 0;
    int subClass = 0;
    int protocol = 0;
    int endpoints = 0;
    std::wstring text;
};

struct InfoRow
{
    std::wstring group;
    std::wstring key;
    std::wstring value;
};

struct UsbNode
{
    int id = -1;
    int parent = -1;
    NodeKind kind = NodeKind::Device;
    bool expanded = true;
    bool connected = true;

    std::wstring instanceId;
    std::wstring parentInstanceId;
    std::wstring title;
    std::wstring shortName;
    std::wstring description;
    std::wstring friendlyName;
    std::wstring manufacturer;
    std::wstring model;
    std::wstring service;
    std::wstring className;
    std::wstring classGuid;
    std::wstring enumerator;
    std::wstring locationInfo;
    std::wstring locationPaths;
    std::wstring driverKey;
    std::wstring driverDesc;
    std::wstring driverVersion;
    std::wstring driverDate;
    std::wstring driverProvider;
    std::wstring driverInf;
    std::wstring driverFile;
    std::wstring driverFileKind;
    std::wstring driverRank;
    std::wstring hardwareIds;
    std::wstring compatibleIds;
    std::wstring containerId;
    std::wstring busReportedDesc;
    std::wstring problem;
    std::wstring capabilities;
    std::wstring configFlags;
    std::wstring installDate;
    std::wstring lastArrivalDate;
    int address = 0;
    int busNumber = 0;
    bool hasAddress = false;
    bool hasBusNumber = false;

    bool isHostController = false;
    bool isRootHub = false;
    bool isHub = false;
    bool isUsbStorage = false;
    bool isComposite = false;
    bool isInterface = false;
    bool isUsb4 = false;

    bool descriptorsValid = false;
    uint16_t bcdUSB = 0;
    uint16_t idVendor = 0;
    uint16_t idProduct = 0;
    uint16_t bcdDevice = 0;
    int deviceClass = 0;
    int deviceSubClass = 0;
    int deviceProtocol = 0;
    int numConfigurations = 0;
    int iManufacturer = 0;
    int iProduct = 0;
    int iSerialNumber = 0;
    std::wstring usbManufacturer;
    std::wstring usbProduct;
    std::wstring usbSerial;
    std::wstring usbVersion;
    std::wstring deviceClassName;
    std::wstring configSummary;
    std::wstring bosSummary;
    std::vector<UsbInterfaceInfo> interfaces;

    std::wstring speedText;
    std::wstring protocolText;
    std::wstring supportedProtocols;
    uint32_t speedMbps = 0;

    std::wstring chipVendor;
    std::wstring chipModel;
    std::wstring chipSource;
    std::wstring hubType;
    int hubPorts = 0;
    std::wstring hubPower;
    std::wstring hubCapabilities;

    CableInfo cable;
    StorageInfo storage;
    std::vector<InfoRow> extraRows;
};

struct UsbSnapshot
{
    std::wstring generatedAt;
    bool elevated = false;
    bool setupApiOk = false;
    bool hubAccessOk = false;
    bool anySmartAvailable = false;
    int hostCount = 0;
    int hubCount = 0;
    int deviceCount = 0;
    int storageCount = 0;
    int interfaceCount = 0;
    int computerId = -1;
    std::wstring summary;
    std::vector<UsbNode> nodes;
    std::vector<int> roots;
    std::vector<std::wstring> warnings;

    const UsbNode* Find(int id) const
    {
        if (id < 0 || static_cast<size_t>(id) >= nodes.size())
            return nullptr;
        return &nodes[static_cast<size_t>(id)];
    }

    UsbNode* Find(int id)
    {
        if (id < 0 || static_cast<size_t>(id) >= nodes.size())
            return nullptr;
        return &nodes[static_cast<size_t>(id)];
    }

    const UsbNode* FindByInstance(const std::wstring& instanceId) const
    {
        for (const auto& node : nodes)
        {
            if (_wcsicmp(node.instanceId.c_str(), instanceId.c_str()) == 0)
                return &node;
        }
        return nullptr;
    }

    UsbNode* FindByInstance(const std::wstring& instanceId)
    {
        for (auto& node : nodes)
        {
            if (_wcsicmp(node.instanceId.c_str(), instanceId.c_str()) == 0)
                return &node;
        }
        return nullptr;
    }

    const UsbNode* StorageNodeFor(int nodeId) const
    {
        const UsbNode* node = Find(nodeId);
        if (node == nullptr)
            return nullptr;
        if (node->storage.present && node->kind != NodeKind::Storage)
            return node;
        if (node->kind == NodeKind::Storage && node->storage.present)
            return node;
        if (node->parent >= 0)
        {
            const UsbNode* parent = Find(node->parent);
            if (parent != nullptr && parent->storage.present)
                return parent;
        }
        return nullptr;
    }
};

std::wstring NodeKindName(NodeKind kind);
std::wstring NodeKindShortName(NodeKind kind);
std::wstring BuildNodeTitle(const UsbNode& node);
std::vector<InfoRow> BuildNodeRows(const UsbNode& node);
std::wstring BuildNodePlainText(const UsbNode& node);
std::wstring BuildSnapshotSummary(const UsbSnapshot& snapshot);
// Rebuilds the cached tree titles and the status summary in the current UI
// language (used after a language switch without a rescan).
void RefreshSnapshotText(UsbSnapshot& snapshot);
std::wstring BuildFullReport(const UsbSnapshot& snapshot);

std::wstring UsbClassName(int classCode);
std::wstring UsbProtocolName(uint16_t bcdUSB);
std::wstring FormatBytes(uint64_t bytes);
std::wstring FormatBytesShort(uint64_t bytes);
std::wstring FormatSpeed(uint32_t mbps);
std::wstring FormatNumber(uint64_t value);
std::wstring FormatDateString(const std::wstring& text);
std::wstring FormatHex(uint32_t value, int width = 4);
std::wstring NormalizeDeviceText(const std::wstring& text);
std::wstring GetLastErrorMessage(DWORD error);
std::wstring GetFileTimeString(const FILETIME& ft);
std::wstring Trim(const std::wstring& text);
std::wstring JoinStrings(const std::vector<std::wstring>& items, const wchar_t* separator);
bool StartsWithNoCase(const std::wstring& value, const std::wstring& prefix);
bool ContainsNoCase(const std::wstring& value, const std::wstring& needle);
std::wstring ToUpperNoCase(const std::wstring& value);
std::wstring ReplaceAll(std::wstring text, const std::wstring& from, const std::wstring& to);
std::wstring MakeSingleLine(const std::wstring& text);
std::wstring IndentText(const std::wstring& text, int spaces);

std::wstring CsvEscape(const std::wstring& text);
