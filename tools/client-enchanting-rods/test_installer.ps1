param([Parameter(Mandatory=$true)][string]$Candidate)
$ErrorActionPreference = 'Stop'
$candidateRoot = (Resolve-Path -LiteralPath $Candidate).Path
$testRoot = Join-Path (Split-Path $candidateRoot -Parent) ('rod-installer-fixtures-' + [guid]::NewGuid().ToString('N'))
$payload = Join-Path $testRoot 'package'
New-Item -ItemType Directory -Path $payload | Out-Null
Copy-Item -LiteralPath (Join-Path $candidateRoot 'Install.ps1') -Destination (Join-Path $payload 'Install.ps1')
Copy-Item -LiteralPath (Join-Path $candidateRoot 'Data') -Destination (Join-Path $payload 'Data') -Recurse
Copy-Item -LiteralPath (Join-Path $candidateRoot 'Interface') -Destination (Join-Path $payload 'Interface') -Recurse
$baseManifest = Get-Content -LiteralPath (Join-Path $candidateRoot 'manifest.json') -Raw
# Only this fixture script supplies fake process records. Production installer
# uses the real cmdlet and fails closed if a running WoW path cannot be read.
$TestRodProcesses = @()
function Get-Process { [CmdletBinding()]param([string]$Name) return $TestRodProcesses }
function New-Case([string]$Name) {
    $client = Join-Path $testRoot $Name
    New-Item -ItemType Directory -Path (Join-Path $client 'Data') | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $client 'Interface\AddOns') | Out-Null
    [IO.File]::WriteAllText((Join-Path $client 'WoW.exe'),'fixture executable; never launched')
    [IO.File]::WriteAllText((Join-Path $client 'Data\fixture.MPQ'),'fixture archive; never opened by WoW')
    $manifest = $baseManifest | ConvertFrom-Json
    $manifest.client_exe_sha256 = (Get-FileHash -LiteralPath (Join-Path $client 'WoW.exe')).Hash
    $manifest.spell_source = 'fixture.MPQ'
    $manifest.source_archive_sha256 = (Get-FileHash -LiteralPath (Join-Path $client 'Data\fixture.MPQ')).Hash
    $file = Get-Item -LiteralPath (Join-Path $client 'Data\fixture.MPQ')
    $manifest.client_archives = @([pscustomobject]@{name='fixture.MPQ';bytes=$file.Length;modified_ticks=$file.LastWriteTimeUtc.Ticks})
    $manifest | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath (Join-Path $payload 'manifest.json') -Encoding utf8
    return $client
}
function Check([bool]$Value,[string]$Why) { if (!$Value) { throw $Why } }
function Reject([scriptblock]$Action,[string]$Why) {
    $failed = $false
    try { & $Action | Out-Null } catch { $failed = $true }
    Check $failed $Why
}
$installer = Join-Path $payload 'Install.ps1'
$client = New-Case 'running-client'
$TestRodProcesses = @([pscustomobject]@{Path=(Join-Path $client 'WoW.exe')})
Reject { & $installer -ClientPath $client } 'Running client accepted'
Check (!(Test-Path -LiteralPath (Join-Path $client 'Data\patch-Z.MPQ'))) 'Running client was mutated'
$TestRodProcesses = @()
$client = New-Case 'successful-install-remove'
$before = (Get-FileHash -LiteralPath (Join-Path $client 'Data\fixture.MPQ')).Hash
& $installer -ClientPath $client | Out-Null
Check (Test-Path -LiteralPath (Join-Path $client 'Data\patch-Z.MPQ')) 'Install failed'
& $installer -ClientPath $client -Remove | Out-Null
Check (!(Test-Path -LiteralPath (Join-Path $client 'Data\patch-Z.MPQ'))) 'Remove left patch active'
Check (!(Test-Path -LiteralPath (Join-Path $client 'Interface\AddOns\WarbandEnchantingRods'))) 'Remove left addon active'
Check ((Get-FileHash -LiteralPath (Join-Path $client 'Data\fixture.MPQ')).Hash -eq $before) 'Original archive changed'
Check (@(Get-ChildItem -LiteralPath (Join-Path $client 'Archive') -Directory).Count -eq 1) 'Removal did not retain backup'
$client = New-Case 'stale-client'
Add-Content -LiteralPath (Join-Path $client 'Data\fixture.MPQ') -Value 'new client version'
Reject { & $installer -ClientPath $client } 'Stale archive accepted'
Check (!(Test-Path -LiteralPath (Join-Path $client 'Interface\AddOns\WarbandEnchantingRods'))) 'Stale client was mutated'
$client = New-Case 'existing-patch'
[IO.File]::WriteAllText((Join-Path $client 'Data\patch-Z.MPQ'),'existing user patch')
Reject { & $installer -ClientPath $client } 'Existing destination accepted'
Check ((Get-Content -LiteralPath (Join-Path $client 'Data\patch-Z.MPQ') -Raw) -eq 'existing user patch') 'Existing patch overwritten'
$client = New-Case 'modified-addon'
& $installer -ClientPath $client | Out-Null
Add-Content -LiteralPath (Join-Path $client 'Interface\AddOns\WarbandEnchantingRods\WarbandEnchantingRods.lua') -Value '-- user change'
Reject { & $installer -ClientPath $client -Remove } 'Modified addon removed'
Check (Test-Path -LiteralPath (Join-Path $client 'Data\patch-Z.MPQ')) 'Partial removal after mismatch'
Write-Output "PASS: install/remove, retained originals/rollback, stale client, existing patch and modified addon. Fixtures: $testRoot"
