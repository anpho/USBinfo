#include "pch.h"
#include "ChipDb.h"

#include <winioctl.h>

namespace
{
struct UsbChipEntry
{
    uint16_t vid;
    uint16_t pid;
    const wchar_t* vendor;
    const wchar_t* chip;
    const wchar_t* category;
    const wchar_t* note;
};

// The USB-IF does not provide a reliable "chip model" property.  These are
// well-known VID/PID pairs used by bridge and hub controllers.  The UI labels
// every match as an inference, and always shows the raw VID/PID as well.
const UsbChipEntry kUsbChips[] =
{
    // USB hubs / hub controllers
    { 0x2109, 0x2813, L"VIA Labs", L"VL817", L"USB Hub", L"USB 3.1 Gen 1 hub controller" },
    { 0x2109, 0x2827, L"VIA Labs", L"VL822", L"USB Hub", L"USB 3.2 Gen 2 hub controller" },
    { 0x2109, 0x3431, L"VIA Labs", L"VL830", L"USB4 / Hub", L"USB4 / DisplayPort hub controller" },
    { 0x2109, 0x0715, L"VIA Labs", L"VL715", L"USB-SATA", L"USB 3.1 to SATA bridge" },
    { 0x2109, 0x0716, L"VIA Labs", L"VL716", L"USB-SATA", L"USB 3.1 to SATA bridge" },
    { 0x05E3, 0x0608, L"Genesys Logic", L"GL850G", L"USB Hub", L"USB 2.0 hub controller" },
    { 0x05E3, 0x0610, L"Genesys Logic", L"GL852G", L"USB Hub", L"USB 2.0 hub controller" },
    { 0x05E3, 0x0612, L"Genesys Logic", L"GL3510", L"USB Hub", L"USB 3.0 hub controller" },
    { 0x05E3, 0x0614, L"Genesys Logic", L"GL3520", L"USB Hub", L"USB 3.0 hub controller" },
    { 0x05E3, 0x0616, L"Genesys Logic", L"GL3224", L"Card Reader", L"USB 3.0 SD card reader" },
    { 0x05E3, 0x0620, L"Genesys Logic", L"GL3230", L"Card Reader", L"USB 3.0 card reader" },
    { 0x05E3, 0x0626, L"Genesys Logic", L"GL3523", L"USB Hub", L"USB 3.1 hub controller" },
    { 0x05E3, 0x0751, L"Genesys Logic", L"GL9750", L"Card Reader", L"USB 3.0 SD 4.0 card reader" },
    { 0x0BDA, 0x5411, L"Realtek", L"RTS5411", L"USB Hub", L"USB 3.1 hub controller" },
    { 0x0BDA, 0x5412, L"Realtek", L"RTS5412", L"USB Hub", L"USB 3.1 hub controller" },
    { 0x0BDA, 0x5423, L"Realtek", L"RTS5423", L"USB Hub", L"USB 3.2 hub controller" },
    { 0x0424, 0x2514, L"Microchip / SMSC", L"USB2514", L"USB Hub", L"USB 2.0 hub controller" },
    { 0x0424, 0x5744, L"Microchip / SMSC", L"USB5744", L"USB Hub", L"USB 3.1 hub controller" },
    { 0x1A40, 0x0101, L"Terminus", L"FE1.1", L"USB Hub", L"USB 2.0 hub controller" },
    { 0x1A40, 0x0201, L"Terminus", L"FE2.1", L"USB Hub", L"USB 2.0 hub controller" },
    { 0x0451, 0x8140, L"Texas Instruments", L"TUSB8041", L"USB Hub", L"USB 3.0 hub controller" },
    { 0x1D6B, 0x0002, L"Linux Foundation", L"root_hub", L"USB Host", L"Linux gadget / root hub" },

    // USB to SATA / NVMe bridge chips
    { 0x174C, 0x1153, L"ASMedia", L"ASM1153", L"USB-SATA", L"USB 3.0 to SATA bridge" },
    { 0x174C, 0x1351, L"ASMedia", L"ASM1351", L"USB-SATA", L"USB 3.1 to SATA bridge" },
    { 0x174C, 0x2362, L"ASMedia", L"ASM2362", L"USB-NVMe", L"USB 3.1 to NVMe bridge" },
    { 0x174C, 0x2364, L"ASMedia", L"ASM2364", L"USB-NVMe", L"USB 3.2 Gen 2x2 to NVMe bridge" },
    { 0x174C, 0x55AA, L"ASMedia", L"ASM1053 / ASM1153", L"USB-SATA", L"USB 3.0 to SATA bridge family" },
    { 0x152D, 0x0562, L"JMicron", L"JMS562", L"USB-SATA", L"USB 3.0 to SATA bridge" },
    { 0x152D, 0x0578, L"JMicron", L"JMS578", L"USB-SATA", L"USB 3.0 to SATA bridge" },
    { 0x152D, 0x0583, L"JMicron", L"JMS583", L"USB-NVMe", L"USB 3.1 to NVMe bridge" },
    { 0x152D, 0x1561, L"JMicron", L"JMS561", L"USB-SATA", L"USB 3.0 to dual SATA bridge" },
    { 0x0BDA, 0x9210, L"Realtek", L"RTL9210", L"USB-NVMe", L"USB 3.1 to NVMe bridge" },
    { 0x0BDA, 0x9211, L"Realtek", L"RTL9210B", L"USB-NVMe", L"USB 3.1 to NVMe bridge" },
    { 0x0BDA, 0x9220, L"Realtek", L"RTL9220", L"USB-NVMe", L"USB 3.2 to dual NVMe bridge" },
    { 0x05E3, 0x0751, L"Genesys Logic", L"GL9750", L"Card Reader", L"USB 3.0 SD card reader" },
    { 0x1F75, 0x0903, L"Innostor", L"IS903", L"USB Flash", L"USB 3.0 flash drive controller" },
    { 0x1F75, 0x0916, L"Innostor", L"IS916", L"USB Flash", L"USB 2.0 flash drive controller" },
    { 0x090C, 0x1000, L"Silicon Motion", L"SM3257EN", L"USB Flash", L"USB 2.0 flash drive controller" },
    { 0x090C, 0x2000, L"Silicon Motion", L"SM3267", L"USB Flash", L"USB 3.0 flash drive controller" },
    { 0x058F, 0x6387, L"Alcor Micro", L"AU6989", L"USB Flash", L"USB 2.0 flash drive controller" },
    { 0x048D, 0x1177, L"ITE", L"IT1177", L"USB Flash", L"USB flash drive controller (generic)" },
    { 0x0781, 0x5588, L"SanDisk", L"主控未公开", L"USB Flash", L"SanDisk Extreme Pro flash drive" },
    { 0x0781, 0x5567, L"SanDisk", L"主控未公开", L"USB Flash", L"SanDisk Ultra flash drive" },
    { 0x0951, 0x1666, L"Kingston", L"DataTraveler (controller not exposed)", L"USB Flash", L"Kingston DataTraveler flash drive" },
    { 0x13FE, 0x5500, L"Kingston", L"DataTraveler (controller not exposed)", L"USB Flash", L"Kingston flash drive" },

    // Common USB peripherals
    { 0x0BDA, 0x8153, L"Realtek", L"RTL8153", L"USB Ethernet", L"USB 3.0 Gigabit Ethernet" },
    { 0x0BDA, 0x8152, L"Realtek", L"RTL8152", L"USB Ethernet", L"USB 2.0 Fast Ethernet" },
    { 0x17EF, 0x720C, L"Lenovo / Realtek", L"RTL8153", L"USB Ethernet", L"Lenovo USB Ethernet" },
    { 0x8087, 0x0026, L"Intel", L"Wireless Bluetooth", L"Bluetooth", L"Intel Bluetooth controller" },
    { 0x8087, 0x0032, L"Intel", L"Wireless Bluetooth", L"Bluetooth", L"Intel Bluetooth controller" },
    { 0x8087, 0x0033, L"Intel", L"Wireless Bluetooth", L"Bluetooth", L"Intel Bluetooth controller" },
    { 0x30C9, 0x0063, L"SunplusIT", L"Integrated Camera", L"Camera", L"Integrated UVC camera" },
    { 0x27C6, 0x6594, L"Goodix", L"Fingerprint Sensor", L"Biometric", L"Goodix fingerprint reader" },
    { 0x22D9, 0x2764, L"OnePlus", L"PHB110", L"Phone / MTP", L"OnePlus phone (MTP)" },
    { 0x22D9, 0x2765, L"OnePlus", L"PHB110", L"Phone / Composite", L"OnePlus phone (composite)" },
    { 0x22D9, 0x2766, L"OnePlus", L"PHB110", L"Phone / Composite", L"OnePlus phone (RNDIS/composite)" },
    { 0x22D9, 0x2769, L"OnePlus", L"PHB110", L"Phone / WinUSB", L"OnePlus phone (WinUSB)" },
};
}

