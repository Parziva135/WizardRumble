param([string]$Version = '0.9.2')
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?$') { throw 'Use a version such as 0.9.2 or 0.9.2-beta.1.' }
. (Join-Path $PSScriptRoot 'release-files.ps1')
$root = Split-Path -Parent $PSScriptRoot
$runtime = Get-WizardRuntimeMap
$sourceFiles = Get-WizardSourceFileMap $root
$windowsFiles = [ordered]@{}
foreach ($relative in $runtime.Keys) { $windowsFiles[$runtime[$relative]] = $relative }
$playerFiles = Get-WizardPlayerFileMap
foreach ($relative in $playerFiles.Keys) { $windowsFiles[$playerFiles[$relative]] = $relative }
foreach ($relative in $sourceFiles.Keys) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $sourceFiles[$relative]) -PathType Leaf)) { throw "Required release file missing: $relative" }
}
foreach ($relative in $playerFiles.Keys) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf)) { throw "Required player instructions missing: $relative" }
}

Add-Type -AssemblyName System.IO.Compression
function Write-WizardArchive($files, $checksumPaths, [string]$output) {
    $temporary = Join-Path (Split-Path -Parent $output) ('WizardRumble-' + [guid]::NewGuid().ToString('N') + '.tmp.zip')
    $stream = [IO.File]::Open($temporary, [IO.FileMode]::CreateNew, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $archive = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create, $false)
    try {
        foreach ($relative in $files.Keys) {
            $entry = $archive.CreateEntry(('WizardRumble/' + $relative), [IO.Compression.CompressionLevel]::Optimal)
            $entryStream = $entry.Open()
            $inputStream = [IO.File]::OpenRead((Join-Path $root $files[$relative]))
            try { $inputStream.CopyTo($entryStream) }
            finally { $inputStream.Dispose(); $entryStream.Dispose() }
        }
        $entry = $archive.CreateEntry('WizardRumble/SHA256SUMS.txt')
        $writer = [IO.StreamWriter]::new($entry.Open(), [Text.UTF8Encoding]::new($false))
        try {
            foreach ($relative in $checksumPaths) {
                $hash = (Get-FileHash -LiteralPath (Join-Path $root $files[$relative]) -Algorithm SHA256).Hash.ToLowerInvariant()
                $writer.WriteLine("$hash  $relative")
            }
        } finally { $writer.Dispose() }
    } finally { $archive.Dispose(); $stream.Dispose() }
    Move-Item -LiteralPath $temporary -Destination $output -Force
    Write-Host "Packaged $output ($($files.Count + 1) entries)"
}
Write-WizardArchive $sourceFiles @($runtime.Keys) (Join-Path $root "dist\WizardRumble-$Version.zip")
Write-WizardArchive $windowsFiles @($runtime.Values) (Join-Path $root "dist\WizardRumble-$Version-Windows.zip")
Write-Host 'Both ZIPs include sixteen runtime checksums. Source movies/GLB, SDK, compiler, downloads, build and .git are excluded.'
Write-Host 'Prepared locally only; no GitHub repository, upload, remote or release was created.'
