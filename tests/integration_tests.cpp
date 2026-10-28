#include "../src/plugin.cpp"
#include <filesystem>
#include <fstream>
#include <set>
#include <cstdio>
using namespace gb;
static unsigned checks=0;
static void verify(bool ok,const char* label){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}
static std::array<unsigned char,128> mode{},manager{};
static unsigned fullCalls=0,budgetCalls=0,beginCalls=0,registerCalls=0;
static void fullMock(void*){++fullCalls;uint32_t n=2;std::memcpy(mode.data()+0x40,&n,4);}
static void setterMock(void*,bool on){++budgetCalls;uint32_t flags=0;std::memcpy(&flags,manager.data()+0x18,4);flags=(flags&~1u)|unsigned(on);std::memcpy(manager.data()+0x18,&flags,4);}
static void beginMock(void*){++beginCalls;}
static void registerMock(void*,void*,void*){++registerCalls;}
template<class T>static void put(P at,T value){std::memcpy(reinterpret_cast<void*>(at),&value,sizeof(value));}
static void reset(bool f,bool b,bool e){
    config={f,b,false};egg::config={e,false,32};fullStopped=false;budgetStopped=false;egg::stopped=false;
    mode={};manager={};fullCalls=budgetCalls=beginCalls=registerCalls=0;
    put(image+0x20b2890,P(mode.data()));put(image+0x20b30e0,P(manager.data()));put(P(manager.data()),image+0x17e7f78);
    nativeFull=fullMock;originalSetter=setterMock;originalBegin=beginMock;originalRegister=registerMock;
}
int wmain(int argc,wchar_t** argv){
    if(argc!=2)return 2;
    auto root=std::filesystem::path(argv[1]);image=P(VirtualAlloc(nullptr,0x2200000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));verify(image!=0,"fixture allocation");
    for(const auto& s:target::signatures)std::memcpy(reinterpret_cast<void*>(image+s.rva),s.bytes.data(),32);
    for(const auto& s:gateSignatures)std::memcpy(reinterpret_cast<void*>(image+s.rva),s.bytes.data(),32);
    for(const auto& s:eggTarget::signatures)std::memcpy(reinterpret_cast<void*>(image+s.rva),s.bytes.data(),32);
    for(bool f:{false,true})for(bool b:{false,true})for(bool e:{false,true}){
        reset(f,b,e);std::array<HookSpec,7> hooks{};size_t count=selectHooks(hooks);
        verify(count==2u*unsigned(f)+2u*unsigned(b)+3u*unsigned(e),"three independent feature hook selection");
        std::set<uint32_t> unique;for(size_t i=0;i<count;++i)unique.insert(hooks[i].rva);verify(unique.size()==count,"one unique hook per address");
        verify(compatibleBytes(),"selected feature byte windows accepted");
        auto* shellByte=reinterpret_cast<uint8_t*>(image+egg::ConstructRva);*shellByte^=1;verify(compatibleBytes()==!e,"disabled egg feature does not require or hook egg bytes");*shellByte^=1;
        hookBegin(nullptr);hookRegister(manager.data(),nullptr,nullptr);
        verify(beginCalls==1&&registerCalls==1&&fullCalls==unsigned(f)&&budgetCalls==unsigned(b),"corpse functions independent of egg switch");
        egg::stopped=true;ensureFull("test");ensureBudget(P(manager.data()));verify(fullCalls==unsigned(f)&&budgetCalls==unsigned(b),"egg fail stop does not stop corpse features");egg::stopped=false;
        verify(installHooks(),"combined MinHook initialization and atomic installation");
        if(count){for(size_t i=0;i<count;++i)verify(MH_DisableHook(reinterpret_cast<void*>(image+hooks[i].rva))==MH_OK,"selected hook can disable");verify(MH_Uninitialize()==MH_OK,"combined hooks uninitialize once");}
        verify(compatibleBytes(),"entry bytes restored after combined hook removal");
        auto ini=root/(std::to_wstring(unsigned(f))+std::to_wstring(unsigned(b))+std::to_wstring(unsigned(e))+L".ini");
        std::ofstream(ini)<<"[Optimization]\nFullLightWeightMode="<<f<<"\nAcceptGeneratorObjects="<<b<<"\n[EggShells]\nEnabled="<<e<<"\nMaxShellGroups=11\n[Logging]\nEnabled=0\n";
        const auto mainCfg=loadConfig(ini.wstring());const auto shellCfg=egg::loadConfig(ini.wstring());
        verify(mainCfg.full==f&&mainCfg.budget==b&&!mainCfg.logging&&shellCfg.enabled==e&&!shellCfg.logging&&shellCfg.maxGroups==11,"single INI controls all features and one log");
    }
    const auto old=root/L"old-020.ini";std::ofstream(old)<<"[Optimization]\nFullLightWeightMode=0\nAcceptGeneratorObjects=1\n[Logging]\nEnabled=0\n";
    const auto oldMain=loadConfig(old.wstring());const auto oldEgg=egg::loadConfig(old.wstring());
    verify(!oldMain.full&&oldMain.budget&&!oldMain.logging&&oldEgg.enabled&&oldEgg.maxGroups==32&&!oldEgg.logging,"old INI keeps existing switches and defaults new egg option on");
    const auto missing=root/L"missing.ini";auto mainCfg=loadConfig(missing.wstring());auto shellCfg=egg::loadConfig(missing.wstring());
    verify(mainCfg.full&&mainCfg.budget&&mainCfg.logging&&shellCfg.enabled&&shellCfg.logging&&shellCfg.maxGroups==32,"missing file defaults all features on and cap 32");
    VirtualFree(reinterpret_cast<void*>(image),0,MEM_RELEASE);
    std::printf("PASS %u integration assertions: all eight feature combinations, actual seven-hook installation/restoration, single INI, old config defaults and fail-stop isolation; no game executed.\n",checks);
    return 0;
}
