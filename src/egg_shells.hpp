#pragma once
#include "egg_signatures.hpp"
namespace egg {
using P=gb::P;
using gb::read;
using gb::readBytes;
using gb::parseBool;
using Factory=void*(*)(void*,void*,const wchar_t*,void*);
using Destroy=void*(*)(void*,uint32_t);
using Construct=void*(*)(void*,void*);
static constexpr uint32_t FactoryRva=0x11945e0,DestroyRva=0x412890,ConstructRva=0x411f60;
static constexpr uint32_t ShellReturnRva=0x41470d,ShellVtable=0x17bc3d8,InitVtable=0x17bc3c8;
struct Config { bool enabled=true,logging=true; uint32_t maxGroups=32; };
static Config config;
static P& image=gb::image;
static Factory originalFactory=nullptr;
static Destroy originalDestroy=nullptr;
static Construct originalConstruct=nullptr;
static std::atomic<bool> stopped{false};
static SRWLOCK stateLock=SRWLOCK_INIT;
#ifdef EGG_UNIT_TEST
static HANDLE& logFile=gb::logFile;
static uint64_t& logBytes=gb::logBytes;
static constexpr uint64_t LogLimit=gb::LogLimit;
#endif
struct Entry { P pointer=0; uint64_t generation=0; bool retiring=false; };
static std::array<Entry,2048> entries{};
struct Counters { uint64_t requests=0,allowed=0,blocked=0,created=0,destroyed=0,failed=0; uint32_t live=0,pending=0,peak=0; };
static Counters counters;
static uint64_t nextGeneration=0;
struct Token { size_t index=entries.size(); uint64_t generation=0; };

static uint32_t parseLimit(std::wstring value){
    const auto first=value.find_first_not_of(L" \t\r\n");if(first==std::wstring::npos)return 32;
    value=value.substr(first,value.find_last_not_of(L" \t\r\n")-first+1);
    uint32_t n=0;for(wchar_t c:value){if(c<L'0'||c>L'9'||n>1024)return 32;n=n*10+uint32_t(c-L'0');}
    return n<=1024?n:32;
}
static Config loadConfig(const std::wstring& path){
    wchar_t enabled[64]={},logging[64]={},limit[64]={};
    GetPrivateProfileStringW(L"EggShells",L"Enabled",L"1",enabled,64,path.c_str());
    GetPrivateProfileStringW(L"EggShells",L"MaxShellGroups",L"32",limit,64,path.c_str());
    GetPrivateProfileStringW(L"Logging",L"Enabled",L"1",logging,64,path.c_str());
    return {parseBool(enabled),parseBool(logging),parseLimit(limit)};
}
template<class... Args>static void message(const char* format,Args... args){
    if(!config.logging)return;
    char text[768]={};std::snprintf(text,sizeof(text),format,args...);gb::message("EGG_%s",text);
}
#ifdef EGG_UNIT_TEST
static void initializeLogging(const std::wstring& folder){gb::config.logging=config.logging;gb::initializeLogging(folder);}
#endif
static void stop(const char* reason){if(!stopped.exchange(true))message("LIMITER_STOP reason=%s original_creation_restored=1",reason);}
static Counters snapshot(){AcquireSRWLockShared(&stateLock);auto c=counters;ReleaseSRWLockShared(&stateLock);return c;}
static void report(const char* reason){const auto c=snapshot();message("BUDGET reason=%s requested=%llu allowed=%llu blocked=%llu created=%llu destroyed=%llu failed=%llu live=%u pending=%u peak=%u cap=%u",reason,(unsigned long long)c.requests,(unsigned long long)c.allowed,(unsigned long long)c.blocked,(unsigned long long)c.created,(unsigned long long)c.destroyed,(unsigned long long)c.failed,c.live,c.pending,c.peak,config.maxGroups);}
static bool sampled(uint64_t n){return n<=4||(n&&(n&(n-1))==0);}
// The only admitted site is GiantAntEgg's break_object branch. The enemy branch has a different return address.
static bool isShellRequest(P caller,const wchar_t* path,void* init){
    if(caller!=image+ShellReturnRva)return false;
    P vt=0;if(!read(P(init),vt)||vt!=image+InitVtable)return false;
    wchar_t name[96]={};
    // One bounded read per egg request, rather than a process-memory syscall for every character.
    if(!readBytes(P(path),name,sizeof(name)))return false;
    bool ended=false;for(wchar_t c:name){if(!c){ended=true;break;}}
    if(!ended)return false;
    return !_wcsicmp(name,L"app:/object/GiantAnt01Egg_Break.sgo")||
           !_wcsicmp(name,L"app:/object/GiantAnt02BEgg_Break.sgo")||
           !_wcsicmp(name,L"app:/object/GoldAntEgg_Break.sgo")||
           !_wcsicmp(name,L"app:/object/e619_squid_small_egg_break.sgo");
}
struct Reservation {
    bool held=false;
    P constructed=0;
    ~Reservation(){if(held){AcquireSRWLockExclusive(&stateLock);--counters.pending;ReleaseSRWLockExclusive(&stateLock);}}
};
static thread_local Reservation* currentReservation=nullptr;
struct CreationScope {
    Reservation* previous;
    explicit CreationScope(Reservation* r):previous(currentReservation){currentReservation=r;}
    ~CreationScope(){currentReservation=previous;}
};
static bool reserve(Reservation& r){
    AcquireSRWLockExclusive(&stateLock);++counters.requests;
    const bool ok=uint64_t(counters.live)+counters.pending<config.maxGroups;
    if(ok){++counters.allowed;++counters.pending;r.held=true;}else ++counters.blocked;
    const auto blocked=counters.blocked;ReleaseSRWLockExclusive(&stateLock);
    if(!ok&&sampled(blocked))report("blocked_before_factory");
    return ok;
}
static void registerCreated(Reservation& r,P object){
    bool full=true;uint64_t created=0;
    AcquireSRWLockExclusive(&stateLock);
    if(r.held){--counters.pending;r.held=false;}
    r.constructed=object;
    for(auto& e:entries){if(!e.pointer){e={object,++nextGeneration,false};++counters.live;++counters.created;counters.peak=(counters.live>counters.peak?counters.live:counters.peak);full=false;break;}}
    created=counters.created;ReleaseSRWLockExclusive(&stateLock);
    if(full)stop("tracking_capacity_exceeded");else if(sampled(created))report("created");
}
static void* hookConstruct(void* object,void* init){
    void* result=originalConstruct(object,init);
    auto* reservation=currentReservation;
    if(reservation&&reservation->held){
        P vt=0;
        if(result!=object||!read(P(object),vt)||vt!=image+ShellVtable)stop("constructed_shell_identity_mismatch");
        else registerCreated(*reservation,P(object));
    }
    return result;
}
static Token retiring(P object){
    Token token;
    AcquireSRWLockExclusive(&stateLock);
    for(size_t i=0;i<entries.size();++i){auto& e=entries[i];if(e.pointer==object&&!e.retiring){e.retiring=true;token={i,e.generation};break;}}
    ReleaseSRWLockExclusive(&stateLock);return token;
}
static void retired(Token t){
    bool empty=false;
    AcquireSRWLockExclusive(&stateLock);
    if(t.index<entries.size()){auto& e=entries[t.index];if(e.generation==t.generation&&e.retiring){e={};--counters.live;++counters.destroyed;empty=counters.live==0&&counters.pending==0;}}
    ReleaseSRWLockExclusive(&stateLock);
    if(empty)report("all_tracked_shells_destroyed");
}
static void* routeFactory(P caller,void* manager,void* transform,const wchar_t* path,void* init){
    if(!config.enabled||stopped.load(std::memory_order_relaxed)||!isShellRequest(caller,path,init))
        return originalFactory(manager,transform,path,init);
    Reservation reservation;
    if(!reserve(reservation))return nullptr; // Original caller has a null-result path; sound, cleanup and network bookkeeping remain in its body.
    CreationScope scope(&reservation);
    void* result=originalFactory(manager,transform,path,init);
    if(!result){AcquireSRWLockExclusive(&stateLock);++counters.failed;ReleaseSRWLockExclusive(&stateLock);return result;}
    P object=0;
    // Identity was registered at constructor return, before the factory publishes the object.
    // Compare pointer values only here; another native thread may already be destroying the shell.
    if(!read(P(result)+0x28,object)||!object||object!=reservation.constructed){stop("factory_result_identity_mismatch");return result;}
    return result;
}
__attribute__((noinline)) static void* hookFactory(void* manager,void* transform,const wchar_t* path,void* init){
    return routeFactory(P(__builtin_return_address(0)),manager,transform,path,init);
}
static void* hookDestroy(void* object,uint32_t flags){
    const auto token=retiring(P(object));
    void* result=originalDestroy(object,flags); // May free object. Do not read it after this call.
    retired(token);return result;
}

static bool compatibleBytes(){for(const auto& s:eggTarget::signatures){std::array<uint8_t,32> b{};if(!readBytes(image+s.rva,b.data(),b.size())||b!=s.bytes)return false;}return true;}
}
