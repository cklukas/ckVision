# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
param([string] $BuildDir = 'build')
$ErrorActionPreference = 'Stop'

$version = '1.24.260710001'
$expectedHash = '175640566A3B59C4B132070EE96C2C77E5AB7EDD2E92732A5EB3610BBF63D90E'
$packageName = "microsoft.windows.console.conpty.$version.nupkg"
$url = "https://api.nuget.org/v3-flatcontainer/microsoft.windows.console.conpty/$version/$packageName"
if (-not $env:RUNNER_TEMP) {
    throw 'RUNNER_TEMP must name a CI-owned temporary workspace'
}
$root = Join-Path $env:RUNNER_TEMP ("ckvision-conpty-modern-" + [guid]::NewGuid().ToString('N'))
$sourceRunner = Join-Path (Resolve-Path $BuildDir) 'tests\Release\cvision_tests.exe'
New-Item -ItemType Directory -Force -Path $root | Out-Null
try {
    $package = Join-Path $root $packageName
    Invoke-WebRequest -Uri $url -OutFile $package -UseBasicParsing
    $actualHash = (Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash
    if ($actualHash -ne $expectedHash) {
        throw "ConPTY package hash mismatch: $actualHash"
    }
    $archive = Join-Path $root 'conpty.zip'
    Copy-Item -LiteralPath $package -Destination $archive
    $contents = Join-Path $root 'package'
    Expand-Archive -LiteralPath $archive -DestinationPath $contents
    $architecture = switch ($env:PROCESSOR_ARCHITECTURE) {
        'AMD64' { 'x64' }
        'ARM64' { 'arm64' }
        default { throw "Unsupported Windows CI architecture: $env:PROCESSOR_ARCHITECTURE" }
    }
    $runtime = Join-Path $root 'runtime'
    New-Item -ItemType Directory -Force -Path (Join-Path $runtime $architecture) | Out-Null
    Copy-Item -LiteralPath $sourceRunner -Destination (Join-Path $runtime 'cvision_tests.exe')
    Copy-Item -LiteralPath (Join-Path $contents "runtimes\win-$architecture\native\conpty.dll") -Destination (Join-Path $runtime 'conpty.dll')
    Copy-Item -LiteralPath (Join-Path $contents "build\native\runtimes\$architecture\OpenConsole.exe") -Destination (Join-Path $runtime "$architecture\OpenConsole.exe")
    $originalHash = (Get-FileHash -LiteralPath $sourceRunner -Algorithm SHA256).Hash
    $runnerHash = (Get-FileHash -LiteralPath (Join-Path $runtime 'cvision_tests.exe') -Algorithm SHA256).Hash
    if ($runnerHash -ne $originalHash) { throw 'Modern ConPTY runner differs from the built test executable' }
    $env:CKVISION_EXPECT_MODERN_CONPTY = '1'
    & (Join-Path $runtime 'cvision_tests.exe') --suite test_windows_terminal_subsession.cpp
    if ($LASTEXITCODE -ne 0) { throw "Modern ConPTY suite failed with exit code $LASTEXITCODE" }
} finally {
    Remove-Item Env:CKVISION_EXPECT_MODERN_CONPTY -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
