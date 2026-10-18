#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace evr::diagnostic
{
// Metadata only. External mutex protects this object. A successful submit does
// not prove completion: callers must establish GPU completion separately.
struct ImageSubmission
{
    struct Change { uint64_t command = 0; int32_t layout = 0; bool supported = true; };
    // Recording callbacks must not allocate: an allocation failure after a prior
    // successful frame could otherwise leave old evidence looking usable.
    std::array<Change, 128> pending{};
    size_t pendingCount = 0;
    int32_t layout = 0;
    uint32_t family = UINT32_MAX;
    uint64_t revision = 0, submission = 0;
    bool known = false, poisoned = false;
    void record(uint64_t command, int32_t nextLayout, bool supported) noexcept
    {
        ++revision;
        if (!command) { poisoned = true; return; }
        for (size_t i = 0; i < pendingCount; ++i) if (pending[i].command == command) {
            pending[i].layout = nextLayout; pending[i].supported &= supported; return;
        }
        if (pendingCount == pending.size()) { poisoned = true; return; }
        pending[pendingCount++] = {command, nextLayout, supported};
    }
    void reset(uint64_t command) noexcept
    {
        for (size_t i = 0; i < pendingCount; ++i) if (pending[i].command == command) {
            pending[i] = pending[--pendingCount]; known = false; return;
        }
    }
    bool submit(uint64_t command, uint32_t queueFamily, uint64_t serial, bool success) noexcept
    {
        for (size_t i = 0; i < pendingCount; ++i) if (pending[i].command == command) {
            if (!success) { known = false; poisoned = true; return true; }
            layout = pending[i].layout; family = queueFamily; submission = serial;
            poisoned |= !pending[i].supported; known = true;
            pending[i] = pending[--pendingCount]; return true;
        }
        return false;
    }
    bool ready() const noexcept { return known && !poisoned && pendingCount == 0; }
};
}
