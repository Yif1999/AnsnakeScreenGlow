param([string]$Output='build\release',[switch]$Tests)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$destination=[IO.Path]::GetFullPath((Join-Path $root $Output))
New-Item -ItemType Directory -Force $destination | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if(-not (Test-Path $vswhere)){throw 'Install Visual Studio with Desktop development with C++ and Windows SDK.'}
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vs){throw 'MSVC x64 toolchain not found.'}
$environmentScript=Join-Path $destination 'compiler-environment.cmd'
@('@echo off',('call "'+$vs+'\VC\Auxiliary\Build\vcvars64.bat" >nul'),'if errorlevel 1 exit /b 1','set') | Set-Content -Encoding ascii $environmentScript
$environment=& cmd.exe /d /c $environmentScript
if($LASTEXITCODE -ne 0){throw 'Could not initialize MSVC environment.'}
foreach($line in $environment){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process')}}
$winrt=Join-Path $env:WindowsSdkDir ('Include\'+$env:WindowsSDKVersion.TrimEnd('\')+'\cppwinrt')
if(-not (Test-Path $winrt)){throw 'Windows SDK C++/WinRT headers not found.'}
$common=@('/nologo','/std:c++20','/EHsc','/O2','/MT','/utf-8','/DUNICODE','/D_UNICODE',('/I'+$winrt))
$libs=@('d3d11.lib','dxgi.lib','d3dcompiler.lib','windowsapp.lib','psapi.lib','user32.lib','gdi32.lib','comctl32.lib','shell32.lib','setupapi.lib','advapi32.lib','uuid.lib')
function Compile([string]$source,[string]$name,[switch]$Gui,[string[]]$Defines=@()){
 $arguments=$common+$Defines+@((Join-Path $root ('src\'+$source)),('/Fo'+(Join-Path $destination ($name+'.obj'))),('/Fe'+(Join-Path $destination ($name+'.exe'))),'/link')
 if($Gui){$arguments+='/SUBSYSTEM:WINDOWS'}
 & cl.exe @arguments @libs
 if($LASTEXITCODE -ne 0){throw "Compilation failed: $name"}
}
Compile 'ScreenGlow.cpp' 'ScreenGlow' -Gui
Compile 'ScreenGlow.cpp' 'Calibrate' -Gui -Defines @('/DCALIBRATION_ONLY')
if($Tests){foreach($test in @('CornerLayoutTests','LedColorTests','EdgeGaussianTests','CalibrationTests','CaptureLifecycleTests')){Compile ($test+'.cpp') $test};Compile 'PerfPattern.cpp' 'PerfPattern' -Gui}
Write-Output "Build complete: $destination"
