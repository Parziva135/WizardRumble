$ErrorActionPreference = 'Stop'

function Initialize-WizardToolchain {
    param([string]$Root)

    if (Get-Command cl.exe -ErrorAction SilentlyContinue) { return }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsRoot) {
            $vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
            if (Test-Path -LiteralPath $vcvars) {
                $environment = & $env:ComSpec /d /s /c "`"call `"$vcvars`" >nul && set`""
                foreach ($line in $environment) {
                    if ($line -match '^([^=]+)=(.*)$') {
                        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
                    }
                }
                if (Get-Command cl.exe -ErrorAction SilentlyContinue) { return }
            }
        }
    }

    $portable = Join-Path $Root '.tools\msvc'
    $vcBase = Join-Path $portable 'VC\Tools\MSVC'
    $sdkBase = Join-Path $portable 'Windows Kits\10'
    if ((Test-Path -LiteralPath $vcBase) -and (Test-Path -LiteralPath (Join-Path $sdkBase 'Include'))) {
        $vc = Get-ChildItem -LiteralPath $vcBase -Directory | Sort-Object Name -Descending | Select-Object -First 1
        $sdk = Get-ChildItem -LiteralPath (Join-Path $sdkBase 'Include') -Directory | Sort-Object Name -Descending | Select-Object -First 1
        $env:PATH = (Join-Path $vc.FullName 'bin\Hostx64\x64') + ';' + (Join-Path $sdkBase "bin\$($sdk.Name)\x64") + ';' + $env:PATH
        $env:INCLUDE = @((Join-Path $vc.FullName 'include'), (Join-Path $sdk.FullName 'ucrt'), (Join-Path $sdk.FullName 'shared'), (Join-Path $sdk.FullName 'um'), (Join-Path $sdk.FullName 'winrt'), (Join-Path $sdk.FullName 'cppwinrt')) -join ';'
        $env:LIB = @((Join-Path $vc.FullName 'lib\x64'), (Join-Path $sdkBase "Lib\$($sdk.Name)\ucrt\x64"), (Join-Path $sdkBase "Lib\$($sdk.Name)\um\x64")) -join ';'
        if (Get-Command cl.exe -ErrorAction SilentlyContinue) { return }
    }

    throw 'MSVC x64 was not found. Use an x64 Developer PowerShell for Visual Studio 2022 with C++ Desktop tools, or put a portable MSVC and Windows SDK toolchain in .tools/msvc.'
}
