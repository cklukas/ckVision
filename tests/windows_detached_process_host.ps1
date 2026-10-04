# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
param(
    [Parameter(Mandatory=$true)][string]$ContractPath,
    [Parameter(Mandatory=$true)][string]$ScratchRoot,
    [switch]$Worker,
    [switch]$ForceScheduledHost
)
$ErrorActionPreference='Stop'
$ContractPath=(Resolve-Path -LiteralPath $ContractPath).Path
$ScratchRoot=(Resolve-Path -LiteralPath $ScratchRoot).Path

if($Worker) {
    $env:TEMP=$ScratchRoot; $env:TMP=$ScratchRoot; $env:TMPDIR=$ScratchRoot
    $ErrorActionPreference='Continue'
    & $ContractPath *> (Join-Path $ScratchRoot 'contract.log')
    $code=$LASTEXITCODE
    $ErrorActionPreference='Stop'
    [IO.File]::WriteAllText((Join-Path $ScratchRoot 'exit.txt'),[string]$code)
    exit $code
}

& $ContractPath --probe-test-host
$probe=$LASTEXITCODE
if($probe -ne 0 -and $probe -ne 42) { throw "Test-host job query failed: $probe" }
if($probe -eq 0 -and !$ForceScheduledHost) {
    & $ContractPath
    exit $LASTEXITCODE
}

# The contract deliberately controls ALL ancestor jobs. A hosted CI process
# may already belong to a non-breakaway ancestor, making that setup impossible
# there. Run the exact same six cases under a same-user, limited-rights test
# host provided by Task Scheduler; never weaken the library's containment gate
# or skip a positive case. Registration/launch/query/timeout failures are red.
# This is test infrastructure only: library code never uses Task Scheduler.
$name='ckvision-detached-contract-'+[Guid]::NewGuid().ToString('N')
$scope=Join-Path $ScratchRoot $name
New-Item -ItemType Directory -Path $scope | Out-Null
$shell=Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
$arguments="-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Worker -ContractPath `"$ContractPath`" -ScratchRoot `"$scope`""
$action=New-ScheduledTaskAction -Execute $shell -Argument $arguments -WorkingDirectory $scope
$user=[Security.Principal.WindowsIdentity]::GetCurrent().Name
$principal=New-ScheduledTaskPrincipal -UserId $user -LogonType S4U -RunLevel Limited
$settings=New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Seconds 60) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
$registered=$false
try {
    Register-ScheduledTask -TaskName $name -Action $action -Principal $principal -Settings $settings | Out-Null
    $registered=$true
    Write-Output "Controlled-job contract: same-user scheduled test host; ambient_probe=$probe task=$name"
    Start-ScheduledTask -TaskName $name
    $deadline=[DateTime]::UtcNow.AddSeconds(65)
    $result=Join-Path $scope 'exit.txt'
    while(!(Test-Path -LiteralPath $result) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
    $log=Join-Path $scope 'contract.log'
    if(Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log }
    if(!(Test-Path -LiteralPath $result)) {
        $info=Get-ScheduledTaskInfo -TaskName $name
        throw "Scheduled test host did not finish; last_task_result=$($info.LastTaskResult)"
    }
    $code=[int](Get-Content -LiteralPath $result -Raw)
    if($code -ne 0) { throw "Controlled-job contract failed: $code" }
} finally {
    if($registered) {
        if((Get-ScheduledTask -TaskName $name).State -eq 'Running') { Stop-ScheduledTask -TaskName $name }
        Unregister-ScheduledTask -TaskName $name -Confirm:$false
    }
}
