#pragma once

#include "pch.h"

struct BenchmarkConfig
{
    std::wstring volumeRoot;
    uint64_t largeBytes = 256ull * 1024 * 1024;
    uint32_t largeBlock = 1024 * 1024;
    uint64_t smallBytes = 32ull * 1024 * 1024;
    uint32_t smallBlock = 4096;
};

struct BenchmarkResult
{
    std::wstring name;
    uint64_t bytes = 0;
    double seconds = 0.0;
    double mbps = 0.0;
    double iops = 0.0;
    double latencyMs = 0.0;
    bool write = false;
    bool sequential = false;
    bool ok = false;
    std::wstring error;
};

struct BenchmarkUpdate
{
    int percent = 0;
    std::wstring stage;
    std::optional<BenchmarkResult> result;
    // Interval throughput (MB/s) measured between two consecutive progress
    // callbacks of the current stage.  Only present on progress callbacks, so
    // the UI can draw a task-manager style real-time chart.
    std::optional<double> sampleMbps;
    bool done = false;
    bool ok = false;
    std::wstring message;
};

using BenchmarkCallback = std::function<void(const BenchmarkUpdate&)>;

// Creates a temporary file on the selected volume and performs large/small
// sequential/random read and write tests.  It never writes to the raw disk.
bool RunBenchmark(const BenchmarkConfig& config, const std::atomic_bool& cancel, const BenchmarkCallback& callback);