std::wstring LookupPciVendor(uint16_t vendorId)
{
    switch (vendorId)
    {
    case 0x8086: return L"Intel";
    case 0x1022: return L"AMD";
    case 0x10DE: return L"NVIDIA";
    case 0x1B21: return L"ASMedia";
    case 0x1912: return L"Renesas";
    case 0x104D: return L"Sony";
    case 0x17AA: return L"Lenovo";
    case 0x14E4: return L"Broadcom";
    case 0x1D0F: return L"Amazon";
    case 0x1AE0: return L"Google";
    default: return std::wstring();
    }
}

std::wstring LookupPciUsbController(uint16_t vendorId, uint16_t deviceId)
{
    if (vendorId == 0x8086)
    {
        // Only a small set of common Intel xHCI IDs.  The PnP description
        // already contains the marketing name, so this is a secondary hint.
        switch (deviceId)
        {
        case 0x9DED: case 0x9DEE: case 0xA12F:
            return L"Intel 100/200 Series Chipset xHCI";
        case 0xA36D: case 0xA36E:
            return L"Intel 300 Series Chipset xHCI";
        case 0x51ED: case 0x51EE:
            return L"Intel 500 Series Chipset xHCI";
        case 0x461E: case 0x461F:
            return L"Intel 600/700 Series Chipset xHCI";
        case 0x7A60: case 0x7A61:
            return L"Intel 800 Series Chipset xHCI";
        default: return std::wstring();
        }
    }
    if (vendorId == 0x1022)
        return L"AMD xHCI";
    if (vendorId == 0x1B21)
        return L"ASMedia xHCI";
    if (vendorId == 0x1912)
        return L"Renesas xHCI";
    return std::wstring();
}

