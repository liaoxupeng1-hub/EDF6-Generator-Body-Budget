# Rebuilding 0.2.0

Requires an existing MinGW-w64 UCRT x64 toolchain with gcc, g++, and windres, plus PowerShell. This script does not install tools.

Run build.ps1 with -Toolchain pointing at its bin directory. It creates build/release/evidence next to the sources, builds the DLL and version resource, runs synthetic policy/config/log tests and two isolated loader hosts. No game is launched or injected by the tests. The shipped default INI is in the source root; copy it beside the rebuilt DLL for deployment.

The five SHA256-pinned native entry byte signatures and four gate windows are included; they are for the documented game version only. Do not silently relax them. MinHook source and its license are in vendor/minhook.

Compiler used for distributed DLL: MinGW-w64 UCRT x64 g++ from the existing local toolchain; exact compiler output retained in the build evidence.
