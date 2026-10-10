#include "../src/patch.h"
#include <cstdio>
#include <cstring>
#include <vector>

static int failures;
static void Check(bool value,const char* label){printf("%s %s\n",value?"PASS":"FAIL",label);if(!value)++failures;}
int main() {
    BYTE* image=static_cast<BYTE*>(VirtualAlloc(nullptr,mipfix::ImageSize,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!image) return 1;
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x100;
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(image+0x100);
    nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.TimeDateStamp=mipfix::TimeStamp;nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.SizeOfImage=mipfix::ImageSize;
    size_t count;const auto* guards=mipfix::Guards(count);
    for(size_t i=0;i<count;++i)memcpy(image+guards[i].rva,guards[i].bytes,guards[i].length);
    std::vector<BYTE> original(image,image+mipfix::ImageSize);
    DWORD old;VirtualProtect(image,mipfix::ImageSize,PAGE_EXECUTE_READ,&old);
    Check(mipfix::Change(image,true)==mipfix::Ok,"apply at a relocated image base with RX pages");
    size_t changed=0;for(size_t i=0;i<original.size();++i) if(original[i]!=image[i])++changed;
    Check(changed==2,"exactly two bytes change; all other bytes remain intact");
    Check(image[0x2ec1bc]==0x5e && image[0x2ec22e]==0,"correct register and immediate replacements");
    Check(mipfix::Change(image,true)==mipfix::Ok,"repeated apply is idempotent");
    MEMORY_BASIC_INFORMATION mbi{};VirtualQuery(image+0x2ec1bc,&mbi,sizeof(mbi));
    Check(mbi.Protect==PAGE_EXECUTE_READ,"original code page protection restored");
    Check(mipfix::Change(image,false)==mipfix::Ok,"restore succeeds");
    Check(memcmp(original.data(),image,original.size())==0,"restore reproduces the original image exactly");
    Check(mipfix::Change(image,false)==mipfix::Ok,"repeated restore is idempotent");
    VirtualProtect(image,mipfix::ImageSize,PAGE_READWRITE,&old);
    nt->FileHeader.TimeDateStamp^=1;
    Check(mipfix::Change(image,true)==mipfix::WrongBuild,"unsupported build rejected");
    Check(image[0x2ec1bc]==0x6e && image[0x2ec22e]==1,"build rejection leaves patches untouched");
    nt->FileHeader.TimeDateStamp^=1;
    image[0xc42032]^=1;
    Check(mipfix::Change(image,true)==mipfix::BytesMismatch,"changed streaming consumer signature rejected");
    Check(image[0x2ec1bc]==0x6e && image[0x2ec22e]==1,"signature rejection makes no partial patch");
    image[0xc42032]^=1;
    image[0x2ec1bc]=0x5e;
    Check(mipfix::Change(image,true)==mipfix::InconsistentState,"partially patched image rejected");
    image[0x2ec1bc]=0x90;
    Check(mipfix::Change(image,false)==mipfix::BytesMismatch,"restore refuses to overwrite another modification");
    VirtualFree(image,0,MEM_RELEASE);
    printf("%d failure(s)\n",failures);return failures?1:0;
}
