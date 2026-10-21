# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
param([Parameter(Mandatory=$true)][string]$Root,
      [ValidateSet('x86_64','arm64')][string]$Architecture='x86_64')
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path "$Root/tmp" | Out-Null
foreach($name in @('TMP','TEMP','TMPDIR')) { [Environment]::SetEnvironmentVariable($name,"$Root/tmp",'Process') }
$filename="cmake-4.4.4-windows-$Architecture.zip"
$base='https://github.com/Kitware/CMake/releases/download/v4.4.4'
Invoke-WebRequest "$base/$filename" -OutFile "$Root/cmake.zip" -TimeoutSec 180
Invoke-WebRequest "$base/cmake-4.4.4-SHA-256.txt" -OutFile "$Root/cmake-digests.txt" -TimeoutSec 60
$expected=@(Get-Content "$Root/cmake-digests.txt" | Where-Object {$_ -match ('^[0-9a-fA-F]{64}\s+\*?'+[regex]::Escape($filename)+'$')})
if($expected.Count -ne 1 -or (Get-FileHash "$Root/cmake.zip").Hash -ine $expected[0].Substring(0,64)) { throw 'Publisher CMake checksum mismatch' }
Expand-Archive "$Root/cmake.zip" "$Root/cmake" -Force
dotnet tool install wix --version 5.0.2 --tool-path "$Root/wix"
if($LASTEXITCODE -ne 0) { throw 'Versioned WiX installation failed' }
$env:WIX_EXTENSIONS="$Root/wix-extensions"
& "$Root/wix/wix.exe" extension add --global WixToolset.UI.wixext/5.0.2
if($LASTEXITCODE -ne 0) { throw 'WiX extension installation failed' }
@{cmake="$Root/cmake/cmake-4.4.4-windows-$Architecture/bin";wix="$Root/wix";extensions=$env:WIX_EXTENSIONS} |
    ConvertTo-Json | Set-Content "$Root/tools.json" -Encoding utf8
Write-Output "CMake and WiX verified under $Root"
