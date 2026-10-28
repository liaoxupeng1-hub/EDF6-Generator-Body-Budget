# Rebuilding 0.3.0

Requires an existing MinGW-w64 UCRT x64 toolchain with gcc, g++ and windres, and PowerShell 7. No tools are downloaded or installed by the build.

Run:

```powershell
./build.ps1 -Toolchain 'D:\Program Files\msys64\ucrt64\bin'
```

Use your own toolchain bin directory if different. The script creates build/, release/ and evidence/, compiles the DLL, runs the original policy tests, egg-shell tests, integration tests and two disabled-feature loader hosts. No game process is launched or injected. Expected offline totals for this snapshot are 1336 + 985 + 110 = 2431 assertions/checks and two loader hosts.

The rebuilt DLL is release/EDF6GeneratorBodyBudget.dll. Copy the root EDF6GeneratorBodyBudget.ini beside it for deployment with EDFModLoader. The INI must be updated when adding the new EggShells section; settings are read once per game launch.

Source files match the delivered 0.3.0 source archive byte-for-byte. Local publication documentation is maintained separately. Target signatures remain tied to one EDF.dll build; do not remove or relax them to claim support for a different build. MinHook source and license are preserved under vendor/minhook.
