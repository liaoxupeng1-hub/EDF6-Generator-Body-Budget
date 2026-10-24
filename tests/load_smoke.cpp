#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
struct Version{unsigned short build,patch,minor,major;};
struct Info{unsigned long infoVersion;const char* name;Version version;};
int wmain(int argc,wchar_t** argv){
 if(argc!=3||GetModuleHandleW(L"EDF.dll"))return 2;
 HMODULE module=LoadLibraryW(argv[1]);if(!module)return 3;
 auto symbol=GetProcAddress(module,"EML6_Load");if(!symbol||GetProcAddress(module,"EML5_Load"))return 4;
 using Load=bool(*)(Info*);Load load=nullptr;std::memcpy(&load,&symbol,sizeof(load));
 if(load(nullptr))return 5;
 Info info{};
 if(!load(&info)||!info.name||info.infoVersion!=1||info.version.major!=0||info.version.minor!=2||info.version.patch!=0)return 6;
 if(!load(&info))return 7;
 const auto folder=std::filesystem::path(argv[1]).parent_path(),log=folder/L"EDF6GeneratorBodyBudget.log";
 const bool expected=std::wstring(argv[2])==L"on";
 for(int i=0;i<50;++i){
   if(std::filesystem::exists(log)){
     if(!expected)return 8;
     std::ifstream in(log,std::ios::binary);const std::string text((std::istreambuf_iterator<char>(in)),{});
     if(text.find("DISABLED no_hooks=1")!=std::string::npos){std::puts("PASS: real EML6_Load 0.2.0; feature switches disabled; log on; no game loaded.");return 0;}
   }Sleep(20);
 }
 if(expected)return 9;
 std::puts("PASS: real EML6_Load 0.2.0; feature switches disabled; log off; no log created; no game loaded.");return 0;
}
