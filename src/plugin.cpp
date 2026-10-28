#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cwctype>
#include <string>
#include <MinHook.h>
#include "target_signatures.hpp"
#include "gate_signatures.hpp"

namespace gb {
using P=uintptr_t;
using Unary=void(*)(void*);
using Setter=void(*)(void*,bool);
using Register=void(*)(void*,void*,void*);
struct Config{bool full=true,budget=true,logging=true;};
static Config config;
static HMODULE selfModule=nullptr;
static P image=0;
static Unary originalBegin=nullptr,originalLight=nullptr,nativeFull=nullptr;
static Setter originalSetter=nullptr;
static Register originalRegister=nullptr;
static std::atomic<bool> fullStopped{false},budgetStopped{false};
static std::atomic<unsigned long> missionStarts{0};
static HANDLE logFile=INVALID_HANDLE_VALUE;
static SRWLOCK logLock=SRWLOCK_INIT;
static uint64_t logBytes=0;
static constexpr uint64_t LogLimit=2*1024*1024;

static bool readBytes(P address,void* out,size_t size){
    if(!size)return true;
    if(address<0x10000||size>8*1024*1024||address+size<address)return false;
    SIZE_T copied=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out,size,&copied)&&copied==size;
}
template<class T>static bool read(P address,T& value){return readBytes(address,&value,sizeof(value));}
static bool parseBool(std::wstring value,bool fallback=true){
    const auto begin=value.find_first_not_of(L" \t\r\n");
    if(begin==std::wstring::npos)return fallback;
    value=value.substr(begin,value.find_last_not_of(L" \t\r\n")-begin+1);
    for(auto& ch:value)ch=wchar_t(towlower(ch));
    if(value==L"1"||value==L"true"||value==L"yes"||value==L"on")return true;
    if(value==L"0"||value==L"false"||value==L"no"||value==L"off")return false;
    return fallback;
}
static Config loadConfig(const std::wstring& path){
    auto boolean=[&](const wchar_t* section,const wchar_t* key){
        wchar_t value[64]={};GetPrivateProfileStringW(section,key,L"1",value,64,path.c_str());
        return parseBool(value,true);
    };
    return {boolean(L"Optimization",L"FullLightWeightMode"),
            boolean(L"Optimization",L"AcceptGeneratorObjects"),
            boolean(L"Logging",L"Enabled")};
}
static void message(const char* format,...){
    if(!config.logging||logFile==INVALID_HANDLE_VALUE)return;
    char body[768]={},line[900]={};va_list args;va_start(args,format);
    std::vsnprintf(body,sizeof(body),format,args);va_end(args);
    SYSTEMTIME now;GetLocalTime(&now);
    const int length=std::snprintf(line,sizeof(line),"[%04u-%02u-%02u %02u:%02u:%02u] %s\r\n",
        now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,body);
    if(length<=0)return;
    const DWORD size=DWORD((size_t(length)<sizeof(line))?length:sizeof(line)-1);
    AcquireSRWLockExclusive(&logLock);
    if(logBytes+size<=LogLimit){DWORD written=0;if(WriteFile(logFile,line,size,&written,nullptr))logBytes+=written;}
    ReleaseSRWLockExclusive(&logLock);
}
static void initializeLogging(const std::wstring& folder){
    if(!config.logging)return;
    const auto path=folder+L"EDF6GeneratorBodyBudget.log";
    // A single local append-only log. Failure to open it never disables optimization.
    logFile=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,
                       nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(logFile!=INVALID_HANDLE_VALUE){LARGE_INTEGER size{};if(GetFileSizeEx(logFile,&size))logBytes=uint64_t(size.QuadPart);}
}
static void stopFull(const char* reason){if(!fullStopped.exchange(true))message("FULL_STOP reason=%s original_game_calls_continue=1",reason);}
static void stopBudget(const char* reason){if(!budgetStopped.exchange(true))message("BUDGET_STOP reason=%s original_game_calls_continue=1",reason);}
static void ensureFull(const char* reason){
    if(!config.full||fullStopped.load())return;
    P state=0,again=0;uint32_t before=0,after=0;
    if(!read(image+0x20b2890,state)||!state)return; // Not initialized: retry at the next game callback.
    if(!read(state+0x40,before)||before>2){stopFull("invalid_mode_state");return;}
    if(before==2)return;
    if(!nativeFull){stopFull("native_function_missing");return;}
    if(!read(image+0x20b2890,again)||again!=state){stopFull("state_changed");return;}
    nativeFull(nullptr); // The verified native function does not use its context argument.
    if(!read(image+0x20b2890,again)||again!=state||!read(state+0x40,after)||after!=2){stopFull("native_setting_not_verified");return;}
    message("FULL_APPLIED reason=%s before=%u after=%u thread=%lu",reason,before,after,GetCurrentThreadId());
}
static void ensureBudget(P manager){
    if(!config.budget||budgetStopped.load())return;
    P global=0,vt=0,again=0;uint32_t before=0,after=0;
    if(!read(image+0x20b30e0,global)||!global){stopBudget("manager_missing");return;}
    if(global!=manager)return;
    if(!read(manager,vt)||vt!=image+0x17e7f78||!read(manager+0x18,before)){stopBudget("invalid_manager");return;}
    if(before&1u)return;
    if(!originalSetter||!read(image+0x20b30e0,again)||again!=manager){stopBudget("manager_changed_or_function_missing");return;}
    originalSetter(nullptr,true);
    if(!read(image+0x20b30e0,again)||again!=manager||!read(manager+0x18,after)||
       !(after&1u)||(before&~1u)!=(after&~1u)){stopBudget("native_setting_not_verified");return;}
    message("BUDGET_APPLIED before=%08X after=%08X thread=%lu",before,after,GetCurrentThreadId());
}
static void hookBegin(void* context){
    originalBegin(context);
    const auto number=missionStarts.fetch_add(1)+1;
    message("MISSION_BEGIN sequence=%lu full=%u budget=%u",number,unsigned(config.full),unsigned(config.budget));
    ensureFull("BeginLoading");
}
static void hookLight(void* context){originalLight(context);ensureFull("SetLightWeightMode");}
static void hookSetter(void* context,bool requested){
    const bool effective=config.budget&&!budgetStopped.load()?true:requested;
    originalSetter(context,effective);
    if(effective!=requested)message("BUDGET_REQUEST_OVERRIDDEN requested=0 effective=1");
}
static void hookRegister(void* manager,void* controller,void* weak){
    ensureFull("registration_fallback");
    ensureBudget(P(manager));
    originalRegister(manager,controller,weak);
}

