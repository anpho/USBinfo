#include "pch.h"
#include "Smart.h"
#include "DeviceProps.h"
#include "Language.h"

#include <winioctl.h>
#include <ntddscsi.h>
#include <malloc.h>

namespace
{
const wchar_t* SmartAttributeName(int id)
{
    switch (id)
    {
    case 1:   return Tr(L"底层读取错误率");
    case 2:   return Tr(L"吞吐量性能");
    case 3:   return Tr(L"旋转时间");
    case 4:   return Tr(L"启动/停止计数");
    case 5:   return Tr(L"重映射扇区计数");
    case 7:   return Tr(L"寻道错误率");
    case 8:   return Tr(L"寻道时间性能");
    case 9:   return Tr(L"通电时间");
    case 10:  return Tr(L"主轴重试次数");
    case 11:  return Tr(L"校准重试次数");
    case 12:  return Tr(L"通电次数");
    case 13:  return Tr(L"软读取错误率");
    case 22:  return Tr(L"氦气液位");
    case 23:  return Tr(L"磁头飞行高度");
    case 24:  return Tr(L"磁头飞行小时");
    case 25:  return Tr(L"读写比率");
    case 170: return Tr(L"坏块计数");
    case 171: return Tr(L"编程失败计数");
    case 172: return Tr(L"擦除失败计数");
    case 173: return Tr(L"平均擦写次数");
    case 174: return Tr(L"意外断电计数");
    case 175: return Tr(L"编程失败块");
    case 176: return Tr(L"擦除失败块");
    case 177: return Tr(L"磨损范围");
    case 178: return Tr(L"保留块剩余");
    case 179: return Tr(L"已用保留块");
    case 180: return Tr(L"未使用保留块");
    case 181: return Tr(L"程序失败计数");
    case 182: return Tr(L"擦除失败计数");
    case 183: return Tr(L"运行中止计数");
    case 184: return Tr(L"端到端错误");
    case 185: return Tr(L"磁头稳定性");
    case 186: return Tr(L"飞行高度");
    case 187: return Tr(L"可纠正错误");
    case 188: return Tr(L"命令超时");
    case 189: return Tr(L"高飞写");
    case 190: return Tr(L"空气温度");
    case 191: return Tr(L"冲击错误率");
    case 192: return Tr(L"断电回收计数");
    case 193: return Tr(L"负载循环计数");
    case 194: return Tr(L"温度");
    case 195: return Tr(L"硬件 ECC 恢复");
    case 196: return Tr(L"重分配事件");
    case 197: return Tr(L"当前待映射扇区");
    case 198: return Tr(L"脱机不可修正扇区");
    case 199: return Tr(L"UDMA CRC 错误");
    case 200: return Tr(L"多区域错误率");
    case 201: return Tr(L"软读取错误率");
    case 202: return Tr(L"数据地址标记错误");
    case 203: return Tr(L"运行中止");
    case 204: return Tr(L"软 ECC 修正");
    case 205: return Tr(L"热敏电阻错误");
    case 206: return Tr(L"飞行高度");
    case 207: return Tr(L"主轴高电流");
    case 208: return Tr(L"旋转重试");
    case 209: return Tr(L"离线寻道性能");
    case 210: return Tr(L"VCM 电流");
    case 211: return Tr(L"VCM 电流");
    case 212: return Tr(L"冲击错误");
    case 213: return Tr(L"震动错误");
    case 220: return Tr(L"磁盘位移");
    case 221: return Tr(L"G 感应错误率");
    case 222: return Tr(L"加载/卸载重试");
    case 223: return Tr(L"加载/卸载失败");
    case 224: return Tr(L"加载摩擦");
    case 225: return Tr(L"加载/卸载计数");
    case 226: return Tr(L"加载时间");
    case 227: return Tr(L"扭矩放大计数");
    case 228: return Tr(L"断电回收");
    case 230: return Tr(L"磁头寿命");
    case 231: return Tr(L"剩余寿命");
    case 232: return Tr(L"可用保留空间");
    case 233: return Tr(L"介质磨损指示");
    case 234: return Tr(L"平均擦除次数");
    case 235: return Tr(L"良好块计数");
    case 240: return Tr(L"磁头飞行时间");
    case 241: return Tr(L"总 LBAs 写入");
    case 242: return Tr(L"总 LBAs 读取");
    case 243: return Tr(L"DRAM 读取回收");
    case 244: return Tr(L"温度");
    case 245: return Tr(L"剩余寿命");
    case 250: return Tr(L"读取错误重试");
    case 251: return Tr(L"最小擦除计数");
    case 252: return Tr(L"最大擦除计数");
    case 254: return Tr(L"空闲块计数");
    case 255: return Tr(L"剩余寿命");
    default:  return nullptr;
    }
}

std::wstring SmartAttributeNameOrId(int id)
{
    const wchar_t* name = SmartAttributeName(id);
    if (name != nullptr)
        return name;
    wchar_t buffer[32] = {};
    swprintf_s(buffer, Tr(L"属性 0x%02X"), id);
    return buffer;
}

bool LooksLikeSmartData(const BYTE* data, size_t size, size_t offset)
{
    if (data == nullptr || size < offset + 2 + 30 * 12)
        return false;
    int plausible = 0;
    for (int i = 0; i < 30; ++i)
    {
        const BYTE* attr = data + offset + i * 12;
        if (attr[0] == 0)
            continue;
        if (attr[3] >= 1 && attr[3] <= 253 && attr[4] >= 1 && attr[4] <= 253)
            ++plausible;
    }
    return plausible >= 2;
}

std::vector<BYTE> SmartSectorFromPredict(const STORAGE_PREDICT_FAILURE& predict)
{
    const BYTE* data = predict.VendorSpecific;
    return std::vector<BYTE>(data, data + sizeof(predict.VendorSpecific));
}

bool ParseSmartAttributes(const std::vector<BYTE>& data, StorageInfo& storage)
{
    if (data.size() < 2 + 30 * 12)
        return false;
    size_t offset = 2;
    if (!LooksLikeSmartData(data.data(), data.size(), offset) && LooksLikeSmartData(data.data(), data.size(), 0))
        offset = 0;
    if (!LooksLikeSmartData(data.data(), data.size(), offset))
        return false;

    std::vector<SmartAttribute> attributes;
    for (int i = 0; i < 30; ++i)
    {
        const BYTE* attr = data.data() + offset + i * 12;
        int id = attr[0];
        if (id == 0)
            continue;
        SmartAttribute item;
        item.id = id;
        item.name = SmartAttributeNameOrId(id);
        item.current = attr[3];
        item.worst = attr[4];
        item.raw = 0;
        for (int byteIndex = 5; byteIndex >= 0; --byteIndex)
            item.raw = (item.raw << 8) | attr[byteIndex];
        item.rawText = FormatNumber(item.raw);
        item.threshold = -1;
        item.status = Tr(L"正常");
        if (item.current == 0)
            item.status = Tr(L"无效");
        attributes.push_back(std::move(item));
    }
    if (attributes.empty())
        return false;
    storage.smart = std::move(attributes);
    return true;
}

bool ParseSmartThresholds(const BYTE* data, size_t size, StorageInfo& storage)
{
    if (data == nullptr || size < 2 + 30 * 12)
        return false;
    std::map<int, int> thresholds;
    for (int i = 0; i < 30; ++i)
    {
        const BYTE* entry = data + 2 + i * 12;
        int id = entry[0];
        if (id != 0)
            thresholds[id] = entry[1];
    }
    if (thresholds.empty())
        return false;
    for (auto& attr : storage.smart)
    {
        auto it = thresholds.find(attr.id);
        if (it != thresholds.end())
            attr.threshold = it->second;
    }
    return true;
}

void ApplySmartStatus(StorageInfo& storage)
{
    for (auto& attr : storage.smart)
    {
        bool warning = false;
        if (attr.threshold >= 0 && attr.current <= attr.threshold)
            warning = true;
        if (attr.id == 5 || attr.id == 197 || attr.id == 198)
            warning = warning || attr.raw > 0;
        if (attr.id == 199)
            warning = warning || attr.raw > 0;
        if (attr.current == 0)
            warning = false;
        attr.status = warning ? Tr(L"注意") : Tr(L"正常");
    }

    auto rawOf = [&storage](int id) -> uint64_t
    {
        for (const auto& attr : storage.smart)
            if (attr.id == id)
                return attr.raw;
        return 0;
    };
    auto addExtra = [&storage](const wchar_t* key, const std::wstring& value)
    {
        if (!value.empty())
            storage.smartExtra.emplace_back(Tr(key), value);
    };

    uint64_t temperature = rawOf(194);
    if (temperature == 0)
        temperature = rawOf(190);
    if (temperature != 0)
    {
        uint64_t celsius = temperature & 0xFF;
        if (celsius > 0 && celsius < 150)
            addExtra(L"温度", std::to_wstring(celsius) + L" °C");
    }
    uint64_t powerOn = rawOf(9);
    if (powerOn != 0)
        addExtra(L"通电时间", FormatNumber(powerOn) + L" " + Tr(L"小时"));
    uint64_t powerCycles = rawOf(12);
    if (powerCycles != 0)
        addExtra(L"通电次数", FormatNumber(powerCycles));
    uint64_t reallocated = rawOf(5);
    if (reallocated != 0)
        addExtra(L"重映射扇区", FormatNumber(reallocated));
    uint64_t pending = rawOf(197);
    if (pending != 0)
        addExtra(L"待映射扇区", FormatNumber(pending));
    uint64_t uncorrectable = rawOf(198);
    if (uncorrectable != 0)
        addExtra(L"不可修正扇区", FormatNumber(uncorrectable));
    uint64_t crc = rawOf(199);
    if (crc != 0)
        addExtra(L"CRC 错误", FormatNumber(crc));
    uint64_t wear = rawOf(233);
    if (wear != 0)
        addExtra(L"介质磨损", FormatNumber(wear));
    uint64_t life = rawOf(231);
    if (life == 0)
        life = rawOf(255);
    if (life != 0)
        addExtra(L"剩余寿命", FormatNumber(life) + L"%");
    uint64_t written = rawOf(241);
    if (written != 0)
        addExtra(L"LBAs 写入", FormatNumber(written));
    uint64_t read = rawOf(242);
    if (read != 0)
        addExtra(L"LBAs 读取", FormatNumber(read));
}

std::wstring AtaIdentifyString(const BYTE* identify, int firstWord, int wordCount)
{
    std::wstring result;
    for (int i = 0; i < wordCount; ++i)
    {
        int offset = (firstWord + i) * 2;
        wchar_t ch1 = static_cast<wchar_t>(identify[offset + 1]);
        wchar_t ch2 = static_cast<wchar_t>(identify[offset]);
        result.push_back(ch1);
        result.push_back(ch2);
    }
    while (!result.empty() && (result.back() == L' ' || result.back() == L'\0'))
        result.pop_back();
    while (!result.empty() && (result.front() == L' ' || result.front() == L'\0'))
        result.erase(result.begin());
    return result;
}

bool AtaPassThrough(HANDLE disk, BYTE command, BYTE features, BYTE sectorCount, BYTE lbaLow, BYTE lbaMid, BYTE lbaHigh, BYTE device, BYTE* data, DWORD dataLength, DWORD* errorCode)
{
    struct alignas(8) AtaBuffer
    {
        ATA_PASS_THROUGH_EX apt;
        BYTE data[512];
    } buffer = {};

    buffer.apt.Length = sizeof(ATA_PASS_THROUGH_EX);
    buffer.apt.AtaFlags = ATA_FLAGS_DATA_IN | ATA_FLAGS_DRDY_REQUIRED;
    buffer.apt.DataTransferLength = dataLength;
    buffer.apt.TimeOutValue = 8;
    buffer.apt.DataBufferOffset = offsetof(AtaBuffer, data);
    buffer.apt.CurrentTaskFile[0] = features;
    buffer.apt.CurrentTaskFile[1] = sectorCount;
    buffer.apt.CurrentTaskFile[2] = lbaLow;
    buffer.apt.CurrentTaskFile[3] = lbaMid;
    buffer.apt.CurrentTaskFile[4] = lbaHigh;
    buffer.apt.CurrentTaskFile[5] = device;
    buffer.apt.CurrentTaskFile[6] = command;

    DWORD returned = 0;
    BOOL ok = ::DeviceIoControl(disk, IOCTL_ATA_PASS_THROUGH, &buffer, sizeof(buffer), &buffer, sizeof(buffer), &returned, nullptr);
    if (errorCode != nullptr)
        *errorCode = ok ? ERROR_SUCCESS : ::GetLastError();
    if (!ok)
        return false;
    if (data != nullptr && dataLength >= 512)
        memcpy(data, buffer.data, 512);
    return true;
}

bool ScsiAtaPassThrough(HANDLE disk, bool use16, BYTE features, BYTE sectorCount, BYTE lbaLow, BYTE lbaMid, BYTE lbaHigh, BYTE device, BYTE* data, DWORD dataLength, DWORD* errorCode)
{
    BYTE* aligned = static_cast<BYTE*>(_aligned_malloc(dataLength, 4096));
    if (aligned == nullptr)
        return false;
    memset(aligned, 0, dataLength);

    BYTE cdb[16] = {};
    if (use16)
    {
        cdb[0] = 0x85;
        cdb[1] = static_cast<BYTE>((4 << 1)); // PIO data-in
        cdb[2] = 0x2E;                        // CK_COND=1, T_DIR=1, BYTE_BLOCK=1, T_LENGTH=2
        cdb[3] = 0;
        cdb[4] = features;
        cdb[5] = 0;
        cdb[6] = sectorCount;
        cdb[7] = 0;
        cdb[8] = lbaLow;
        cdb[9] = 0;
        cdb[10] = lbaMid;
        cdb[11] = 0;
        cdb[12] = lbaHigh;
        cdb[13] = device;
        cdb[14] = 0xB0;
    }
    else
    {
        cdb[0] = 0xA1;
        cdb[1] = static_cast<BYTE>((4 << 1));
        cdb[2] = 0x2E;
        cdb[3] = features;
        cdb[4] = sectorCount;
        cdb[5] = lbaLow;
        cdb[6] = lbaMid;
        cdb[7] = lbaHigh;
        cdb[8] = device;
        cdb[9] = 0xB0;
    }

    SCSI_PASS_THROUGH_DIRECT spt = {};
    spt.Length = sizeof(spt);
    spt.CdbLength = use16 ? 16 : 12;
    spt.SenseInfoLength = 32;
    spt.DataIn = SCSI_IOCTL_DATA_IN;
    spt.DataTransferLength = dataLength;
    spt.TimeOutValue = 10;
    spt.DataBuffer = aligned;
    spt.SenseInfoOffset = sizeof(SCSI_PASS_THROUGH_DIRECT);
    memcpy(spt.Cdb, cdb, spt.CdbLength);

    std::vector<BYTE> ioBuffer(sizeof(SCSI_PASS_THROUGH_DIRECT) + 32);
    memcpy(ioBuffer.data(), &spt, sizeof(spt));
    DWORD returned = 0;
    BOOL ok = ::DeviceIoControl(disk, IOCTL_SCSI_PASS_THROUGH_DIRECT, ioBuffer.data(),
        static_cast<DWORD>(ioBuffer.size()), ioBuffer.data(), static_cast<DWORD>(ioBuffer.size()), &returned, nullptr);
    if (errorCode != nullptr)
        *errorCode = ok ? ERROR_SUCCESS : ::GetLastError();
    if (ok)
        memcpy(data, aligned, std::min<DWORD>(dataLength, 512));
    _aligned_free(aligned);
    return ok != FALSE;
}

void AddSmartMessage(StorageInfo& storage, const std::wstring& message)
{
    if (!storage.smartMessage.empty())
        storage.smartMessage += Tr(L"；");
    storage.smartMessage += message;
}

bool TryNvmeSmart(HANDLE disk, StorageInfo& storage, DWORD* errorCode)
{
    struct
    {
        STORAGE_PROPERTY_QUERY query;
        STORAGE_PROTOCOL_SPECIFIC_DATA protocol;
    } input = {};

    input.query.PropertyId = StorageAdapterProtocolSpecificProperty;
    input.query.QueryType = PropertyStandardQuery;
    input.protocol.ProtocolType = ProtocolTypeNvme;
    input.protocol.DataType = NVMeDataTypeLogPage;
    input.protocol.ProtocolDataRequestValue = 0x02; // SMART / Health Information
    input.protocol.ProtocolDataRequestSubValue = 0;
    input.protocol.ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    input.protocol.ProtocolDataLength = 512;

    BYTE output[1024] = {};
    DWORD returned = 0;
    BOOL ok = ::DeviceIoControl(disk, IOCTL_STORAGE_QUERY_PROPERTY, &input, sizeof(input), output, sizeof(output), &returned, nullptr);
    if (!ok)
    {
        if (errorCode != nullptr)
            *errorCode = ::GetLastError();
        return false;
    }

    auto* protocol = reinterpret_cast<STORAGE_PROTOCOL_SPECIFIC_DATA*>(output);
    const size_t offset = protocol->ProtocolDataOffset;
    if (protocol->ProtocolDataLength < 512 ||
        offset > sizeof(output) ||
        offset + 512 > sizeof(output) ||
        offset + 512 > returned)
        return false;

    const BYTE* data = output + offset;
    BYTE criticalWarning = data[0];
    USHORT temperatureKelvin = static_cast<USHORT>(data[1] | (data[2] << 8));
    BYTE availableSpare = data[3];
    BYTE spareThreshold = data[4];
    BYTE percentageUsed = data[5];
    uint64_t dataUnitsRead = 0;
    uint64_t dataUnitsWritten = 0;
    for (int i = 0; i < 8; ++i)
        dataUnitsRead |= static_cast<uint64_t>(data[32 + i]) << (8 * i);
    for (int i = 0; i < 8; ++i)
        dataUnitsWritten |= static_cast<uint64_t>(data[48 + i]) << (8 * i);
    uint64_t powerCycles = 0;
    uint64_t powerOnHours = 0;
    uint64_t unsafeShutdowns = 0;
    uint64_t mediaErrors = 0;
    for (int i = 0; i < 8; ++i)
        powerCycles |= static_cast<uint64_t>(data[112 + i]) << (8 * i);
    for (int i = 0; i < 8; ++i)
        powerOnHours |= static_cast<uint64_t>(data[128 + i]) << (8 * i);
    for (int i = 0; i < 8; ++i)
        unsafeShutdowns |= static_cast<uint64_t>(data[144 + i]) << (8 * i);
    for (int i = 0; i < 8; ++i)
        mediaErrors |= static_cast<uint64_t>(data[160 + i]) << (8 * i);

    storage.smartExtra.emplace_back(Tr(L"NVMe 关键警告"), FormatHex(criticalWarning, 2));
    storage.smartExtra.emplace_back(Tr(L"NVMe 温度"), std::to_wstring(temperatureKelvin > 273 ? temperatureKelvin - 273 : 0) + L" °C");
    storage.smartExtra.emplace_back(Tr(L"NVMe 可用备用空间"), std::to_wstring(availableSpare) + L"%");
    storage.smartExtra.emplace_back(Tr(L"NVMe 备用空间阈值"), std::to_wstring(spareThreshold) + L"%");
    storage.smartExtra.emplace_back(Tr(L"NVMe 已用寿命"), std::to_wstring(percentageUsed) + L"%");
    storage.smartExtra.emplace_back(Tr(L"数据单元读取"), FormatNumber(dataUnitsRead));
    storage.smartExtra.emplace_back(Tr(L"数据单元写入"), FormatNumber(dataUnitsWritten));
    storage.smartExtra.emplace_back(Tr(L"通电次数"), FormatNumber(powerCycles));
    storage.smartExtra.emplace_back(Tr(L"通电时间"), FormatNumber(powerOnHours) + L" " + Tr(L"小时"));
    storage.smartExtra.emplace_back(Tr(L"异常断电"), FormatNumber(unsafeShutdowns));
    storage.smartExtra.emplace_back(Tr(L"介质错误"), FormatNumber(mediaErrors));
    storage.health = criticalWarning == 0 ? Tr(L"NVMe SMART 正常") : Tr(L"NVMe SMART 警告");
    storage.smartAvailable = true;
    storage.smartStatus = storage.health;
    return true;
}
}

