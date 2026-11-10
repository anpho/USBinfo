#include "pch.h"
#include "StorageEnumerator.h"
#include "DeviceProps.h"
#include "ChipDb.h"
#include "Smart.h"
#include "Language.h"

#include <initguid.h>
#include <devguid.h>
#include <devpkey.h>
#include <usbiodef.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <winioctl.h>
#include <ntddscsi.h>

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

struct DiskRecord
{
    int number = -1;
    std::wstring instanceId;
    std::wstring parentId;
    std::wstring usbAncestor;
    std::wstring containerId;
    std::wstring friendly;
    std::wstring description;
    std::wstring vendor;
    std::wstring model;
    std::wstring revision;
    std::wstring serial;
    std::wstring busType;
    uint64_t size = 0;
    bool removable = false;
    bool rawAccess = false;
    StorageInfo storage;
    std::vector<VolumeInfo> volumes;
};

std::wstring GuidToString(const GUID& guid)
{
    wchar_t buffer[64] = {};
    if (::StringFromGUID2(guid, buffer, _countof(buffer)) <= 0)
        return std::wstring();
    return buffer;
}

std::wstring TrimTrailingSlash(std::wstring value)
{
    while (!value.empty() && (value.back() == L'\\' || value.back() == L'/'))
        value.pop_back();
    return value;
}

std::vector<std::wstring> ParseMultiString(const wchar_t* value)
{
    std::vector<std::wstring> result;
    if (value == nullptr)
        return result;
    const wchar_t* current = value;
    while (*current != L'\0')
    {
        std::wstring item(current);
        if (!item.empty())
            result.push_back(item);
        current += item.size() + 1;
    }
    return result;
}

std::wstring JoinVolumePaths(const std::vector<std::wstring>& paths)
{
    std::vector<std::wstring> cleaned;
    for (const auto& path : paths)
        cleaned.push_back(TrimTrailingSlash(path) + L"\\");
    return JoinStrings(cleaned, L"; ");
}

std::wstring FindDrivePath(const std::vector<std::wstring>& paths)
{
    for (const auto& path : paths)
    {
        if (path.size() == 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/'))
            return path;
    }
    return std::wstring();
}

std::wstring OpenPathForVolume(const std::wstring& volumeName, const std::vector<std::wstring>& paths)
{
    std::wstring drive = FindDrivePath(paths);
    if (!drive.empty())
        return L"\\\\.\\" + drive.substr(0, 2);
    return volumeName;
}

void FillStorageMetadata(const std::wstring& instanceId, StorageInfo& storage)
{
    storage.pnpInstance = instanceId;

    // USBSTOR\Disk&Ven_SanDisk&Prod_Extreme_Pro&Rev_1.00\...
    std::wstring upper = instanceId;
    size_t diskPos = upper.find(L"Disk&Ven_");
    if (diskPos == std::wstring::npos)
        return;
    size_t vendorBegin = diskPos + 9;
    size_t vendorEnd = upper.find(L"&Prod_", vendorBegin);
    size_t productEnd = upper.find(L"&Rev_", vendorEnd == std::wstring::npos ? vendorBegin : vendorEnd);
    if (vendorEnd == std::wstring::npos)
        return;
    auto clean = [](std::wstring text)
    {
        std::replace(text.begin(), text.end(), L'_', L' ');
        return Trim(text);
    };
    storage.vendor = clean(instanceId.substr(vendorBegin, vendorEnd - vendorBegin));
    if (productEnd != std::wstring::npos)
    {
        storage.model = clean(instanceId.substr(vendorEnd + 6, productEnd - (vendorEnd + 6)));
        size_t revisionEnd = instanceId.find(L'\\', productEnd);
        storage.revision = clean(instanceId.substr(productEnd + 5, revisionEnd == std::wstring::npos ? std::wstring::npos : revisionEnd - (productEnd + 5)));
    }
    else
    {
        storage.model = clean(instanceId.substr(vendorEnd + 6));
    }
}

bool QueryStorageDescriptor(HANDLE disk, StorageInfo& storage)
{
    STORAGE_PROPERTY_QUERY query = {};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    DWORD required = 0;
    if (!::DeviceIoControl(disk, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), nullptr, 0, &required, nullptr) ||
        required == 0)
    {
        return false;
    }
    std::vector<BYTE> buffer(required + 16);
    if (!::DeviceIoControl(disk, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
        buffer.data(), static_cast<DWORD>(buffer.size()), &required, nullptr))
        return false;

    auto* descriptor = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());
    const size_t available = std::min<size_t>(required, buffer.size());
    auto getString = [&buffer, available](DWORD offset) -> std::wstring
    {
        if (offset == 0)
            return std::wstring();
        return ReadBoundedWideString(buffer.data(), available, offset);
    };
    std::wstring vendor = getString(descriptor->VendorIdOffset);
    std::wstring product = getString(descriptor->ProductIdOffset);
    std::wstring revision = getString(descriptor->ProductRevisionOffset);
    std::wstring serial = getString(descriptor->SerialNumberOffset);
    if (!vendor.empty())
        storage.vendor = Trim(vendor);
    if (!product.empty())
        storage.model = Trim(product);
    if (!revision.empty())
        storage.revision = Trim(revision);
    if (!serial.empty())
        storage.serial = Trim(serial);
    storage.busType = StorageBusTypeName(descriptor->BusType);
    storage.removable = descriptor->RemovableMedia != FALSE;
    return true;
}

