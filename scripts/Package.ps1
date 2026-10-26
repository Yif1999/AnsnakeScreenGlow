param([string]$Version='1.0.2',[string]$BuildDirectory='build\release')
$ErrorActionPreference='Stop'
if($Version -notmatch '^\d+\.\d+\.\d+(-[a-zA-Z0-9.-]+)?$'){throw 'Invalid version'}
$root=Split-Path $PSScriptRoot -Parent
$package=Join-Path $root "dist\AnsnakeScreenGlow-$Version-win-x64"
if(Test-Path $package){throw 'Package folder exists; use a new version or move the previous package.'}
New-Item -ItemType Directory -Force $package | Out-Null
foreach($name in @('ScreenGlow.exe','Calibrate.exe')){Copy-Item (Join-Path $root "$BuildDirectory\$name") $package}
foreach($name in @('README.md','CHANGELOG.md')){Copy-Item (Join-Path $root $name) $package}
Copy-Item (Join-Path $root 'docs') (Join-Path $package 'docs') -Recurse
@(foreach($file in Get-ChildItem $package -File -Recurse){$hash=Get-FileHash $file.FullName -Algorithm SHA256;"$($hash.Hash.ToLower())  $($file.FullName.Substring($package.Length+1).Replace('\','/'))"}) | Set-Content (Join-Path $package 'SHA256SUMS.txt')
$zip=$package+'.zip';Compress-Archive -Path (Join-Path $package '*') -DestinationPath $zip
Get-FileHash $zip -Algorithm SHA256