bool ReadAtaIdentify(HANDLE diskHandle, StorageInfo& storage)
{
    if (diskHandle == nullptr || diskHandle == INVALID_HANDLE_VALUE)
        return false;
    BYTE identify[512] = {};
    DWORD error = 0;
    if (!AtaPassThrough(diskHandle, 0xEC, 0, 0, 0, 0, 0, 0xA0, identify, sizeof(identify), &error))
    {
        error = 0;
        if (!ScsiAtaPassThrough(diskHandle, true, 0, 1, 0, 0, 0, 0xA0, identify, sizeof(identify), &error))
            return false;
    }
    std::wstring model = AtaIdentifyString(identify, 27, 20);
    std::wstring serial = AtaIdentifyString(identify, 10, 10);
    std::wstring firmware = AtaIdentifyString(identify, 23, 4);
    if (!model.empty() && storage.model.empty())
        storage.model = model;
    if (!serial.empty() && storage.serial.empty())
        storage.serial = serial;
    if (!firmware.empty() && storage.revision.empty())
        storage.revision = firmware;
    return true;
}

bool ReadSmartData(HANDLE diskHandle, StorageInfo& storage)
{
    if (diskHandle == nullptr || diskHandle == INVALID_HANDLE_VALUE)
    {
        storage.smartQueried = true;
        storage.smartStatus = Tr(L"不可用");
        storage.smartMessage = Tr(L"没有可用的磁盘句柄；请以管理员身份运行或检查设备访问权限。");
        return false;
    }

    storage.smartQueried = true;
    bool predictSucceeded = false;
    bool attributesRead = false;
    DWORD lastError = ERROR_SUCCESS;

    STORAGE_PREDICT_FAILURE predict = {};
    DWORD returned = 0;
    if (::DeviceIoControl(diskHandle, IOCTL_STORAGE_PREDICT_FAILURE, nullptr, 0, &predict, sizeof(predict), &returned, nullptr))
    {
        predictSucceeded = true;
        storage.health = predict.PredictFailure ? Tr(L"SMART 预测故障") : Tr(L"SMART 正常");
        storage.smartStatus = storage.health;
        std::vector<BYTE> data = SmartSectorFromPredict(predict);
        if (ParseSmartAttributes(data, storage))
            attributesRead = true;
    }
    else
    {
        lastError = ::GetLastError();
    }

    if (!attributesRead)
    {
        BYTE values[512] = {};
        DWORD error = 0;
        if (AtaPassThrough(diskHandle, 0xB0, 0xD0, 0x01, 0xC2, 0x4F, 0xC2, 0xA0, values, sizeof(values), &error))
        {
            std::vector<BYTE> data(values, values + sizeof(values));
            if (ParseSmartAttributes(data, storage))
                attributesRead = true;
        }
        if (!attributesRead)
        {
            error = 0;
            if (ScsiAtaPassThrough(diskHandle, true, 0xD0, 0x01, 0xC2, 0x4F, 0xC2, 0xA0, values, sizeof(values), &error))
            {
                std::vector<BYTE> data(values, values + sizeof(values));
                if (ParseSmartAttributes(data, storage))
                    attributesRead = true;
            }
            else if (ScsiAtaPassThrough(diskHandle, false, 0xD0, 0x01, 0xC2, 0x4F, 0xC2, 0xA0, values, sizeof(values), &error))
            {
                std::vector<BYTE> data(values, values + sizeof(values));
                if (ParseSmartAttributes(data, storage))
                    attributesRead = true;
            }
            else
            {
                lastError = error;
            }
        }
    }

    if (attributesRead)
    {
        BYTE thresholds[512] = {};
        DWORD error = 0;
        if (AtaPassThrough(diskHandle, 0xB0, 0xD1, 0x01, 0x01, 0x4F, 0xC2, 0xA0, thresholds, sizeof(thresholds), &error))
        {
            ParseSmartThresholds(thresholds, sizeof(thresholds), storage);
        }
        else
        {
            error = 0;
            if (ScsiAtaPassThrough(diskHandle, true, 0xD1, 0x01, 0x01, 0x4F, 0xC2, 0xA0, thresholds, sizeof(thresholds), &error))
                ParseSmartThresholds(thresholds, sizeof(thresholds), storage);
        }
        ApplySmartStatus(storage);
        storage.smartAvailable = true;
        if (storage.health.empty())
            storage.health = Tr(L"SMART 数据已读取");
        if (storage.smartStatus.empty())
            storage.smartStatus = storage.health;
        if (storage.smartMessage.empty())
            storage.smartMessage = Tr(L"SMART 数据来自 ATA 属性表。");
        return true;
    }

    DWORD nvmeError = 0;
    if (TryNvmeSmart(diskHandle, storage, &nvmeError))
        return true;

    storage.smartAvailable = predictSucceeded;
    if (predictSucceeded)
    {
        storage.smartStatus = storage.health;
        storage.smartMessage = Tr(L"已读取预测故障状态，但此设备未返回 SMART 属性表（常见于 USB 闪存盘/桥接器）。");
        return true;
    }

    storage.smartStatus = Tr(L"不可用");
    storage.smartMessage = Tr(L"设备未返回 SMART 数据。");
    if (!IsProcessElevated())
        storage.smartMessage += std::wstring(L" ") + Tr(L"请以管理员身份运行后重试。");
    else if (lastError != ERROR_SUCCESS)
        storage.smartMessage += std::wstring(L" ") + Tr(L"最后错误") + L": " + GetLastErrorMessage(lastError);
    storage.smartMessage += std::wstring(L" ") + Tr(L"USB 大容量存储桥接器通常不会透传 ATA/NVMe SMART。");
    return false;
}