bool QueryLength(HANDLE disk, StorageInfo& storage)
{
    GET_LENGTH_INFORMATION length = {};
    DWORD returned = 0;
    if (::DeviceIoControl(disk, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &length, sizeof(length), &returned, nullptr))
    {
        storage.capacityBytes = length.Length.QuadPart;
        return true;
    }
    return false;
}

bool QueryGeometry(HANDLE disk, StorageInfo& storage)
{
    BYTE buffer[sizeof(DISK_GEOMETRY_EX) + 512] = {};
    DWORD returned = 0;
    if (!::DeviceIoControl(disk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, buffer, sizeof(buffer), &returned, nullptr))
        return false;
    auto* geometry = reinterpret_cast<DISK_GEOMETRY_EX*>(buffer);
    storage.logicalSector = geometry->Geometry.BytesPerSector;
    if (geometry->DiskSize.QuadPart > 0)
        storage.capacityBytes = geometry->DiskSize.QuadPart;
    return true;
}

bool QueryAlignment(HANDLE disk, StorageInfo& storage)
{
    STORAGE_PROPERTY_QUERY query = {};
    query.PropertyId = StorageAccessAlignmentProperty;
    query.QueryType = PropertyStandardQuery;
    STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR alignment = {};
    DWORD returned = 0;
    if (!::DeviceIoControl(disk, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
        &alignment, sizeof(alignment), &returned, nullptr))
        return false;
    storage.physicalSector = alignment.BytesPerPhysicalSector;
    return true;
}

bool QueryHotplug(HANDLE disk, StorageInfo& storage)
{
    STORAGE_HOTPLUG_INFO hotplug = {};
    DWORD returned = 0;
    if (!::DeviceIoControl(disk, IOCTL_STORAGE_GET_HOTPLUG_INFO, nullptr, 0, &hotplug, sizeof(hotplug), &returned, nullptr))
        return false;
    storage.removable = storage.removable || hotplug.MediaRemovable != FALSE;
    storage.hotplug = hotplug.DeviceHotplug != FALSE;
    return true;
}

bool QueryPartitionStyle(HANDLE disk, StorageInfo& storage)
{
    std::vector<BYTE> buffer(64 * 1024);
    DWORD returned = 0;
    if (!::DeviceIoControl(disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, nullptr, 0,
        buffer.data(), static_cast<DWORD>(buffer.size()), &returned, nullptr))
        return false;
    auto* layout = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX*>(buffer.data());
    switch (layout->PartitionStyle)
    {
    case PARTITION_STYLE_MBR: storage.partitionStyle = L"MBR"; break;
    case PARTITION_STYLE_GPT: storage.partitionStyle = L"GPT"; break;
    case PARTITION_STYLE_RAW: storage.partitionStyle = L"RAW"; break;
    default:                  storage.partitionStyle = Tr(L"未知"); break;
    }
    return true;
}