ChipIdentity LookupUsbChip(uint16_t vendorId, uint16_t productId, bool hubContext)
{
    ChipIdentity result;
    for (const auto& entry : kUsbChips)
    {
        if (entry.vid == vendorId && entry.pid == productId)
        {
            result.known = true;
            result.vendor = entry.vendor;
            result.chip = entry.chip;
            result.category = entry.category;
            result.note = entry.note;
            return result;
        }
    }

    // Fall back to a vendor-level guess for known hub silicon vendors.
    switch (vendorId)
    {
    case 0x2109:
        result.vendor = L"VIA Labs";
        result.category = hubContext ? L"USB Hub" : L"USB 桥接";
        result.note = L"根据厂商 ID 推断";
        return result;
    case 0x05E3:
        result.vendor = L"Genesys Logic";
        result.category = hubContext ? L"USB Hub" : L"USB 桥接";
        result.note = L"根据厂商 ID 推断";
        return result;
    case 0x0BDA:
        result.vendor = L"Realtek";
        result.category = hubContext ? L"USB Hub" : L"USB 桥接";
        result.note = L"根据厂商 ID 推断";
        return result;
    case 0x0424:
        result.vendor = L"Microchip / SMSC";
        result.category = L"USB Hub";
        result.note = L"根据厂商 ID 推断";
        return result;
    case 0x1A40:
        result.vendor = L"Terminus";
        result.category = L"USB Hub";
        result.note = L"根据厂商 ID 推断";
        return result;
    case 0x174C:
        result.vendor = L"ASMedia";
        result.category = L"USB 桥接";
        result.note = L"根据厂商 ID 推断";
        return result;
    case 0x152D:
        result.vendor = L"JMicron";
        result.category = L"USB 桥接";
        result.note = L"根据厂商 ID 推断";
        return result;
    default:
        return result;
    }
}

std::wstring StorageBusTypeName(int busType)
{
    switch (busType)
    {
    case BusTypeScsi:      return L"SCSI";
    case BusTypeAtapi:     return L"ATAPI";
    case BusTypeAta:       return L"ATA";
    case BusType1394:      return L"IEEE 1394";
    case BusTypeSsa:       return L"SSA";
    case BusTypeFibre:     return L"Fibre Channel";
    case BusTypeUsb:       return L"USB";
    case BusTypeRAID:      return L"RAID";
    case BusTypeiScsi:     return L"iSCSI";
    case BusTypeSas:       return L"SAS";
    case BusTypeSata:      return L"SATA";
    case BusTypeSd:        return L"SD";
    case BusTypeMmc:       return L"MMC";
    case BusTypeVirtual:   return L"虚拟";
    case BusTypeFileBackedVirtual: return L"文件虚拟";
    case BusTypeSpaces:    return L"Storage Spaces";
    case BusTypeNvme:      return L"NVMe";
    case BusTypeSCM:       return L"SCM";
    case BusTypeUfs:       return L"UFS";
    default:
    {
        wchar_t buffer[64] = {};
        swprintf_s(buffer, L"总线类型 %d", busType);
        return buffer;
    }
    }
}
