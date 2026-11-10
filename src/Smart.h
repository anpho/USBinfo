#pragma once

#include "pch.h"
#include "Model.h"

// Attempts to read ATA SMART data through the Windows storage stack.
// Supports IOCTL_STORAGE_PREDICT_FAILURE, IOCTL_ATA_PASS_THROUGH and
// SAT ATA PASS-THROUGH (SCSI).  Returns true if any health/SMART data was read.
bool ReadSmartData(HANDLE diskHandle, StorageInfo& storage);
bool ReadAtaIdentify(HANDLE diskHandle, StorageInfo& storage);

// Small synthetic parse test used by --self-test.
bool SmartSelfTest();
