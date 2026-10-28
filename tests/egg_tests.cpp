#define EGG_UNIT_TEST
#include "../src/plugin.cpp"
#include <thread>
#include <vector>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <iostream>
using namespace egg;
static unsigned checks=0;
static void check(bool value,const char* label){++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}
struct Object{P vt=0;};
struct Result{std::array<unsigned char,0x28> pad{};Object* object=nullptr;};
static std::atomic<unsigned> factoryCalls{0},destroyCalls{0};
static bool returnNull=false,throwCreation=false,badClass=false;
static bool destroyDuringFactory=false,nullAfterConstruction=false;
static P observedManager=0,observedTransform=0,observedInit=0;static const wchar_t* observedPath=nullptr;
static thread_local Object object;
static thread_local Result result;
static void* mockConstruct(void* p,void*){static_cast<Object*>(p)->vt=image+(badClass?ShellVtable+8:ShellVtable);return p;}
static void* mockFactory(void* m,void* t,const wchar_t* p,void* i){
    ++factoryCalls;observedManager=P(m);observedTransform=P(t);observedPath=p;observedInit=P(i);
    if(throwCreation)throw std::runtime_error("mock creation failed");
    if(returnNull)return nullptr;
    hookConstruct(&object,i);result.object=&object;
    if(destroyDuringFactory)hookDestroy(&object,0x81234567u);
    return nullAfterConstruction?nullptr:&result;
}
static void* mockDestroy(void* o,uint32_t f){check(f==0x81234567u,"destructor flags preserved");++destroyCalls;return o;}
static void reset(){entries={};counters={};nextGeneration=0;stopped=false;config={true,false,32};image=0x180000000ull;originalFactory=mockFactory;originalDestroy=mockDestroy;originalConstruct=mockConstruct;factoryCalls=0;destroyCalls=0;returnNull=throwCreation=badClass=false;destroyDuringFactory=nullAfterConstruction=false;currentReservation=nullptr;}
static wchar_t normal[96]=L"app:/object/GiantAnt01Egg_Break.sgo",red[96]=L"APP:/OBJECT/GIANTANT02BEGG_BREAK.SGO",gold[96]=L"app:/object/GoldAntEgg_Break.sgo";
static wchar_t squid[96]=L"app:/object/e619_squid_small_egg_break.sgo";
static P init;
static void* request(const wchar_t* path=normal,P caller=0){init=image+InitVtable;return routeFactory(caller?caller:image+ShellReturnRva,reinterpret_cast<void*>(0xabcdef),reinterpret_cast<void*>(0x123450),path,&init);}
static void functional(){
    reset();config.maxGroups=0;
    check(request()==nullptr&&factoryCalls==0,"zero cap blocks before model/physics factory");
    check(snapshot().blocked==1&&snapshot().live==0&&snapshot().pending==0,"zero cap counters");
    config.enabled=false;check(request()!=nullptr&&factoryCalls==1,"disabled forwards original");
    reset();config.maxGroups=0;
    check(request(normal,image+0x414431)!=nullptr,"ant creation return address never filtered");
    check(request(normal,image+ShellReturnRva+1)!=nullptr,"unrelated caller never filtered");
    wchar_t other[96]=L"app:/object/e663_spider_egg_sac.sgo";check(request(other)!=nullptr,"other resource not filtered");
    init=image+InitVtable+8;check(routeFactory(image+ShellReturnRva,nullptr,nullptr,normal,&init)!=nullptr,"unexpected initializer preserved");
    check(!isShellRequest(image+ShellReturnRva,reinterpret_cast<wchar_t*>(1),&init),"invalid path safe");
    init=image+InitVtable;check(!isShellRequest(image+ShellReturnRva,normal,reinterpret_cast<void*>(1)),"invalid initializer safe");
    wchar_t unterminated[96];for(auto& c:unterminated)c=L'x';check(!isShellRequest(image+ShellReturnRva,unterminated,&init),"unterminated path rejected");
    check(snapshot().requests==0,"unrelated requests not counted");
    reset();config.maxGroups=1;
    for(auto path:{normal,red,gold,squid}){
        void* r=request(path);check(r==&result,"original factory result preserved");
        check(observedManager==0xabcdef&&observedTransform==0x123450&&observedPath==path&&observedInit==P(&init),"all original arguments preserved");
        check(snapshot().live==1&&snapshot().pending==0,"one shell group counted once");
        auto before=factoryCalls.load();check(request()==nullptr&&factoryCalls==before,"full budget does not call original factory");
        check(hookDestroy(&object,0x81234567u)==&object,"original destructor return preserved");
        check(snapshot().live==0,"capacity restored after native destructor");
    }
    check(snapshot().created==4&&snapshot().destroyed==4&&snapshot().peak==1,"three variants lifecycle");
    check(hookDestroy(reinterpret_cast<void*>(0x12345678),0x81234567u)==reinterpret_cast<void*>(0x12345678),"untracked destruction forwarded without dereference");
    check(snapshot().destroyed==4,"untracked destruction not counted");
    reset();config.maxGroups=1;returnNull=true;check(request()==nullptr,"failed native factory returned unchanged");
    check(snapshot().pending==0&&snapshot().live==0&&snapshot().failed==1,"failed creation releases reservation");
    returnNull=false;throwCreation=true;bool threw=false;try{request();}catch(const std::runtime_error&){threw=true;}
    check(threw&&snapshot().pending==0&&currentReservation==nullptr,"exceptions propagate and restore reservation scope");throwCreation=false;
    badClass=true;check(request()!=nullptr&&stopped,"unknown factory result stops enforcement without changing result");
    auto calls=factoryCalls.load();config.maxGroups=0;check(request()!=nullptr&&factoryCalls==calls+1,"fail stop forwards later creation");
    reset();config.maxGroups=1;destroyDuringFactory=true;
    check(request()!=nullptr&&!stopped&&snapshot().live==0&&snapshot().created==1&&snapshot().destroyed==1,"object destroyed before factory return leaves no stale count");
    nullAfterConstruction=true;check(request()==nullptr&&!stopped&&snapshot().live==0&&snapshot().pending==0&&snapshot().failed==1,"native failed creation after construction and cleanup does not leak capacity");
    reset();config.maxGroups=3;
    Object a{image+ShellVtable};Reservation r;check(reserve(r),"reserve slot");registerCreated(r,P(&a));auto old=retiring(P(&a));
    check(snapshot().live==1,"destructing object remains counted until destructor completes");
    Reservation r2;check(reserve(r2),"reserve for address reuse");registerCreated(r2,P(&a));auto newer=retiring(P(&a));
    retired(old);check(snapshot().live==1,"old generation cannot remove reused address");retired(old);check(snapshot().live==1,"duplicate cleanup cannot underflow");
    retired(newer);check(snapshot().live==0,"new generation cleaned independently");
    reset();config.maxGroups=32;std::vector<Token> tokens;
    std::array<Object,32> bodies{};for(auto& o:bodies){Reservation slot;check(reserve(slot),"fill cap");registerCreated(slot,P(&o));}
    for(unsigned n=0;n<800;n++){Reservation blocked;check(!reserve(blocked),"burst above cap blocked before factory");}
    check(snapshot().live==32&&snapshot().blocked==800&&snapshot().peak==32,"large hatch burst stays bounded");
    for(auto& o:bodies)retired(retiring(P(&o)));
    for(int mission=0;mission<50;++mission){Reservation slot;check(reserve(slot),"mission restart can reuse native-cleared capacity");registerCreated(slot,P(&a));retired(retiring(P(&a)));}
    check(snapshot().live==0&&snapshot().pending==0,"repeated native cleanup leaves no stale count");
    std::puts("PASS: creation boundary, original arguments/returns, three variants, zero cap, failure, identity, generations and repeated lifecycle.");
}
static void concurrency(){
    reset();config.maxGroups=3;std::atomic<bool> exceeded{false};
    std::vector<std::thread> workers;
    for(int t=0;t<8;++t)workers.emplace_back([&](){Object local;for(int n=0;n<2000;++n){Reservation r;if(reserve(r)){registerCreated(r,P(&local));if(snapshot().live>3)exceeded=true;std::this_thread::yield();retired(retiring(P(&local)));}}});
    for(auto& t:workers)t.join();
    check(!exceeded&&snapshot().live==0&&snapshot().pending==0,"concurrent reservations and native cleanup");
    check(snapshot().requests==16000&&snapshot().created==snapshot().destroyed,"concurrent bookkeeping complete");
    std::puts("PASS: 8-thread synthetic reservation/lifecycle test; not a game threading test.");
}
static void configAndLogging(const std::filesystem::path& root){
    reset();auto missing=loadConfig((root/L"missing.ini").wstring());check(missing.enabled&&missing.logging&&missing.maxGroups==32,"missing INI defaults");
    check(parseLimit(L"0")==0&&parseLimit(L"1024")==1024&&parseLimit(L"1025")==32&&parseLimit(L"-1")==32&&parseLimit(L"999999999999999999")==32&&parseLimit(L"bad")==32,"limit validation");
    auto path=root/L"test.ini";std::ofstream(path)<<"[EggShells]\nEnabled=0\nMaxShellGroups=0\n[Logging]\nEnabled=0\n";
    auto c=loadConfig(path.wstring());check(!c.enabled&&!c.logging&&c.maxGroups==0,"INI zero values honored");
    std::ofstream(root/L"partial.ini")<<"[EggShells]\nMaxShellGroups=7\n";c=loadConfig((root/L"partial.ini").wstring());check(c.enabled&&c.logging&&c.maxGroups==7,"missing keys default independently");
    config.logging=false;initializeLogging(root.wstring()+L"\\");message("not written");check(!std::filesystem::exists(root/L"EDF6GeneratorBodyBudget.log"),"logging off creates nothing");
    config.logging=true;initializeLogging(root.wstring()+L"\\");message("test marker");check(std::filesystem::file_size(root/L"EDF6GeneratorBodyBudget.log")>0,"logging on writes");
    auto size=std::filesystem::file_size(root/L"EDF6GeneratorBodyBudget.log");logBytes=LogLimit;message("over limit");check(std::filesystem::file_size(root/L"EDF6GeneratorBodyBudget.log")==size,"log bounded");
    CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;config.logging=false;initializeLogging(root.wstring()+L"\\");message("off again");check(std::filesystem::file_size(root/L"EDF6GeneratorBodyBudget.log")==size,"old log retained when disabled");
    config.logging=true;initializeLogging((root/L"nonexistent").wstring()+L"\\");check(logFile==INVALID_HANDLE_VALUE,"logging open failure tolerated");
    std::puts("PASS: actual Windows INI defaults, switches and bounded logging.");
}
static void trampolines(){
    reset();auto memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x2000000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));check(memory!=nullptr,"allocate synthetic module");
    image=P(memory);for(const auto& s:eggTarget::signatures)std::memcpy(memory+s.rva,s.bytes.data(),s.bytes.size());
    check(compatibleBytes(),"all pinned windows accepted");memory[eggTarget::signatures.back().rva]^=1;check(!compatibleBytes(),"boundary byte modification rejected");memory[eggTarget::signatures.back().rva]^=1;
    check(MH_Initialize()==MH_OK,"initialize real MinHook");
    for(auto rva:{FactoryRva,DestroyRva,ConstructRva}){
        void* original=nullptr;void* replacement=rva==FactoryRva?reinterpret_cast<void*>(hookFactory):(rva==DestroyRva?reinterpret_cast<void*>(hookDestroy):reinterpret_cast<void*>(hookConstruct));
        check(MH_CreateHook(memory+rva,replacement,&original)==MH_OK&&original,"exact entry prologue relocates");
        check(MH_EnableHook(memory+rva)==MH_OK,"enable fixture detour");check(MH_DisableHook(memory+rva)==MH_OK,"disable fixture detour");check(MH_RemoveHook(memory+rva)==MH_OK,"remove fixture detour");
    }
    check(MH_Uninitialize()==MH_OK&&compatibleBytes(),"fixtures restore original bytes");VirtualFree(memory,0,MEM_RELEASE);
    std::puts("PASS: real MinHook relocation/enable/disable on copies of exact game entry bytes; no game code executed.");
}
static void* syntheticNativeFactory(void* m,void* t,const wchar_t* p,void* i){
    ++factoryCalls;check(P(m)==0xabcdef&&P(t)==0x123450&&p==normal&&i==&init,"machine detour preserves native ABI arguments");
    auto construct=reinterpret_cast<Construct>(image+ConstructRva);construct(&object,i);result.object=&object;return &result;
}
static void integratedDetours(){
    reset();config.maxGroups=1;
    auto memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x2000000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));check(memory!=nullptr,"allocate integration module");image=P(memory);init=image+InitVtable;
    auto stub=[&](uint32_t rva,void* destination){unsigned char* p=memory+rva;std::memset(p,0x90,64);p+=16;*p++=0x48;*p++=0xb8;P a=P(destination);std::memcpy(p,&a,8);p+=8;*p++=0xff;*p=0xe0;};
    stub(FactoryRva,reinterpret_cast<void*>(syntheticNativeFactory));stub(ConstructRva,reinterpret_cast<void*>(mockConstruct));stub(DestroyRva,reinterpret_cast<void*>(mockDestroy));
    auto caller=[&](uint32_t returnRva){
        const uint32_t start=returnRva-0x70;unsigned char* p=memory+start;
        *p++=0x48;*p++=0x83;*p++=0xec;*p++=0x28;
        auto imm=[&](unsigned char rex,unsigned char op,P n){*p++=rex;*p++=op;std::memcpy(p,&n,8);p+=8;};
        imm(0x48,0xb9,0xabcdef);imm(0x48,0xba,0x123450);imm(0x49,0xb8,P(normal));imm(0x49,0xb9,P(&init));
        while(P(p)<image+returnRva-5)*p++=0x90;
        *p++=0xe8;int32_t rel=int32_t((image+FactoryRva)-(image+returnRva));std::memcpy(p,&rel,4);p+=4;
        *p++=0x48;*p++=0x83;*p++=0xc4;*p++=0x28;*p=0xc3;
        return reinterpret_cast<void*(*)()>(memory+start);
    };
    auto targetCaller=caller(ShellReturnRva),otherCaller=caller(ShellReturnRva+0x100);
    FlushInstructionCache(GetCurrentProcess(),memory,0x2000000);
    gb::config={false,false,false};
    check(gb::installHooks(),"install all three actual hooks on synthetic functions");
    check(targetCaller()==&result&&snapshot().live==1&&snapshot().created==1,"actual return-address gate and constructor registration");
    auto before=factoryCalls.load();check(targetCaller()==nullptr&&factoryCalls==before,"actual machine call skips factory at full cap");
    auto destroy=reinterpret_cast<Destroy>(image+DestroyRva);check(destroy(&object,0x81234567u)==&object&&snapshot().live==0,"actual destructor detour releases capacity");
    check(targetCaller()==&result&&snapshot().live==1,"actual call can create after native cleanup");destroy(&object,0x81234567u);
    config.maxGroups=0;before=factoryCalls.load();check(otherCaller()==&result&&factoryCalls==before+1&&snapshot().live==0,"different machine caller remains untouched at zero cap");
    config.enabled=false;check(targetCaller()==&result&&snapshot().live==0,"actual disabled pass through does not register shells");
    check(MH_DisableHook(MH_ALL_HOOKS)==MH_OK&&MH_Uninitialize()==MH_OK,"integration hooks removed");VirtualFree(memory,0,MEM_RELEASE);
    std::puts("PASS: executed synthetic Windows x64 callsite through all three real detours; caller gate, original arguments, pre-creation blocking and native cleanup; no game code executed.");
}
int wmain(int argc,wchar_t** argv){if(argc!=2)return 2;functional();concurrency();configAndLogging(argv[1]);trampolines();integratedDetours();std::printf("PASS %u assertions\n",checks);return 0;}
