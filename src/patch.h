#pragma once
#include <windows.h>
#include <cstddef>

namespace mipfix {
enum Result : DWORD { Ok=0, WrongProcess=1, WrongBuild=2, BytesMismatch=3,
    ProtectionFailed=4, CacheFlushFailed=5, InconsistentState=6, Pending=256 };
constexpr DWORD ImageSize=0x0250a000;
constexpr DWORD TimeStamp=0x4bf33be6;
struct Guard { DWORD rva; const BYTE* bytes; size_t length; int changedIndex; BYTE replacement; };
const Guard* Guards(size_t& count);
Result Change(BYTE* image, bool enable);
const char* Describe(Result result);
}
