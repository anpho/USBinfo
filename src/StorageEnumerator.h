#pragma once

#include "pch.h"
#include "Model.h"

// Attaches USB mass-storage information to the matching USB device nodes and
// adds one synthetic "存储" child per storage device.
void EnumerateUsbStorage(UsbSnapshot& snapshot, std::atomic_bool* cancel, const std::function<void(const std::wstring&)>& progress);
