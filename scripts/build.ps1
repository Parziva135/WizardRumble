param(
    [string]$SdkPath = (Join-Path $env:APPDATA 'bakkesmod\bakkesmod\bakkesmodsdk'),
    [switch]$RunTests
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'toolchain.ps1')
Initialize-WizardToolchain -Root $root

$sdk = (Resolve-Path -LiteralPath $SdkPath).Path
$sdkLibrary = Join-Path $sdk 'lib\pluginsdk.lib'
if (-not (Test-Path -LiteralPath $sdkLibrary)) { throw "BakkesMod SDK library missing: $sdkLibrary" }

$build = Join-Path $root 'build'
$obj = Join-Path $build 'obj'
$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Path $build, $obj, $dist -Force | Out-Null
$sources = @(Get-ChildItem -LiteralPath (Join-Path $root 'src') -Filter '*.cpp' -File | Sort-Object Name)
if (-not $sources.Count) { throw 'No src/*.cpp source files found.' }
$output = Join-Path $dist 'WizardRumble.dll'
$arguments = @('/nologo', '/LD', '/std:c++17', '/EHsc', '/MD', '/O2', '/W4', '/utf-8', '/DNOMINMAX', '/DWIN32_LEAN_AND_MEAN', '/D_CRT_SECURE_NO_WARNINGS', ('/I"' + (Join-Path $root 'include') + '"'), ('/I"' + (Join-Path $sdk 'include') + '"'), ('/Fo"' + $obj + '/"'), ('/Fe"' + $output + '"'))
$arguments += $sources | ForEach-Object { '"' + $_.FullName + '"' }
$arguments += @('/link', ('"' + $sdkLibrary + '"'), '/MACHINE:X64', ('/IMPLIB:"' + (Join-Path $build 'WizardRumble.lib') + '"'), ('/PDB:"' + (Join-Path $build 'WizardRumble.pdb') + '"'))
$response = Join-Path $build 'plugin.rsp'
[IO.File]::WriteAllText($response, ($arguments -join ' '), [Text.UTF8Encoding]::new($false))
& cl.exe "@$response"
if ($LASTEXITCODE -ne 0) { throw "Plugin compilation failed with exit code $LASTEXITCODE." }
Write-Host "Built $output"

if ($RunTests) { & (Join-Path $PSScriptRoot 'test.ps1') }
