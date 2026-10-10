#include "patch.h"
#include <cstring>

namespace mipfix {
// ESI points to UObject + 0x10 here. EBP=1, EBX=0. Keep the residency bit.
static const BYTE material[] = {0x39,0x5c,0x24,0x2c,0x74,0x09,0x83,0x4e,0x6c,0x04,
    0x89,0x6e,0x70,0xeb,0x07,0x83,0x66,0x6c,0xfb,0x89,0x5e,0x70};
static const BYTE texture[] = {0x83,0x7c,0x24,0x04,0x00,0x74,0x0e,0x83,0x48,0x6c,0x04,
    0xc7,0x40,0x70,0x01,0x00,0x00,0x00,0xc2,0x04,0x00,0x83,0x60,0x6c,0xfb,
    0xc7,0x40,0x70,0x00,0x00,0x00,0x00,0xc2,0x04,0x00};
static const BYTE zeroRegs[]={0x33,0xdb,0x33,0xf6};
static const BYTE oneReg[]={0x8d,0x6b,0x01};
static const BYTE playerCall[]={0xe8,0x54,0xb7,0x04,0x00};
static const BYTE forcedMips[]={0x8b,0x41,0x68,0x2b,0x81,0x80,0x00,0x00,0x00,
    0x33,0xd2,0x85,0xc0,0x0f,0x9e,0xc2,0x4a,0x23,0xd0,0x8b,0xfa};
static const Guard guards[]={
    {0x002ec1b1,material,sizeof(material),11,0x5e}, // mov [esi+70],ebx
    {0x002ec220,texture,sizeof(texture),14,0x00},  // mov [eax+70],0
    {0x002ec12a,zeroRegs,sizeof(zeroRegs),-1,0},
    {0x002ec153,oneReg,sizeof(oneReg),-1,0},
    {0x002a09c7,playerCall,sizeof(playerCall),-1,0},
    {0x00c4202f,forcedMips,sizeof(forcedMips),-1,0}
};
const Guard* Guards(size_t& count) {count=sizeof(guards)/sizeof(*guards); return guards;}
const char* Describe(Result r) {
    switch(r) {
    case Ok:return "Success";
    case WrongProcess:return "Refused: host executable is not TWFC.exe";
    case WrongBuild:return "Refused: unsupported executable header/build";
    case BytesMismatch:return "Refused: instruction signature differs";
    case ProtectionFailed:return "Failed: cannot change code-page protection";
    case CacheFlushFailed:return "Failed: instruction-cache flush failed; writes rolled back";
    case InconsistentState:return "Refused: patch bytes are in inconsistent states";
    default:return "Initialization pending";
    }
}
Result Change(BYTE* image, bool enable) {
    if(!image) return WrongBuild;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<sizeof(*dos) || dos->e_lfanew>4096)
        return WrongBuild;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(image+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 ||
       nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
       nt->FileHeader.TimeDateStamp!=TimeStamp || nt->OptionalHeader.SizeOfImage!=ImageSize)
        return WrongBuild;
    int original=0,patched=0;
    for(const auto& g:guards) {
        if(g.rva+g.length>ImageSize) return WrongBuild;
        for(size_t j=0;j<g.length;++j) {
            const BYTE b=image[g.rva+j];
            if(static_cast<int>(j)==g.changedIndex) {
                if(b==g.bytes[j]) ++original;
                else if(b==g.replacement) ++patched;
                else return BytesMismatch;
            } else if(b!=g.bytes[j]) return BytesMismatch;
        }
    }
    if(original && patched) return InconsistentState;
    if((enable && patched==2) || (!enable && original==2)) return Ok;
    DWORD protection[2]={};
    BYTE* targets[2]={image+guards[0].rva+guards[0].changedIndex,
                      image+guards[1].rva+guards[1].changedIndex};
    // Both patches are on the same page, but also work if page placement changes.
    for(int i=0;i<2;++i) {
        if(!VirtualProtect(targets[i],1,PAGE_EXECUTE_READWRITE,&protection[i])) {
            for(int j=i-1;j>=0;--j){DWORD ignored;VirtualProtect(targets[j],1,protection[j],&ignored);}
            return ProtectionFailed;
        }
    }
    const BYTE before[2]={*targets[0],*targets[1]};
    for(int i=0;i<2;++i) {
        // An x86 byte store is atomic. Instruction length and boundaries do not change.
        *reinterpret_cast<volatile BYTE*>(targets[i])=enable ? guards[i].replacement :
            guards[i].bytes[guards[i].changedIndex];
    }
    MemoryBarrier();
    Result result=Ok;
    if(!FlushInstructionCache(GetCurrentProcess(),image+guards[0].rva,
        guards[1].rva+guards[1].length-guards[0].rva)) {
        for(int i=0;i<2;++i) *reinterpret_cast<volatile BYTE*>(targets[i])=before[i];
        FlushInstructionCache(GetCurrentProcess(),image+guards[0].rva,
            guards[1].rva+guards[1].length-guards[0].rva);
        result=CacheFlushFailed;
    }
    for(int i=1;i>=0;--i) {DWORD ignored;
        if(!VirtualProtect(targets[i],1,protection[i],&ignored)) result=ProtectionFailed;
    }
    return result;
}
}
