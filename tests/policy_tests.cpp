#include "../src/plugin.cpp"
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
using namespace gb;
static unsigned checks=0,setCalls=0,fullCalls=0,beginCalls=0,lightCalls=0,regCalls=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error("line "+std::to_string(__LINE__)+": " #x);}while(0)
template<class T>void put(P p,size_t offset,T value){std::memcpy(reinterpret_cast<void*>(p+offset),&value,sizeof(value));}
static std::array<uint8_t,0x80> state{},manager{},replacement{};
static void* contextSeen=nullptr;static bool requestedSeen=false;
static void* regA=nullptr;static void* regB=nullptr;static void* regC=nullptr;
static int fullBehavior=0,budgetBehavior=0;
static unsigned eventOrder=0,beginOrder=0,fullOrder=0;
static uint32_t get(P at){uint32_t value=0;CHECK(read(at,value));return value;}
static void full(void* context){
    ++fullCalls;fullOrder=++eventOrder;CHECK(context==nullptr);
    if(fullBehavior==1)return;
    if(fullBehavior==3)throw std::runtime_error("native full exception");
    put<uint32_t>(P(state.data()),0x40,2);
    if(fullBehavior==2)put(image,0x20b2890,P(replacement.data()));
}
static void setter(void* context,bool desired){
    ++setCalls;contextSeen=context;requestedSeen=desired;
    if(budgetBehavior==1)return;
    if(budgetBehavior==3)throw std::runtime_error("native setter exception");
    const auto before=get(P(manager.data())+0x18);
    put<uint32_t>(P(manager.data()),0x18,(before&~1u)|uint32_t(desired));
    if(budgetBehavior==2)put<uint32_t>(P(manager.data()),0x18,before^4u);
}
static void begin(void* context){++beginCalls;contextSeen=context;beginOrder=++eventOrder;}
static void light(void* context){++lightCalls;contextSeen=context;put<uint32_t>(P(state.data()),0x40,1);}
static void registration(void* a,void* b,void* c){++regCalls;regA=a;regB=b;regC=c;}
static void reset(uint32_t flags=0){
    egg::config={false,false,32};
    config={true,true,false};fullStopped=false;budgetStopped=false;missionStarts=0;
    setCalls=fullCalls=beginCalls=lightCalls=regCalls=eventOrder=beginOrder=fullOrder=0;fullBehavior=budgetBehavior=0;
    state.fill(0);manager.fill(0);replacement.fill(0);
    put(image,0x20b2890,P(state.data()));put(image,0x20b30e0,P(manager.data()));
    put(P(manager.data()),0,image+0x17e7f78);put(P(manager.data()),0x18,flags);
    nativeFull=full;originalSetter=setter;originalBegin=begin;originalLight=light;originalRegister=registration;
}
static void configAndLogTests(const std::wstring& folder){
    const auto ini=folder+L"settings.ini",missing=folder+L"absent.ini";
    CHECK(!std::filesystem::exists(missing));auto cfg=loadConfig(missing);CHECK(cfg.full&&cfg.budget&&cfg.logging);
    for(const auto* value:{L"1",L"true",L"TRUE",L"Yes",L"on",L"  true "})CHECK(parseBool(value));
    for(const auto* value:{L"0",L"false",L"FALSE",L"No",L"off",L"  false "})CHECK(!parseBool(value));
    CHECK(parseBool(L""));CHECK(parseBool(L"bad"));CHECK(!parseBool(L"bad",false));
    for(int fullValue=0;fullValue<=1;++fullValue)for(int budgetValue=0;budgetValue<=1;++budgetValue)for(int logValue=0;logValue<=1;++logValue){
        CHECK(WritePrivateProfileStringW(L"Optimization",L"FullLightWeightMode",fullValue?L"1":L"0",ini.c_str()));
        CHECK(WritePrivateProfileStringW(L"Optimization",L"AcceptGeneratorObjects",budgetValue?L"true":L"false",ini.c_str()));
        CHECK(WritePrivateProfileStringW(L"Logging",L"Enabled",logValue?L"yes":L"no",ini.c_str()));
        cfg=loadConfig(ini);CHECK(cfg.full==bool(fullValue)&&cfg.budget==bool(budgetValue)&&cfg.logging==bool(logValue));
    }
    CHECK(WritePrivateProfileStringW(L"Optimization",L"FullLightWeightMode",nullptr,ini.c_str()));
    cfg=loadConfig(ini);CHECK(cfg.full); // A missing key must independently default on.
    const auto logPath=folder+L"EDF6GeneratorBodyBudget.log";
    CHECK(!std::filesystem::exists(logPath));config.logging=false;initializeLogging(folder);message("disabled");CHECK(!std::filesystem::exists(logPath));
    config.logging=true;initializeLogging(folder);CHECK(logFile!=INVALID_HANDLE_VALUE);message("fixture message");
    CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;
    std::ifstream input(std::filesystem::path(logPath),std::ios::binary);const std::string text((std::istreambuf_iterator<char>(input)),{});CHECK(text.find("fixture message")!=std::string::npos);input.close();
    initializeLogging(folder);const auto actual=logBytes;logBytes=LogLimit;message("must not grow");CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;
    CHECK(std::filesystem::file_size(logPath)==actual);
    config.logging=true;initializeLogging(folder+L"nonexistent-subdirectory\\");CHECK(logFile==INVALID_HANDLE_VALUE);
    reset();ensureFull("log_open_failed");CHECK(fullCalls==1&&!fullStopped.load());
}
static int(*ownOriginal)(int)=nullptr;
static __attribute__((noinline)) int ownFunction(int value){volatile int v=value;return v*3+1;}
static int ownHook(int value){return ownOriginal(value)+100;}
int wmain(int argc,wchar_t** argv){
 try{
    CHECK(argc==2);const std::wstring folder=std::wstring(argv[1])+L"\\";
    CHECK(std::filesystem::is_directory(folder));
    image=P(VirtualAlloc(nullptr,0x20b4000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));CHECK(image);
    configAndLogTests(folder);
    int context=11,controller=22,weak=33;
    for(bool f:{false,true})for(bool b:{false,true}){
        reset(0xa4);config.full=f;config.budget=b;
        std::array<HookSpec,7> hooks{};CHECK(selectHooks(hooks)==size_t(2*unsigned(f)+2*unsigned(b)));
        hookBegin(&context);hookRegister(manager.data(),&controller,&weak);
        CHECK(beginCalls==1&&regCalls==1&&regA==manager.data()&&regB==&controller&&regC==&weak);
        CHECK(get(P(state.data())+0x40)==(f?2u:0u));CHECK(get(P(manager.data())+0x18)==(b?0xa5u:0xa4u));
        if(f)CHECK(beginOrder<fullOrder);
        hookLight(&context);CHECK(lightCalls==1&&get(P(state.data())+0x40)==(f?2u:1u));
        for(bool requested:{false,true}){hookSetter(&context,requested);CHECK(contextSeen==&context&&requestedSeen==(b?true:requested));}
    }
    for(uint32_t flags=0;flags<256;++flags){reset(flags);ensureBudget(P(manager.data()));CHECK(get(P(manager.data())+0x18)==(flags|1u));CHECK(setCalls==unsigned(!(flags&1u)));ensureBudget(P(manager.data()));CHECK(setCalls==unsigned(!(flags&1u)));}
    reset();hookBegin(&context);hookBegin(&context);CHECK(beginCalls==2&&fullCalls==1&&missionStarts==2);
    put<uint32_t>(P(state.data()),0x40,0);hookBegin(&context);CHECK(fullCalls==2&&get(P(state.data())+0x40)==2); // Restart re-enables.
    reset();put(image,0x20b2890,P(0));hookBegin(&context);CHECK(beginCalls==1&&fullCalls==0&&!fullStopped.load());
    put(image,0x20b2890,P(state.data()));hookBegin(&context);CHECK(fullCalls==1); // Late state initialization retries.
    for(int behavior:{1,2}){reset();fullBehavior=behavior;ensureFull("fixture");CHECK(fullStopped.load());ensureBudget(P(manager.data()));CHECK(setCalls==1&&!budgetStopped.load());}
    reset();put<uint32_t>(P(state.data()),0x40,3);ensureFull("invalid");CHECK(fullStopped.load()&&fullCalls==0);
    reset();nativeFull=nullptr;ensureFull("missing");CHECK(fullStopped.load());
    reset();ensureBudget(P(replacement.data()));CHECK(setCalls==0&&!budgetStopped.load());
    reset();put(image,0x20b30e0,P(0));hookRegister(manager.data(),&controller,&weak);CHECK(budgetStopped.load()&&regCalls==1);
    reset();put(P(manager.data()),0,image+0x17e7f80);ensureBudget(P(manager.data()));CHECK(budgetStopped.load()&&setCalls==0);
    for(int behavior:{1,2}){reset();budgetBehavior=behavior;hookRegister(manager.data(),&controller,&weak);CHECK(budgetStopped.load()&&regCalls==1);ensureFull("independent");CHECK(fullCalls==1&&!fullStopped.load());budgetBehavior=0;hookSetter(&context,false);CHECK(!requestedSeen);}
    reset();originalSetter=nullptr;ensureBudget(P(manager.data()));CHECK(budgetStopped.load());
    reset();budgetBehavior=3;bool threw=false;try{hookSetter(&context,false);}catch(const std::runtime_error&){threw=true;}CHECK(threw);
    reset();fullBehavior=3;threw=false;try{hookBegin(&context);}catch(const std::runtime_error&){threw=true;}CHECK(threw&&beginCalls==1);
    for(const auto& signature:target::signatures)std::memcpy(reinterpret_cast<void*>(image+signature.rva),signature.bytes.data(),32);
    for(const auto& signature:gateSignatures)std::memcpy(reinterpret_cast<void*>(image+signature.rva),signature.bytes.data(),32);
    CHECK(compatibleBytes());for(const auto& signature:target::signatures){auto at=reinterpret_cast<uint8_t*>(image+signature.rva);*at^=1;CHECK(!compatibleBytes());*at^=1;}CHECK(compatibleBytes());
    CHECK(MH_Initialize()==MH_OK);int(*volatile call)(int)=ownFunction;CHECK(call(2)==7);
    CHECK(MH_CreateHook(reinterpret_cast<void*>(ownFunction),reinterpret_cast<void*>(ownHook),reinterpret_cast<void**>(&ownOriginal))==MH_OK);
    CHECK(MH_EnableHook(reinterpret_cast<void*>(ownFunction))==MH_OK);CHECK(call(2)==107);
    CHECK(MH_DisableHook(reinterpret_cast<void*>(ownFunction))==MH_OK);CHECK(call(2)==7);CHECK(MH_RemoveHook(reinterpret_cast<void*>(ownFunction))==MH_OK);
    reset();std::array<HookSpec,7> hooks{};const auto count=selectHooks(hooks);auto memory=static_cast<uint8_t*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));CHECK(memory);
    for(size_t i=0;i<count;++i){const target::Signature* signature=nullptr;for(const auto& item:target::signatures)if(item.rva==hooks[i].rva)signature=&item;CHECK(signature);std::memcpy(memory,signature->bytes.data(),32);void* trampoline=nullptr;CHECK(MH_CreateHook(memory,hooks[i].replacement,&trampoline)==MH_OK&&trampoline);CHECK(MH_RemoveHook(memory)==MH_OK);}
    CHECK(MH_Uninitialize()==MH_OK);VirtualFree(memory,0,MEM_RELEASE);VirtualFree(reinterpret_cast<void*>(image),0,MEM_RELEASE);
    std::cout<<"PASS "<<checks<<" checks: independent feature/config/log combinations, missing INI and key defaults, original-thread ordering, restart, native forwarding, flag preservation, fail-stop isolation, actual file-log switch/cap, signature rejection and four MinHook trampolines; no game loaded.\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
}
