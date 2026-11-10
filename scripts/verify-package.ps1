param([string]$Version = '0.9.2')
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?$') { throw 'Invalid version.' }
. (Join-Path $PSScriptRoot 'release-files.ps1')
$root = Split-Path -Parent $PSScriptRoot
$runtime = Get-WizardRuntimeMap
$sourceFiles = Get-WizardSourceFileMap $root
$windowsFiles = [ordered]@{}
foreach ($relative in $runtime.Keys) { $windowsFiles[$runtime[$relative]] = $relative }
$playerFiles = Get-WizardPlayerFileMap
foreach ($relative in $playerFiles.Keys) { $windowsFiles[$playerFiles[$relative]] = $relative }

Add-Type -AssemblyName System.IO.Compression.FileSystem
function Test-WizardArchive($files, $checksumPaths, [string]$zipPath) {
    $archive = [IO.Compression.ZipFile]::OpenRead($zipPath)
    $sha = [Security.Cryptography.SHA256]::Create()
    $seen = @{}
    try {
        $expectedChecksums = @($checksumPaths | ForEach-Object {
            $hash = (Get-FileHash -LiteralPath (Join-Path $root $files[$_]) -Algorithm SHA256).Hash.ToLowerInvariant()
            "$hash  $_"
        }) -join "`n"
        foreach ($entry in $archive.Entries) {
            if (-not $entry.FullName.StartsWith('WizardRumble/')) { throw "Unexpected archive root: $($entry.FullName)" }
            $relative = $entry.FullName.Substring('WizardRumble/'.Length)
            if ($seen.ContainsKey($relative)) { throw "Duplicate archive path: $relative" }
            $seen[$relative] = $true
            if ($relative -match '(^|/)(\.git|\.tools|\.research|build|downloads)(/|$)|(^|/)\.\.(?:/|$)|\.(mov|mp4|glb|pdb|obj|lib|exp)$' -or $relative.Contains('\')) { throw "Forbidden archive path: $relative" }
            $stream = $entry.Open()
            try {
                if ($relative -eq 'SHA256SUMS.txt') {
                    $reader = [IO.StreamReader]::new($stream)
                    try { $text = $reader.ReadToEnd().Trim() } finally { $reader.Dispose() }
                    if ($text.Replace("`r`n", "`n") -ne $expectedChecksums) { throw 'Embedded checksum file differs from sixteen final runtime checksums.' }
                } else {
                    if (-not $files.Contains($relative)) { throw "Unexpected archive file: $relative" }
                    $hash = ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '').ToLowerInvariant()
                    $expected = (Get-FileHash -LiteralPath (Join-Path $root $files[$relative]) -Algorithm SHA256).Hash.ToLowerInvariant()
                    if ($hash -ne $expected) { throw "Packaged bytes differ: $relative" }
                }
            } finally { $stream.Dispose() }
        }
        foreach ($relative in @($files.Keys) + @('SHA256SUMS.txt')) {
            if (-not $seen.ContainsKey($relative)) { throw "Required archive file missing: $relative" }
        }
        Write-Host "Verified $([IO.Path]::GetFileName($zipPath)): $($archive.Entries.Count) entries, exact bytes and sixteen runtime checksums."
    } finally { $archive.Dispose(); $sha.Dispose() }
}
foreach ($relative in @('include/PatronumMesh.h', 'include/PatronumVolume.h', 'include/PatronumShell.h', 'include/SpellGlow.h', 'src/SpellGlow.cpp', 'scripts/bake-patronum-model.py', 'scripts/bake-patronum-volume.py', 'scripts/bake-patronum-shell.py', 'scripts/generate-glow-textures.py', 'docs/MESHNET.md', 'docs/MODEL_CREDITS.md', 'docs/THIRD_PARTY.md')) {
    if (-not $sourceFiles.Contains($relative)) { throw "Required source or credit missing: $relative" }
}
$mesh = Get-Content -LiteralPath (Join-Path $root 'include\PatronumMesh.h') -Raw
if ($mesh -notmatch 'std::array<Edge,\s*(\d+)>' -or [int]$Matches[1] -lt 1 -or [int]$Matches[1] -gt 380) { throw 'Patronum mesh must contain 1 to 380 bounded feature edges.' }
$core = Get-Content -LiteralPath (Join-Path $root 'include\SpellCore.h') -Raw
if ($core -notmatch 'SpellCount\s*=\s*11\s*;') { throw 'Expected eleven-spell pool.' }
if ($runtime.Count -ne 16) { throw 'Expected sixteen runtime files.' }
Test-WizardArchive $sourceFiles @($runtime.Keys) (Join-Path $root "dist\WizardRumble-$Version.zip")
Test-WizardArchive $windowsFiles @($runtime.Values) (Join-Path $root "dist\WizardRumble-$Version-Windows.zip")
