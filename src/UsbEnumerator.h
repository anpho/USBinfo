#pragma once

#include "pch.h"
#include "Model.h"

class UsbEnumerator
{
public:
    // Enumerates USB host controllers, hubs, devices, ports/cable information
    // and USB storage.  The returned snapshot is never null.  The operation
    // is synchronous; callers normally run it on a worker thread.
    static std::shared_ptr<UsbSnapshot> Scan(std::atomic_bool* cancel = nullptr,
        const std::function<void(const std::wstring&)>& progress = {});
};
