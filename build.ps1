param([string]$Toolchain='D:\Program Files\msys64\ucrt64\bin')
$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
$taskVendor=Join-Path $taskRoot 'vendor\minhook'
Set-Location -LiteralPath $taskRoot
foreach($taskDirectory in @('build','release','evidence')){New-Item -ItemType Directory -Path (Join-Path $taskRoot $taskDirectory) -Force | Out-Null}
if(-not(Test-Path -LiteralPath ($Toolchain+'\g++.exe'))){throw 'Set -Toolchain to an existing MinGW-w64 UCRT x64 bin directory.'}
$env:PATH=$Toolchain+';'+$env:PATH
$taskObjects=@()
foreach($taskFile in @('buffer.c','hook.c','trampoline.c','hde\hde64.c')){
 $taskObj=Join-Path ($taskRoot+'\build') ([IO.Path]::GetFileNameWithoutExtension($taskFile)+'.o')
 & ($Toolchain+'\gcc.exe') -c -O2 -Wall -Wextra -DNDEBUG -D_WIN32_WINNT=0x0601 ($taskVendor+'\src\'+$taskFile) -o $taskObj
 if($LASTEXITCODE -ne 0){throw ('C compilation failed: '+$taskFile)}
 $taskObjects+=$taskObj
}
& ($Toolchain+'\windres.exe') -i 'src\version.rc' -O coff -o 'build\version.o'
if($LASTEXITCODE -ne 0){throw 'Version resource compilation failed'}
$taskCommon=@('-std=c++20','-O2','-Wall','-Wextra','-D_WIN32_WINNT=0x0601','-static','-static-libgcc','-static-libstdc++','-I',($taskVendor+'\include'))
& ($Toolchain+'\g++.exe') @taskCommon '-shared' 'src\plugin.cpp' @taskObjects 'build\version.o' '-lbcrypt' '-Wl,--no-insert-timestamp,--dynamicbase,--nxcompat' '-o' 'release\EDF6GeneratorBodyBudget.dll'
if($LASTEXITCODE -ne 0){throw 'DLL compilation failed'}
& ($Toolchain+'\g++.exe') @taskCommon '-municode' 'tests\policy_tests.cpp' @taskObjects '-lbcrypt' '-o' 'build\policy_tests.exe'
if($LASTEXITCODE -ne 0){throw 'Policy test build failed'}
$taskUnit=Join-Path $taskRoot ('build\unit-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskUnit -Force | Out-Null
& ($taskRoot+'\build\policy_tests.exe') $taskUnit | Tee-Object -FilePath ($taskRoot+'\evidence\policy-validation.txt')
if($LASTEXITCODE -ne 0){throw 'Policy tests failed'}
& ($Toolchain+'\g++.exe') @taskCommon '-municode' 'tests\load_smoke.cpp' '-o' 'build\load_smoke.exe'
if($LASTEXITCODE -ne 0){throw 'Loader host build failed'}
foreach($taskLog in @('on','off')){
 $taskFixture=Join-Path $taskRoot ('build\loader-'+$taskLog+'-'+[guid]::NewGuid().ToString('N'))
 New-Item -ItemType Directory -Path $taskFixture -Force | Out-Null
 Copy-Item -LiteralPath ($taskRoot+'\release\EDF6GeneratorBodyBudget.dll') -Destination ($taskFixture+'\EDF6GeneratorBodyBudget.dll')
 $taskLogValue=if($taskLog -eq 'on'){'1'}else{'0'}
 @('[Optimization]','FullLightWeightMode=0','AcceptGeneratorObjects=0','[Logging]',('Enabled='+$taskLogValue)) | Set-Content -LiteralPath ($taskFixture+'\EDF6GeneratorBodyBudget.ini') -Encoding ascii
 & ($taskRoot+'\build\load_smoke.exe') ($taskFixture+'\EDF6GeneratorBodyBudget.dll') $taskLog | Tee-Object -FilePath ($taskRoot+'\evidence\loader-'+$taskLog+'.txt')
 if($LASTEXITCODE -ne 0){throw ('Loader smoke failed: '+$taskLog)}
}
Get-FileHash -LiteralPath ($taskRoot+'\release\EDF6GeneratorBodyBudget.dll') -Algorithm SHA256
