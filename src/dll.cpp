#include "patch.h"
#include <cstdio>
#include <cwchar>
#include <cstring>

static HMODULE selfModule;
static SRWLOCK fixLock=SRWLOCK_INIT;
static volatile LONG status=mipfix::Pending;
static void Log(mipfix::Result r, bool enable) {
    char text[256];
    sprintf_s(text,"TWFC_PlayerMipFix: %s: %s (code %lu)\r\n",
              enable?"apply":"restore",mipfix::Describe(r),static_cast<DWORD>(r));
    OutputDebugStringA(text);
    wchar_t path[32768]={};
    DWORD n=GetModuleFileNameW(selfModule,path,32768);
    if(!n || n>=32768) return;
    auto* slash=wcsrchr(path,L'\\');
    if(!slash) return;
    wcscpy_s(slash+1,32768-static_cast<size_t>(slash+1-path),L"TWFC_PlayerMipFix.log");
    HANDLE f=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,
                        OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f!=INVALID_HANDLE_VALUE){DWORD written;WriteFile(f,text,static_cast<DWORD>(strlen(text)),&written,nullptr);CloseHandle(f);}
}
static DWORD SetFix(bool enable) {
    AcquireSRWLockExclusive(&fixLock);
    wchar_t path[32768]={};
    DWORD n=GetModuleFileNameW(nullptr,path,32768);
    const wchar_t* name=wcsrchr(path,L'\\');
    mipfix::Result result=mipfix::WrongProcess;
    if(n && n<32768 && _wcsicmp(name?name+1:path,L"TWFC.exe")==0)
        result=mipfix::Change(reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr)),enable);
    InterlockedExchange(&status,result);
    Log(result,enable);
    ReleaseSRWLockExclusive(&fixLock);
    return result;
}
extern "C" DWORD WINAPI ApplyFix(){return SetFix(true);}
extern "C" DWORD WINAPI RestoreFix(){return SetFix(false);}
extern "C" DWORD WINAPI GetFixStatus(){return static_cast<DWORD>(InterlockedCompareExchange(&status,0,0));}
static DWORD WINAPI Initialize(void*) {
    // Prevent unloading while initialization or exported operations are executing.
    // There is no detour, trampoline, engine callback, or persistent worker.
    HMODULE pinned;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                         reinterpret_cast<LPCWSTR>(&Initialize),&pinned)) {
        InterlockedExchange(&status,mipfix::ProtectionFailed);
        Log(mipfix::ProtectionFailed,true);
        return 1;
    }
    return ApplyFix();
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        selfModule=instance;
        // /MT has static CRT TLS; do not call DisableThreadLibraryCalls.
        HANDLE thread=CreateThread(nullptr,0,Initialize,nullptr,0,nullptr);
        if(thread) CloseHandle(thread);
        else InterlockedExchange(&status,mipfix::ProtectionFailed);
    }
    return TRUE;
}