HANDLE OpenPhysicalDrive(int number, bool& readAccess)
{
    readAccess = false;
    if (number < 0)
        return INVALID_HANDLE_VALUE;
    wchar_t path[64] = {};
    swprintf_s(path, L"\\\\.\\PhysicalDrive%d", number);
    HANDLE handle = ::CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle != INVALID_HANDLE_VALUE)
    {
        readAccess = true;
        return handle;
    }
    handle = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle != INVALID_HANDLE_VALUE)
    {
        readAccess = true;
        return handle;
    }
    handle = ::CreateFileW(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    return handle;
}

std::wstring UsbAncestorOf(const std::wstring& instanceId)
{
    DEVINST devInst = 0;
    if (::CM_Locate_DevNodeW(&devInst, const_cast<wchar_t*>(instanceId.c_str()), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS)
        return std::wstring();
    DEVINST current = devInst;
    for (int depth = 0; depth < 12; ++depth)
    {
        DEVINST parent = 0;
        if (::CM_Get_Parent(&parent, current, 0) != CR_SUCCESS)
            break;
        std::wstring id = GetDeviceId(parent);
        if (StartsWithNoCase(id, L"USB\\") || StartsWithNoCase(id, L"USB4\\"))
            return id;
        current = parent;
    }
    return std::wstring();
}

std::map<std::wstring, int, CaseInsensitiveLess> ReadDiskNumberMap()
{
    std::map<std::wstring, int, CaseInsensitiveLess> result;
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\disk\\Enum", 0, KEY_READ, &key) != ERROR_SUCCESS)
        return result;
    for (DWORD index = 0; index < 256; ++index)
    {
        wchar_t name[64] = {};
        DWORD nameLength = _countof(name);
        std::vector<BYTE> data(1024, 0);
        DWORD dataLength = static_cast<DWORD>(data.size());
        DWORD type = 0;
        LONG status = ::RegEnumValueW(key, index, name, &nameLength, nullptr, &type, data.data(), &dataLength);
        if (status == ERROR_MORE_DATA)
        {
            data.resize(dataLength);
            status = ::RegEnumValueW(key, index, name, &nameLength, nullptr, &type, data.data(), &dataLength);
        }
        if (status != ERROR_SUCCESS)
            break;
        if ((type != REG_SZ && type != REG_EXPAND_SZ) || dataLength < sizeof(wchar_t))
            continue;
        std::wstring valueText = ReadBoundedWideString(data.data(), dataLength, 0);
        int number = _wtoi(name);
        if (number >= 0 && !valueText.empty())
            result[valueText] = number;
    }
    ::RegCloseKey(key);
    return result;
}

void EnumerateVolumes(std::vector<DiskRecord>& disks)
{
    std::map<int, size_t> diskIndex;
    for (size_t i = 0; i < disks.size(); ++i)
        diskIndex[disks[i].number] = i;

    wchar_t volumeName[MAX_PATH] = {};
    HANDLE find = ::FindFirstVolumeW(volumeName, _countof(volumeName));
    if (find == INVALID_HANDLE_VALUE)
        return;

    do
    {
        VolumeInfo volume;
        volume.volumeName = volumeName;
        std::vector<wchar_t> pathBuffer;
        std::vector<std::wstring> pathList;
        DWORD pathLength = 0;
        ::GetVolumePathNamesForVolumeNameW(volumeName, nullptr, 0, &pathLength);
        if (pathLength > 0)
        {
            pathBuffer.resize(pathLength + 1);
            if (::GetVolumePathNamesForVolumeNameW(volumeName, pathBuffer.data(),
                static_cast<DWORD>(pathBuffer.size()), &pathLength))
                pathList = ParseMultiString(pathBuffer.data());
        }
        volume.mountPoints = JoinVolumePaths(pathList);
        std::wstring drivePath = FindDrivePath(pathList);
        volume.rootPath = !drivePath.empty() ? drivePath : volumeName;

        wchar_t label[256] = {};
        wchar_t fileSystem[64] = {};
        DWORD serial = 0;
        DWORD maxComponent = 0;
        DWORD flags = 0;
        if (::GetVolumeInformationW(volumeName, label, _countof(label), &serial,
            &maxComponent, &flags, fileSystem, _countof(fileSystem)))
        {
            volume.label = label;
            volume.fileSystem = fileSystem;
            volume.readOnly = (flags & FILE_READ_ONLY_VOLUME) != 0;
        }

        ULARGE_INTEGER freeAvailable = {};
        ULARGE_INTEGER total = {};
        ULARGE_INTEGER totalFree = {};
        if (::GetDiskFreeSpaceExW(volume.rootPath.c_str(), &freeAvailable, &total, &totalFree))
        {
            volume.totalBytes = total.QuadPart;
            volume.freeBytes = freeAvailable.QuadPart;
        }

        HANDLE handle = ::CreateFileW(OpenPathForVolume(volumeName, pathList).c_str(), 0,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            handle = ::CreateFileW(volumeName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, 0, nullptr);
        }
        if (handle != INVALID_HANDLE_VALUE)
        {
            std::vector<BYTE> buffer(sizeof(VOLUME_DISK_EXTENTS) + sizeof(DISK_EXTENT) * 16);
            DWORD returned = 0;
            if (::DeviceIoControl(handle, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0,
                buffer.data(), static_cast<DWORD>(buffer.size()), &returned, nullptr))
            {
                auto* extents = reinterpret_cast<VOLUME_DISK_EXTENTS*>(buffer.data());
                if (extents->NumberOfDiskExtents > 0)
                    volume.diskNumber = static_cast<int>(extents->Extents[0].DiskNumber);
            }
            ::CloseHandle(handle);
        }

        auto it = diskIndex.find(volume.diskNumber);
        if (it != diskIndex.end())
            disks[it->second].volumes.push_back(volume);
    } while (::FindNextVolumeW(find, volumeName, _countof(volumeName)));
    ::FindVolumeClose(find);
}

void AttachStorageToNode(UsbSnapshot& snapshot, UsbNode& node, const StorageInfo& storage)
{
    node.isUsbStorage = true;
    if (!node.storage.present)
    {
        node.storage = storage;
        node.storage.present = true;
    }

    UsbNode child;
    child.kind = NodeKind::Storage;
    child.parent = node.id;
    child.instanceId = storage.pnpInstance.empty() ? (node.instanceId + L"#storage") : storage.pnpInstance;
    child.storage = storage;
    child.storage.present = true;
    child.shortName = Tr(L"存储");
    child.description = storage.model;
    child.expanded = false;
    child.storage.bridgeVendor = node.chipVendor;
    child.storage.bridgeChip = node.chipModel;
    if (child.storage.bridgeChip.empty() && node.idProduct != 0)
    {
        wchar_t buffer[64] = {};
        swprintf_s(buffer, Tr(L"USB 桥接芯片 (VID_%04X:PID_%04X)"), node.idVendor, node.idProduct);
        child.storage.bridgeChip = buffer;
    }
    if (child.storage.flashChip.empty())
        child.storage.flashChip = Tr(L"无法读取（USB 桥接器不暴露 NAND/闪存颗粒型号）");
    if (child.storage.controllerChip.empty())
        child.storage.controllerChip = child.storage.model;

    int childId = static_cast<int>(snapshot.nodes.size());
    child.id = childId;
    snapshot.nodes.push_back(std::move(child));
}

std::wstring StorageModelFromFriendly(const std::wstring& friendly)
{
    std::wstring model = friendly;
    const std::wstring suffix = L" USB Device";
    if (model.size() >= suffix.size() && _wcsicmp(model.c_str() + model.size() - suffix.size(), suffix.c_str()) == 0)
        model = model.substr(0, model.size() - suffix.size());
    return Trim(model);
}
}

