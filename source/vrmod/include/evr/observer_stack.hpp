#pragma once
#include <array>
#include <cstdint>
#include <cstring>

namespace evr
{
// At the validated 0x266380 camera site RDI holds the task-query context.
// Read our saved register, never infer a task pointer from an unrelated stack slot.
inline uintptr_t observerSavedRdi(uintptr_t originalRsp) noexcept
{
    uintptr_t result{};
    std::memcpy(&result, reinterpret_cast<const void *>(originalRsp - 64), sizeof(result));
    return result;
}
// Only call from camera_stub.asm's observer callback. These are offsets into
// that stub's own saved registers, not offsets into game objects.
inline std::array<uintptr_t, 5> observerEntryArguments(uintptr_t originalRsp) noexcept
{
    std::array<uintptr_t, 5> result{};
    constexpr intptr_t offsets[]{-24, -32, -72, -80, 40}; // RCX, RDX, R8, R9, stack arg5
    for (size_t i = 0; i < result.size(); ++i)
        std::memcpy(&result[i], reinterpret_cast<const void *>(originalRsp + offsets[i]), sizeof(uintptr_t));
    return result;
}
} // namespace evr
