#pragma once

#include "pch.h"

struct ChipIdentity
{
    bool known = false;
    std::wstring vendor;
    std::wstring chip;
    std::wstring category;
    std::wstring note;
};

ChipIdentity LookupUsbChip(uint16_t vendorId, uint16_t productId, bool hubContext);
std::wstring LookupPciVendor(uint16_t vendorId);
std::wstring LookupPciUsbController(uint16_t vendorId, uint16_t deviceId);
std::wstring StorageBusTypeName(int busType);
