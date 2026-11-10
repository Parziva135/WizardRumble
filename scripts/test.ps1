$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'toolchain.ps1')
Initialize-WizardToolchain -Root $root

$build = Join-Path $root 'build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$source = Join-Path $root 'tests\core_tests.cpp'
if (-not (Test-Path -LiteralPath $source)) { throw "Tests not found: $source" }
$binary = Join-Path $build 'core_tests.exe'
$arguments = @('/nologo', '/std:c++17', '/EHsc', '/MD', '/O2', '/W4', '/utf-8', '/DNOMINMAX', ('/I"' + (Join-Path $root 'include') + '"'), ('/Fo"' + (Join-Path $build 'core_tests.obj') + '"'), ('/Fe"' + $binary + '"'), ('"' + $source + '"'), '/link', '/MACHINE:X64')
$response = Join-Path $build 'tests.rsp'
[IO.File]::WriteAllText($response, ($arguments -join ' '), [Text.UTF8Encoding]::new($false))
& cl.exe "@$response"
if ($LASTEXITCODE -ne 0) { throw "Test compilation failed with exit code $LASTEXITCODE." }
& $binary
if ($LASTEXITCODE -ne 0) { throw "Core tests failed with exit code $LASTEXITCODE." }