static bool hashFile(const wchar_t* path,std::array<uint8_t,32>& out){
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                           nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    if(ok)ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
    std::array<uint8_t,65536> buffer{};DWORD count=0;
    while(ok){
        if(!ReadFile(file,buffer.data(),DWORD(buffer.size()),&count,nullptr)){ok=false;break;}
        if(!count)break;
        if(BCryptHashData(hash,buffer.data(),count,0)<0){ok=false;break;}
    }
    if(ok)ok=BCryptFinishHash(hash,out.data(),ULONG(out.size()),0)>=0;
    if(hash)BCryptDestroyHash(hash);
    if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    CloseHandle(file);return ok;
}
}
#include "egg_shells.hpp"
namespace gb {
static bool compatibleBytes(){
    for(const auto& s:target::signatures){
        const bool full=s.rva==0x1acb10||s.rva==0x1ba150||s.rva==0x1ba160;
        if(!(full?config.full:config.budget))continue;
        std::array<uint8_t,32> bytes{};if(!readBytes(image+s.rva,bytes.data(),32)||bytes!=s.bytes)return false;
    }
    if(config.budget)for(const auto& s:gateSignatures){std::array<uint8_t,32> bytes{};if(!readBytes(image+s.rva,bytes.data(),32)||bytes!=s.bytes)return false;}
    return !egg::config.enabled||egg::compatibleBytes();
}
struct HookSpec{uint32_t rva;void* replacement;void** original;};
static size_t selectHooks(std::array<HookSpec,7>& hooks){
    size_t count=0;
    if(config.full){
        hooks[count++]={0x1acb10,reinterpret_cast<void*>(hookBegin),reinterpret_cast<void**>(&originalBegin)};
        hooks[count++]={0x1ba160,reinterpret_cast<void*>(hookLight),reinterpret_cast<void**>(&originalLight)};
    }
    if(config.budget){
        hooks[count++]={0x6eed10,reinterpret_cast<void*>(hookRegister),reinterpret_cast<void**>(&originalRegister)};
        hooks[count++]={0x1baa20,reinterpret_cast<void*>(hookSetter),reinterpret_cast<void**>(&originalSetter)};
    }
    if(egg::config.enabled){
        hooks[count++]={egg::FactoryRva,reinterpret_cast<void*>(egg::hookFactory),reinterpret_cast<void**>(&egg::originalFactory)};
        hooks[count++]={egg::ConstructRva,reinterpret_cast<void*>(egg::hookConstruct),reinterpret_cast<void**>(&egg::originalConstruct)};
        hooks[count++]={egg::DestroyRva,reinterpret_cast<void*>(egg::hookDestroy),reinterpret_cast<void**>(&egg::originalDestroy)};
    }
    return count;
}
static bool installHooks(){
    std::array<HookSpec,7> hooks{};const size_t count=selectHooks(hooks);
    if(!count)return true;
    if(MH_Initialize()!=MH_OK)return false;
    size_t created=0;
    for(size_t i=0;i<count;++i){auto& hook=hooks[i];
        if(MH_CreateHook(reinterpret_cast<void*>(image+hook.rva),hook.replacement,hook.original)!=MH_OK){
            for(size_t j=0;j<created;++j)MH_RemoveHook(reinterpret_cast<void*>(image+hooks[j].rva));
            MH_Uninitialize();return false;
        }++created;
    }
    bool ok=true;
    for(size_t i=0;i<count;++i)if(MH_QueueEnableHook(reinterpret_cast<void*>(image+hooks[i].rva))!=MH_OK)ok=false;
    if(!ok||MH_ApplyQueued()!=MH_OK){
        for(size_t i=0;i<count;++i){MH_DisableHook(reinterpret_cast<void*>(image+hooks[i].rva));MH_RemoveHook(reinterpret_cast<void*>(image+hooks[i].rva));}
        MH_Uninitialize();return false;
    }
    return true;
}
static DWORD WINAPI worker(void*){
    try{
        wchar_t path[32768]={};if(!GetModuleFileNameW(selfModule,path,32768))return 0;
        std::wstring folder(path);folder.resize(folder.find_last_of(L"\\/")+1);
        config=loadConfig(folder+L"EDF6GeneratorBodyBudget.ini");
        egg::config=egg::loadConfig(folder+L"EDF6GeneratorBodyBudget.ini");
        initializeLogging(folder);
        message("START version=0.3.0 full=%u budget=%u logging=%u pid=%lu",unsigned(config.full),unsigned(config.budget),unsigned(config.logging),GetCurrentProcessId());
        message("EGG_CONFIG enabled=%u max_shell_groups=%u shared_ant_and_e619=1",unsigned(egg::config.enabled),egg::config.maxGroups);
        if(!config.full&&!config.budget&&!egg::config.enabled){message("DISABLED no_hooks=1");return 0;}
        for(const wchar_t* name:{L"EDF6BodyBudgetGateProbe.dll",L"EDF6MixedBodyBudgetProbe.dll",L"EDF6BodyBudgetProbe.dll",
            L"EDF6AntLegExit.dll",L"EDF6SingleLegExit.dll",L"EDF6BodyProbe.dll",L"EDF6AntLimbFilter.dll",L"EDF6AntZeroLimbs.dll",L"EDF6InsectZeroLimbs.dll"}){
            if(GetFileAttributesW((folder+name).c_str())!=INVALID_FILE_ATTRIBUTES||GetModuleHandleW(name)){
                message("REFUSED conflicting_old_experiment=1");return 0;
            }
        }
        if(egg::config.enabled&&(GetFileAttributesW((folder+L"EDF6AntEggShellBudget.dll").c_str())!=INVALID_FILE_ATTRIBUTES||GetModuleHandleW(L"EDF6AntEggShellBudget.dll"))){
            message("REFUSED standalone_egg_plugin_present=1 remove_EDF6AntEggShellBudget.dll_before_using_integrated_egg_feature=1");return 0;
        }
        HMODULE game=nullptr;
        for(int attempt=0;attempt<600&&!game;++attempt){game=GetModuleHandleW(L"EDF.dll");if(!game)Sleep(100);}
        if(!game){message("REFUSED EDF.dll_missing=1");return 0;}
        image=P(game);wchar_t gamePath[32768]={};
        if(!GetModuleFileNameW(game,gamePath,32768))return 0;
        std::array<uint8_t,32> digest{};
        if(!hashFile(gamePath,digest)||digest!=target::sha256){message("REFUSED unsupported_EDF.dll_hash=1");return 0;}
        if(!compatibleBytes()){message("REFUSED game_entry_or_gate_modified=1");return 0;}
        nativeFull=reinterpret_cast<Unary>(image+0x1ba150);
        if(!installHooks()){message("REFUSED hook_installation_failed=1");return 0;}
        std::array<HookSpec,7> active{};message("READY version=0.3.0 hooks=%u full=%u budget=%u egg_shells=%u max_shell_groups=%u",unsigned(selectHooks(active)),unsigned(config.full),unsigned(config.budget),unsigned(egg::config.enabled),egg::config.maxGroups);
        // Initialization finishes here. No periodic worker or foreign-process access.
    }catch(...){fullStopped=true;budgetStopped=true;egg::stopped=true;message("INITIALIZATION_FAILED");}
    return 0;
}
}
struct PluginVersion{unsigned short build,patch,minor,major;};
struct PluginInfo{unsigned long infoVersion;const char* name;PluginVersion version;};
static_assert(sizeof(PluginInfo)==24);
extern "C" __declspec(dllexport) bool EML6_Load(PluginInfo* info){
    if(!info)return false;
    info->infoVersion=1;info->name="EDF6 Corpse and Egg Shell Optimization";info->version={0,0,3,0};
    static std::atomic<bool> started{false};if(started.exchange(true))return true;
    HANDLE thread=CreateThread(nullptr,0,gb::worker,nullptr,0,nullptr);
    if(!thread){started=false;return false;}CloseHandle(thread);return true;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){gb::selfModule=module;DisableThreadLibraryCalls(module);}return TRUE;
}