bool SmartSelfTest()
{
    std::vector<BYTE> data(512, 0);
    auto setAttribute = [&data](int slot, int id, int current, int worst, uint64_t raw)
    {
        BYTE* attr = data.data() + 2 + slot * 12;
        attr[0] = static_cast<BYTE>(id);
        attr[3] = static_cast<BYTE>(current);
        attr[4] = static_cast<BYTE>(worst);
        for (int i = 0; i < 6; ++i)
            attr[5 + i] = static_cast<BYTE>((raw >> (8 * i)) & 0xFF);
    };
    setAttribute(0, 5, 100, 100, 1);
    setAttribute(1, 9, 100, 100, 1234);

    StorageInfo storage;
    if (!ParseSmartAttributes(data, storage) || storage.smart.size() != 2)
        return false;

    std::vector<BYTE> thresholds(512, 0);
    thresholds[2] = 5;
    thresholds[3] = 10;
    thresholds[2 + 12] = 9;
    thresholds[2 + 12 + 1] = 10;
    if (!ParseSmartThresholds(thresholds.data(), thresholds.size(), storage))
        return false;
    ApplySmartStatus(storage);

    bool sawWarning = false;
    bool sawNormal = false;
    for (const auto& attr : storage.smart)
    {
        if (attr.id == 5 && attr.status == Tr(L"注意"))
            sawWarning = true;
        if (attr.id == 9 && attr.status == Tr(L"正常"))
            sawNormal = true;
    }

    StorageInfo invalid;
    std::vector<BYTE> zeros(512, 0);
    if (ParseSmartAttributes(zeros, invalid))
        return false;
    return sawWarning && sawNormal;
}
