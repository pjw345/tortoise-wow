param(
    [Parameter(Mandatory=$true)][string]$ClientPath,
    [switch]$Remove
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $ClientPath).Path.TrimEnd('\')
$manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw | ConvertFrom-Json
function Child-Path([string]$Relative) {
    $path = [IO.Path]::GetFullPath((Join-Path $root $Relative))
    if (!$path.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Path escaped the client directory.' }
    $parent = $path
    while ($parent -and $parent.Length -ge $root.Length) {
        if (Test-Path -LiteralPath $parent) {
            if ((Get-Item -LiteralPath $parent -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing a linked path: $parent" }
        }
        $parent = Split-Path -Parent $parent
    }
    return $path
}
function Check-Hash([string]$Path,[string]$Expected) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf) -or (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Expected) { throw "File hash differs: $Path" }
}
foreach ($process in @(Get-Process -Name WoW -ErrorAction SilentlyContinue)) {
    if (!$process.Path -or $process.Path.StartsWith($root + '\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Close this WoW client completely before installing or removing the patch.' }
}
$patch = Child-Path 'Data\patch-Z.MPQ'
$addon = Child-Path 'Interface\AddOns\WarbandEnchantingRods'
$addonNames = @('Recipes.lua','WarbandEnchantingRods.lua','WarbandEnchantingRods.toc')
if ($Remove) {
    if (Test-Path -LiteralPath $patch) { Check-Hash $patch $manifest.mpq_sha256 }
    if (Test-Path -LiteralPath $addon) {
        $extra = @(Get-ChildItem -LiteralPath $addon -Force | Where-Object { $_.PSIsContainer -or $_.Name -notin $addonNames })
        if ($extra.Count) { throw 'The addon folder has extra content; move it manually to preserve your changes.' }
        foreach ($name in $addonNames) {
            $path = Child-Path ('Interface\AddOns\WarbandEnchantingRods\' + $name)
            if (Test-Path -LiteralPath $path) { Check-Hash $path $manifest.addon_sha256.$name }
        }
    }
    $backup = Child-Path ('Archive\WarbandEnchantingRods-disabled-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
    New-Item -ItemType Directory -Path $backup | Out-Null
    # All resolved source and destination paths stay inside the chosen client.
    if (Test-Path -LiteralPath $patch) { Move-Item -LiteralPath $patch -Destination (Join-Path $backup 'patch-Z.MPQ') }
    if (Test-Path -LiteralPath $addon) { Move-Item -LiteralPath $addon -Destination (Join-Path $backup 'WarbandEnchantingRods') }
    Write-Output "Patch disabled; files preserved in $backup"
    return
}
if ((Test-Path -LiteralPath $patch) -or (Test-Path -LiteralPath $addon)) { throw 'Patch/addon destination already exists; nothing was overwritten.' }
Check-Hash (Child-Path 'WoW.exe') $manifest.client_exe_sha256
$inventory = @(Get-ChildItem -LiteralPath (Child-Path 'Data') -File | Where-Object Extension -IEQ '.mpq')
if ($inventory.Count -ne $manifest.client_archives.Count) { throw 'Client archive list changed; rebuild the patch for this client.' }
foreach ($entry in $manifest.client_archives) {
    if ([IO.Path]::GetFileName($entry.name) -ne $entry.name) { throw 'Invalid archive filename.' }
    $file = Get-Item -LiteralPath (Child-Path ('Data\' + $entry.name))
    if ($file.Length -ne $entry.bytes -or $file.LastWriteTimeUtc.Ticks -ne [long]$entry.modified_ticks) { throw "Client archive changed: $($entry.name). Rebuild the patch." }
}
Check-Hash (Child-Path ('Data\' + $manifest.spell_source)) $manifest.source_archive_sha256
Check-Hash (Join-Path $PSScriptRoot 'Data\patch-Z.MPQ') $manifest.mpq_sha256
foreach ($name in $addonNames) { Check-Hash (Join-Path $PSScriptRoot ('Interface\AddOns\WarbandEnchantingRods\' + $name)) $manifest.addon_sha256.$name }
New-Item -ItemType Directory -Path $addon | Out-Null
foreach ($name in $addonNames) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot ('Interface\AddOns\WarbandEnchantingRods\' + $name)) -Destination (Join-Path $addon $name) }
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Data\patch-Z.MPQ') -Destination $patch
Check-Hash $patch $manifest.mpq_sha256
foreach ($name in $addonNames) { Check-Hash (Join-Path $addon $name) $manifest.addon_sha256.$name }
Write-Output 'Installed. Start the client and enable Warband Enchanting Rods. Live enchanting tests remain required.'
