param([string]$BakkesModPath = (Join-Path $env:APPDATA 'bakkesmod\bakkesmod'))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'release-files.ps1')
$root = Split-Path -Parent $PSScriptRoot
$runtime = Get-WizardRuntimeMap
$sourceLayout = Test-Path -LiteralPath (Join-Path $root 'dist\WizardRumble.dll') -PathType Leaf
$items = @()
foreach ($sourceRelative in $runtime.Keys) {
    $relative = $runtime[$sourceRelative]
    $source = Join-Path $root $(if ($sourceLayout) { $sourceRelative } else { $relative })
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Required installation file missing: $source" }
    $items += @{ Source = $source; Relative = $relative; Hash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash }
}
if (-not (Test-Path -LiteralPath (Join-Path $BakkesModPath 'plugins') -PathType Container)) {
    throw "BakkesMod plugins folder not found: $BakkesModPath. Use File > Open BakkesMod folder in the injector to find it."
}
# Validate the whole packaged payload before replacing any installed file.
$checksumPath = Join-Path $root 'SHA256SUMS.txt'
if (-not $sourceLayout -and -not (Test-Path -LiteralPath $checksumPath -PathType Leaf)) {
    throw 'The Windows package is missing SHA256SUMS.txt. Extract a fresh copy of the ZIP before installing.'
}
if (Test-Path -LiteralPath $checksumPath -PathType Leaf) {
    $checksums = @{}
    foreach ($line in Get-Content -LiteralPath $checksumPath) {
        if ($line -notmatch '^([a-fA-F0-9]{64})  (.+)$' -or $checksums.ContainsKey($Matches[2])) { throw 'Invalid or duplicate checksum entry.' }
        $checksums[$Matches[2]] = $Matches[1]
    }
    foreach ($sourceRelative in $runtime.Keys) {
        $relative = if ($sourceLayout) { $sourceRelative } else { $runtime[$sourceRelative] }
        if (-not $checksums.ContainsKey($relative)) { throw "Missing packaged checksum: $relative" }
        $actual = (Get-FileHash -LiteralPath (Join-Path $root $relative) -Algorithm SHA256).Hash
        if ($actual -ne $checksums[$relative]) { throw "Package checksum mismatch: $relative. Extract a fresh copy of the ZIP." }
    }
}
foreach ($item in $items) {
    $destination = Join-Path $BakkesModPath $item.Relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $item.Source -Destination $destination -Force
    if ($item.Hash -ne (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash) { throw "Copied checksum mismatch: $($item.Relative)" }
}
Write-Host 'Copied DLL, settings, twelve WAVs and two glow textures; verified all sixteen copied checksums.'
Write-Host 'In F6: plugin load WizardRumble, then wr_status. Require version=0.9.2 and glow_textures=1.'
Write-Host 'Audio is in data\wizardrumble\audio; wr_audio_reload reloads clips. wr_glow_reload reloads textures.'
Write-Host 'No separate GLB is needed. Existing key binds and plugin startup settings were left unchanged.'
Write-Host 'For multiplayer, everyone needs NetcodePlugin 166 and a reachable player-hosted LAN match. See START_HERE.md.'
