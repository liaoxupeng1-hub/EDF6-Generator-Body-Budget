param([string]$Toolchain='D:\Program Files\msys64\ucrt64\bin')
$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
Set-Location -LiteralPath $taskRoot
$env:PATH=$Toolchain+';'+$env:PATH
foreach($taskDir in @('build','release','evidence')){New-Item -ItemType Directory -Path (Join-Path $taskRoot $taskDir) -Force | Out-Null}
$taskObjects=@()
foreach($taskSource in @('buffer.c','hook.c','trampoline.c','hde\hde64.c')){
 $taskObj=Join-Path ($taskRoot+'\build') ([IO.Path]::GetFileNameWithoutExtension($taskSource)+'.o')
 & ($Toolchain+'\gcc.exe') -c -O2 -Wall -Wextra -Werror -DNDEBUG -D_WIN32_WINNT=0x0601 ($taskRoot+'\vendor\minhook\src\'+$taskSource) -o $taskObj
 if($LASTEXITCODE -ne 0){throw 'MinHook compilation failed'}
 $taskObjects+=$taskObj
}
& ($Toolchain+'\windres.exe') -i 'src\version.rc' -O coff -o 'build\version.o'
if($LASTEXITCODE -ne 0){throw 'Version resource failed'}
$taskFlags=@('-std=c++20','-O2','-Wall','-Wextra','-Werror','-D_WIN32_WINNT=0x0601','-static','-static-libgcc','-static-libstdc++','-I',($taskRoot+'\vendor\minhook\include'))
& ($Toolchain+'\g++.exe') @taskFlags -shared 'src\plugin.cpp' @taskObjects 'build\version.o' -lbcrypt '-Wl,--no-insert-timestamp,--dynamicbase,--nxcompat' -o 'release\EDF6GeneratorBodyBudget.dll'
if($LASTEXITCODE -ne 0){throw 'DLL compilation failed'}
foreach($taskSuite in @('policy','egg','integration')){
 & ($Toolchain+'\g++.exe') @taskFlags -Wno-unused-function -municode ('tests\'+$taskSuite+'_tests.cpp') @taskObjects -lbcrypt -o ('build\'+$taskSuite+'_tests.exe')
 if($LASTEXITCODE -ne 0){throw ('Test compilation failed: '+$taskSuite)}
 $taskUnit=Join-Path $taskRoot ('build\'+$taskSuite+'-'+[guid]::NewGuid().ToString('N'))
 New-Item -ItemType Directory -Path $taskUnit -Force | Out-Null
 & ($taskRoot+'\build\'+$taskSuite+'_tests.exe') ($taskUnit+'\') | Tee-Object -FilePath ($taskRoot+'\evidence\'+$taskSuite+'-validation.txt')
 if($LASTEXITCODE -ne 0){throw ('Tests failed: '+$taskSuite)}
}
& ($Toolchain+'\g++.exe') @taskFlags -municode 'tests\load_smoke.cpp' -o 'build\load_smoke.exe'
if($LASTEXITCODE -ne 0){throw 'Loader fixture compilation failed'}
foreach($taskMode in @('on','off')){
 $taskFixture=Join-Path $taskRoot ('build\loader-'+$taskMode+'-'+[guid]::NewGuid().ToString('N'))
 New-Item -ItemType Directory -Path $taskFixture -Force | Out-Null
 Copy-Item -LiteralPath ($taskRoot+'\release\EDF6GeneratorBodyBudget.dll') -Destination ($taskFixture+'\EDF6GeneratorBodyBudget.dll')
 $taskLogging=if($taskMode -eq 'on'){'1'}else{'0'}
 @('[Optimization]','FullLightWeightMode=0','AcceptGeneratorObjects=0','[EggShells]','Enabled=0','MaxShellGroups=32','[Logging]',('Enabled='+$taskLogging)) | Set-Content -LiteralPath ($taskFixture+'\EDF6GeneratorBodyBudget.ini') -Encoding ascii
 & ($taskRoot+'\build\load_smoke.exe') ($taskFixture+'\EDF6GeneratorBodyBudget.dll') $taskMode | Tee-Object -FilePath ($taskRoot+'\evidence\loader-'+$taskMode+'.txt')
 if($LASTEXITCODE -ne 0){throw 'Real loader fixture failed'}
}
Get-FileHash -LiteralPath ($taskRoot+'\release\EDF6GeneratorBodyBudget.dll') -Algorithm SHA256
