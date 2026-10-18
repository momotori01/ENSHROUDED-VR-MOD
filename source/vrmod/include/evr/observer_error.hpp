#pragma once
#include <windows.h>

namespace evr
{
// ReadProcessMemory can change a render/game thread's last-error value.
// Observation must not leak its diagnostic errors into the original code.
struct ObserverLastError
{
    const DWORD value = GetLastError();
    ~ObserverLastError()
    {
        SetLastError(value);
    }
};
} // namespace evr
