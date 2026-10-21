# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
param([Parameter(Mandatory=$true)][string]$Msi,
      [Parameter(Mandatory=$true)][string]$Zip,
      [Parameter(Mandatory=$true)][string]$Scratch,
      [Parameter(Mandatory=$true)][string]$Version,
      [switch]$KeepInstalled)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
New-Item -ItemType Directory -Path $Scratch | Out-Null
foreach($name in @('TMP','TEMP','TMPDIR')) { [Environment]::SetEnvironmentVariable($name,$Scratch,'Process') }
$Msi=(Resolve-Path $Msi).Path; $Zip=(Resolve-Path $Zip).Path
foreach($package in @($Msi,$Zip)) {
    $sidecar=[IO.File]::ReadAllText("$package.sha256").Trim().Split(@(' ','*'),[StringSplitOptions]::RemoveEmptyEntries)
    if($sidecar.Count -ne 2 -or $sidecar[0] -ine (Get-FileHash $package).Hash -or $sidecar[1] -cne [IO.Path]::GetFileName($package)) { throw 'Package checksum mismatch' }
}
Expand-Archive -LiteralPath $Zip -DestinationPath "$Scratch/zip"
$payload=@(Get-ChildItem "$Scratch/zip" -Recurse -File -Filter ck-sysmon.exe)
if($payload.Count -ne 1) { throw 'ZIP does not contain exactly one monitor' }
if($KeepInstalled) {
    if([IO.Path]::GetFileName($Msi) -notmatch 'Windows-(x64|arm64)\.msi$') { throw 'Unknown package architecture' }
    $install=Join-Path $env:LOCALAPPDATA ("ckvision-"+$Matches[1])
}
else { $install=Join-Path $Scratch 'installed' }
$process=Start-Process msiexec.exe -ArgumentList @('/i',('"'+$Msi+'"'),'/qn','/norestart',('INSTALL_ROOT="'+$install+'"'),'/L*v',('"'+$Scratch+'/install.log"')) -Wait -PassThru
if($process.ExitCode -notin @(0,3010)) { throw "MSI install failed: $($process.ExitCode)" }
$executable=Join-Path $install 'bin/ck-sysmon.exe'
try {
    $prefix=$payload[0].Directory.Parent.FullName
    $files=@(Get-ChildItem $prefix -Recurse -File)
    foreach($file in $files) {
        $relative=$file.FullName.Substring($prefix.Length+1)
        $target=Join-Path $install $relative
        if(!(Test-Path $target) -or (Get-FileHash $target).Hash -ine (Get-FileHash $file.FullName).Hash) { throw "MSI payload differs: $relative" }
    }
    if((Get-FileHash $executable).Hash -ine (Get-FileHash $payload[0].FullName).Hash) { throw 'MSI installed bytes differ from ZIP' }
    if((& $executable --version) -cne "ck-sysmon $Version" -or $LASTEXITCODE -ne 0) { throw 'Installed version mismatch' }
    $sample=& $executable --sample
    if($LASTEXITCODE -ne 0 -or $sample[0] -cne 'CKSYSMON 2' -or $sample[-1] -cne 'END') { throw 'Installed native collector failed' }
    $userPath=[string][Environment]::GetEnvironmentVariable('Path','User')
    if($userPath.Split(';') -inotcontains (Join-Path $install 'bin')) { throw 'MSI did not register the user PATH' }
    @{version=$Version;executable=$executable;sha256=(Get-FileHash $executable).Hash;msi=$Msi;zip=$Zip;sample=$sample} |
        ConvertTo-Json -Depth 4 | Set-Content "$Scratch/verification.json" -Encoding utf8
} finally {
    if(!$KeepInstalled) {
        $process=Start-Process msiexec.exe -ArgumentList @('/x',('"'+$Msi+'"'),'/qn','/norestart','/L*v',('"'+$Scratch+'/uninstall.log"')) -Wait -PassThru
        if($process.ExitCode -notin @(0,3010)) { throw 'MSI uninstall failed' }
        foreach($file in $files) {
            $target=Join-Path $install $file.FullName.Substring($prefix.Length+1)
            if(Test-Path $target) { throw "MSI left payload behind: $target" }
        }
        $remaining=[string][Environment]::GetEnvironmentVariable('Path','User')
        if($remaining.Split(';') -icontains (Join-Path $install 'bin')) { throw 'MSI left a stale user PATH entry' }
    }
}
Write-Output "PASS complete MSI/ZIP payload identity, version, sample and user PATH"
if(!$KeepInstalled) { Write-Output 'PASS uninstall removed all package files and user PATH' }
