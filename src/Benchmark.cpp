#include "pch.h"
#include "Benchmark.h"
#include "Model.h"
#include "Language.h"

#include <malloc.h>

namespace
{
struct TempBenchFile
{
    std::wstring path;
    HANDLE handle = INVALID_HANDLE_VALUE;
    bool buffered = false;

    ~TempBenchFile()
    {
        if (handle != INVALID_HANDLE_VALUE)
            ::CloseHandle(handle);
        if (!path.empty())
            ::DeleteFileW(path.c_str());
    }
};

double SecondsSince(const LARGE_INTEGER& start, const LARGE_INTEGER& frequency)
{
    LARGE_INTEGER now = {};
    ::QueryPerformanceCounter(&now);
    return static_cast<double>(now.QuadPart - start.QuadPart) / static_cast<double>(frequency.QuadPart);
}

uint64_t XorShift64(uint64_t& state)
{
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

std::wstring MakeTempPath(const std::wstring& volumeRoot)
{
    std::wstring root = volumeRoot;
    if (root.empty())
        return std::wstring();
    if (root.back() != L'\\' && root.back() != L'/')
        root.push_back(L'\\');
    wchar_t buffer[128] = {};
    swprintf_s(buffer, L"USBInfo_bench_%lu_%llu.tmp",
        static_cast<unsigned long>(::GetCurrentProcessId()),
        static_cast<unsigned long long>(::GetTickCount64()));
    return root + buffer;
}

bool OpenTestFile(TempBenchFile& file, const BenchmarkConfig& config, std::wstring& error)
{
    file.path = MakeTempPath(config.volumeRoot);
    if (file.path.empty())
    {
        error = Tr(L"无效的卷路径。");
        return false;
    }

    DWORD flags = FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_FLAG_NO_BUFFERING;
    file.handle = ::CreateFileW(file.path.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ, nullptr, CREATE_ALWAYS, flags, nullptr);
    file.buffered = false;
    if (file.handle == INVALID_HANDLE_VALUE)
    {
        flags = FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
        file.handle = ::CreateFileW(file.path.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ, nullptr, CREATE_ALWAYS, flags, nullptr);
        file.buffered = true;
    }
    if (file.handle == INVALID_HANDLE_VALUE)
    {
        wchar_t message[1024] = {};
        swprintf_s(message, Tr(L"无法创建测试文件：%s；%s"),
            file.path.c_str(), GetLastErrorMessage(::GetLastError()).c_str());
        error = message;
        return false;
    }

    LARGE_INTEGER position = {};
    position.QuadPart = static_cast<LONGLONG>(config.largeBytes);
    LARGE_INTEGER zero = {};
    if (!::SetFilePointerEx(file.handle, position, nullptr, FILE_BEGIN) ||
        !::SetEndOfFile(file.handle) ||
        !::SetFilePointerEx(file.handle, zero, nullptr, FILE_BEGIN) ||
        !::FlushFileBuffers(file.handle))
    {
        wchar_t message[1024] = {};
        swprintf_s(message, Tr(L"无法预分配测试文件：%s"),
            GetLastErrorMessage(::GetLastError()).c_str());
        error = message;
        return false;
    }
    return true;
}

bool RunStage(HANDLE file, const std::wstring& name, uint64_t bytes, uint32_t blockSize,
    bool write, bool sequential, uint64_t seed, const std::atomic_bool& cancel,
    const BenchmarkCallback& callback, uint64_t completedBefore, uint64_t totalBytes,
    BenchmarkResult& result, std::wstring& error)
{
    result.name = name;
    result.bytes = bytes;
    result.write = write;
    result.sequential = sequential;
    result.ok = false;

    if (file == INVALID_HANDLE_VALUE || bytes == 0 || blockSize == 0 || bytes % blockSize != 0)
    {
        error = Tr(L"测试参数无效。");
        result.error = error;
        return false;
    }

    void* raw = _aligned_malloc(blockSize, 4096);
    if (raw == nullptr)
    {
        error = Tr(L"无法分配测试缓冲区。");
        result.error = error;
        return false;
    }
    BYTE* buffer = static_cast<BYTE*>(raw);
    for (uint32_t i = 0; i < blockSize; ++i)
        buffer[i] = static_cast<BYTE>((i * 37u + static_cast<uint32_t>(seed)) & 0xFFu);

    const uint64_t blocks = bytes / blockSize;
    uint64_t state = seed == 0 ? 0x9E3779B97F4A7C15ull : seed;
    LARGE_INTEGER frequency = {};
    LARGE_INTEGER start = {};
    LARGE_INTEGER lastSample = {};
    ::QueryPerformanceFrequency(&frequency);
    ::QueryPerformanceCounter(&start);
    lastSample = start;
    uint64_t lastSampleBytes = 0;

    bool ok = true;
    std::wstring localError;
    uint64_t progressStep = std::max<uint64_t>(1, blocks / 64);

    for (uint64_t i = 0; i < blocks; ++i)
    {
        if (cancel.load())
        {
            localError = Tr(L"已取消。");
            ok = false;
            break;
        }

        uint64_t index = sequential ? i : (XorShift64(state) % blocks);
        uint64_t offset = index * blockSize;
        LARGE_INTEGER position = {};
        position.QuadPart = static_cast<LONGLONG>(offset);
        if (!::SetFilePointerEx(file, position, nullptr, FILE_BEGIN))
        {
            wchar_t message[1024] = {};
            swprintf_s(message, Tr(L"定位失败：%s"), GetLastErrorMessage(::GetLastError()).c_str());
            localError = message;
            ok = false;
            break;
        }

        DWORD transferred = 0;
        BOOL ioOk = FALSE;
        if (write)
            ioOk = ::WriteFile(file, buffer, blockSize, &transferred, nullptr);
        else
            ioOk = ::ReadFile(file, buffer, blockSize, &transferred, nullptr);
        if (!ioOk || transferred != blockSize)
        {
            wchar_t message[1024] = {};
            swprintf_s(message, write ? Tr(L"写入失败：%s") : Tr(L"读取失败：%s"),
                GetLastErrorMessage(::GetLastError()).c_str());
            localError = message;
            ok = false;
            break;
        }

        if ((i + 1) % progressStep == 0 || i + 1 == blocks)
        {
            uint64_t done = completedBefore + (i + 1) * blockSize;
            BenchmarkUpdate update;
            update.percent = totalBytes == 0 ? 0 : static_cast<int>(std::min<uint64_t>(100, done * 100 / totalBytes));
            update.stage = name;

            // Interval throughput since the previous progress callback; the UI
            // uses it for the live chart.  The first interval starts at the
            // beginning of this stage.
            LARGE_INTEGER now = {};
            ::QueryPerformanceCounter(&now);
            const uint64_t intervalBytes = (i + 1) * blockSize - lastSampleBytes;
            const double intervalSeconds = static_cast<double>(now.QuadPart - lastSample.QuadPart) /
                static_cast<double>(frequency.QuadPart);
            if (intervalBytes > 0 && intervalSeconds > 1e-9)
                update.sampleMbps = (static_cast<double>(intervalBytes) / (1024.0 * 1024.0)) / intervalSeconds;
            lastSample = now;
            lastSampleBytes = (i + 1) * blockSize;

            callback(update);
        }
    }

    if (write && ok)
    {
        if (!::FlushFileBuffers(file))
        {
            wchar_t message[1024] = {};
            swprintf_s(message, Tr(L"刷新写入缓存失败：%s"), GetLastErrorMessage(::GetLastError()).c_str());
            localError = message;
            ok = false;
        }
    }

    if (!ok)
    {
        _aligned_free(raw);
        result.error = localError;
        error = localError;
        return false;
    }

    double seconds = std::max(1e-9, SecondsSince(start, frequency));
    result.seconds = seconds;
    result.mbps = (static_cast<double>(bytes) / (1024.0 * 1024.0)) / seconds;
    result.iops = static_cast<double>(blocks) / seconds;
    result.latencyMs = seconds * 1000.0 / static_cast<double>(blocks);
    result.ok = true;
    _aligned_free(raw);
    return true;
}

bool SendError(const BenchmarkCallback& callback, const std::wstring& message, BenchmarkResult* partial = nullptr)
{
    BenchmarkUpdate update;
    update.done = true;
    update.ok = false;
    update.message = message;
    update.percent = 100;
    if (partial != nullptr)
        update.result = *partial;
    callback(update);
    return false;
}
}

bool RunBenchmark(const BenchmarkConfig& config, const std::atomic_bool& cancel, const BenchmarkCallback& callback)
{
    BenchmarkUpdate initial;
    initial.percent = 0;
    initial.stage = Tr(L"检查可用空间…");
    callback(initial);

    ULARGE_INTEGER freeAvailable = {};
    ULARGE_INTEGER total = {};
    ULARGE_INTEGER totalFree = {};
    if (!::GetDiskFreeSpaceExW(config.volumeRoot.c_str(), &freeAvailable, &total, &totalFree))
    {
        wchar_t message[1024] = {};
        swprintf_s(message, Tr(L"无法读取卷可用空间：%s"), GetLastErrorMessage(::GetLastError()).c_str());
        return SendError(callback, message);
    }

    uint64_t required = config.largeBytes + 64ull * 1024 * 1024;
    if (freeAvailable.QuadPart < required)
    {
        wchar_t message[256] = {};
        swprintf_s(message, Tr(L"可用空间不足：需要约 %s，当前可用 %s。"),
            FormatBytesShort(required).c_str(), FormatBytesShort(freeAvailable.QuadPart).c_str());
        return SendError(callback, message);
    }

    TempBenchFile file;
    std::wstring error;
    if (!OpenTestFile(file, config, error))
        return SendError(callback, error);

    if (file.buffered)
    {
        BenchmarkUpdate update;
        update.percent = 0;
        update.stage = Tr(L"提示：文件系统不支持无缓冲 I/O，测试使用系统缓存。");
        callback(update);
    }

    struct Stage
    {
        const wchar_t* name;
        uint64_t bytes;
        uint32_t block;
        bool write;
        bool sequential;
    };
    uint64_t smallBytes = std::min(config.smallBytes, config.largeBytes);
    if (smallBytes < config.smallBlock)
        smallBytes = config.smallBlock;
    smallBytes -= smallBytes % config.smallBlock;

    const Stage stages[] =
    {
        { Tr(L"大文件顺序写入（1 MiB）"), config.largeBytes, config.largeBlock, true, true },
        { Tr(L"大文件顺序读取（1 MiB）"), config.largeBytes, config.largeBlock, false, true },
        { Tr(L"4K 随机写入"), smallBytes, config.smallBlock, true, false },
        { Tr(L"4K 随机读取"), smallBytes, config.smallBlock, false, false },
        { Tr(L"4K 顺序写入"), smallBytes, config.smallBlock, true, true },
        { Tr(L"4K 顺序读取"), smallBytes, config.smallBlock, false, true },
        { Tr(L"1 MiB 随机写入"), config.largeBytes, config.largeBlock, true, false },
        { Tr(L"1 MiB 随机读取"), config.largeBytes, config.largeBlock, false, false },
    };

    uint64_t totalBytes = 0;
    for (const auto& stage : stages)
        totalBytes += stage.bytes;

    uint64_t completed = 0;
    uint64_t seed = 0x123456789ABCDEFull;
    for (const auto& stage : stages)
    {
        if (cancel.load())
            return SendError(callback, Tr(L"测试已取消。"));

        BenchmarkUpdate update;
        update.percent = totalBytes == 0 ? 0 : static_cast<int>(completed * 100 / totalBytes);
        update.stage = stage.name;
        callback(update);

        BenchmarkResult result;
        if (!RunStage(file.handle, stage.name, stage.bytes, stage.block, stage.write, stage.sequential,
            seed++, cancel, callback, completed, totalBytes, result, error))
        {
            return SendError(callback, std::wstring(stage.name) + Tr(L"：") + error, &result);
        }

        completed += stage.bytes;
        update.percent = totalBytes == 0 ? 100 : static_cast<int>(completed * 100 / totalBytes);
        update.stage = stage.name;
        update.result = result;
        callback(update);
    }

    BenchmarkUpdate done;
    done.percent = 100;
    done.stage = Tr(L"测试完成");
    done.done = true;
    done.ok = true;
    done.message = Tr(L"所有测试已完成。测试文件已删除。");
    callback(done);
    return true;
}