void EnumerateUsbStorage(UsbSnapshot& snapshot, std::atomic_bool* cancel,
    const std::function<void(const std::wstring&)>& progress)
{
    if (cancel != nullptr && cancel->load())
        return;

    std::vector<DiskRecord> disks;
    std::map<std::wstring, int, CaseInsensitiveLess> diskNumbers = ReadDiskNumberMap();

    HDEVINFO diskSet = ::SetupDiGetClassDevsW(&GUID_DEVCLASS_DISKDRIVE, nullptr, nullptr, DIGCF_PRESENT);
    if (diskSet != INVALID_HANDLE_VALUE)
    {
        SP_DEVINFO_DATA deviceData = {};
        deviceData.cbSize = sizeof(deviceData);
        for (DWORD i = 0; ::SetupDiEnumDeviceInfo(diskSet, i, &deviceData); ++i)
        {
            if (cancel != nullptr && cancel->load())
                break;
            std::wstring instanceId = GetDeviceInstanceId(diskSet, deviceData);
            if (instanceId.empty())
                continue;

            std::wstring usbAncestor = UsbAncestorOf(instanceId);
            if (usbAncestor.empty() && !StartsWithNoCase(instanceId, L"USBSTOR\\"))
                continue;

            DiskRecord disk;
            disk.instanceId = instanceId;
            disk.usbAncestor = usbAncestor;
            disk.parentId = GetDeviceParentId(deviceData.DevInst);

            std::wstring value;
            if (GetDevicePropertyString(diskSet, deviceData, DEVPKEY_Device_FriendlyName, value))
                disk.friendly = NormalizeDeviceText(value);
            if (disk.friendly.empty() && GetDevicePropertyString(diskSet, deviceData, DEVPKEY_Device_DeviceDesc, value))
                disk.friendly = NormalizeDeviceText(value);

            GUID container = {};
            if (GetDevicePropertyGuid(diskSet, deviceData, DEVPKEY_Device_ContainerId, container))
                disk.containerId = GuidToString(container);

            auto numberIt = diskNumbers.find(instanceId);
            if (numberIt != diskNumbers.end())
                disk.number = numberIt->second;

            FillStorageMetadata(instanceId, disk.storage);
            if (disk.storage.model.empty())
                disk.storage.model = StorageModelFromFriendly(disk.friendly);

            if (disk.number >= 0)
            {
                disk.storage.diskNumber = disk.number;
                disk.storage.diskPath = L"\\\\.\\PhysicalDrive" + std::to_wstring(disk.number);
                bool readAccess = false;
                HANDLE handle = OpenPhysicalDrive(disk.number, readAccess);
                if (handle != INVALID_HANDLE_VALUE)
                {
                    disk.storage.rawAccess = readAccess;
                    QueryStorageDescriptor(handle, disk.storage);
                    QueryLength(handle, disk.storage);
                    QueryGeometry(handle, disk.storage);
                    QueryAlignment(handle, disk.storage);
                    QueryHotplug(handle, disk.storage);
                    QueryPartitionStyle(handle, disk.storage);
                    disk.storage.busType = L"USB";
                    disk.storage.removable = true;
                    if (readAccess)
                    {
                        ReadAtaIdentify(handle, disk.storage);
                        ReadSmartData(handle, disk.storage);
                    }
                    else
                    {
                        disk.storage.smartQueried = true;
                        disk.storage.smartStatus = Tr(L"不可用");
                        disk.storage.smartMessage = Tr(L"无法以读写方式打开物理磁盘；SMART 需要管理员权限。");
                    }
                    ::CloseHandle(handle);
                }
                else
                {
                    wchar_t message[1024] = {};
                    swprintf_s(message, Tr(L"无法打开物理磁盘：%s。容量和卷信息仍可通过卷 API 获取。"),
                        GetLastErrorMessage(::GetLastError()).c_str());
                    disk.storage.smartQueried = true;
                    disk.storage.smartStatus = Tr(L"不可用");
                    disk.storage.smartMessage = message;
                }
            }

            if (disk.storage.model.empty())
                disk.storage.model = Tr(L"USB 存储设备");
            disk.storage.present = true;
            if (disk.storage.busType.empty())
                disk.storage.busType = L"USB";
            disk.storage.removable = true;
            disks.push_back(std::move(disk));
        }
        ::SetupDiDestroyDeviceInfoList(diskSet);
    }

    if (progress)
        progress(Tr(L"关联卷、容量与文件系统…"));
    EnumerateVolumes(disks);

    for (auto& disk : disks)
    {
        if (cancel != nullptr && cancel->load())
            break;

        uint64_t volumeTotal = 0;
        uint64_t volumeFree = 0;
        for (const auto& volume : disk.volumes)
        {
            volumeTotal += volume.totalBytes;
            volumeFree += volume.freeBytes;
        }
        if (disk.storage.capacityBytes == 0)
            disk.storage.capacityBytes = volumeTotal;
        disk.storage.freeBytes = volumeFree;
        disk.storage.volumes = disk.volumes;
        if (disk.storage.smartStatus.empty())
            disk.storage.smartStatus = Tr(L"未查询");
        if (disk.storage.health.empty())
            disk.storage.health = disk.storage.smartAvailable ? disk.storage.smartStatus : std::wstring(Tr(L"未知"));
        if (disk.storage.partitionStyle.empty())
            disk.storage.partitionStyle = Tr(L"未知");
        if (disk.storage.flashChip.empty())
            disk.storage.flashChip = Tr(L"无法读取（USB 桥接器不暴露 NAND/闪存颗粒型号）");

        UsbNode* target = nullptr;
        if (!disk.usbAncestor.empty())
            target = snapshot.FindByInstance(disk.usbAncestor);
        if (target == nullptr && !disk.containerId.empty())
        {
            for (auto& node : snapshot.nodes)
            {
                if (_wcsicmp(node.containerId.c_str(), disk.containerId.c_str()) == 0 &&
                    (node.kind == NodeKind::Device || node.kind == NodeKind::Hub))
                {
                    target = &node;
                    break;
                }
            }
        }
        if (target == nullptr)
            continue;

        ChipIdentity chip = LookupUsbChip(target->idVendor, target->idProduct, false);
        if (chip.known || !chip.vendor.empty())
        {
            disk.storage.bridgeVendor = chip.vendor;
            disk.storage.bridgeChip = chip.chip;
        }
        if (disk.storage.bridgeChip.empty())
            disk.storage.bridgeChip = target->chipModel;
        if (disk.storage.bridgeVendor.empty())
            disk.storage.bridgeVendor = target->chipVendor;
        if (disk.storage.vendor.empty())
        {
            disk.storage.vendor = target->usbManufacturer;
            if (disk.storage.vendor.empty())
                disk.storage.vendor = target->manufacturer;
        }

        AttachStorageToNode(snapshot, *target, disk.storage);
    }

    // If a USB mass-storage PnP node exists but no disk class record was found,
    // still show the bridge and a clear warning instead of hiding the device.
    const int initialNodeCount = static_cast<int>(snapshot.nodes.size());
    for (int i = 0; i < initialNodeCount; ++i)
    {
        UsbNode* node = snapshot.Find(i);
        if (node == nullptr)
            continue;
        if (node->kind == NodeKind::Storage || node->kind == NodeKind::Cable)
            continue;
        if (!node->isUsbStorage || node->storage.present)
            continue;
        StorageInfo partial;
        partial.present = true;
        partial.model = node->friendlyName;
        if (partial.model.empty())
            partial.model = node->model;
        if (partial.model.empty())
            partial.model = node->description;
        if (partial.model.empty())
            partial.model = Tr(L"USB 存储设备");
        partial.bridgeVendor = node->chipVendor;
        partial.bridgeChip = node->chipModel;
        partial.busType = L"USB";
        partial.smartStatus = Tr(L"不可用");
        partial.smartMessage = Tr(L"未找到对应的磁盘/卷；设备可能没有媒体或驱动未加载。");
        partial.flashChip = Tr(L"无法读取");
        AttachStorageToNode(snapshot, *node, partial);
    }
}
