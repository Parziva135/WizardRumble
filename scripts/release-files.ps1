# One ordered manifest for installation and both ZIP layouts.
function Get-WizardRuntimeMap {
    $files = [ordered]@{
        'dist/WizardRumble.dll' = 'plugins/WizardRumble.dll'
        'plugins/settings/wizardrumble.set' = 'plugins/settings/wizardrumble.set'
    }
    foreach ($name in @('accio', 'avada_kedavra', 'leviosa_cast', 'leviosa_float', 'eat_slugs', 'stupefy', 'petrificus_totalus', 'protego', 'expecto_patronum', 'bombarda_maxima', 'obliviate', 'imperio')) {
        $files["assets/audio/$name.wav"] = "data/wizardrumble/audio/$name.wav"
    }
    foreach ($name in @('glow_soft', 'glow_mist')) {
        $files["assets/textures/$name.png"] = "data/wizardrumble/textures/$name.png"
    }
    return $files
}

function Get-WizardPlayerFileMap {
    $files = [ordered]@{
        'START_HERE.md' = 'START_HERE.md'
        'LICENSE' = 'LICENSE'
        'scripts/install.ps1' = 'scripts/install.ps1'
        'scripts/release-files.ps1' = 'scripts/release-files.ps1'
        'docs/SHARING.md' = 'docs/SHARING.md'
        'docs/MESHNET.md' = 'docs/MESHNET.md'
        'docs/MODEL_CREDITS.md' = 'docs/MODEL_CREDITS.md'
        'docs/THIRD_PARTY.md' = 'docs/THIRD_PARTY.md'
    }
    return $files
}

function Get-WizardSourceFileMap([string]$Root) {
    $files = [ordered]@{}
    $runtime = Get-WizardRuntimeMap
    foreach ($relative in @($runtime.Keys) + @('README.md', 'START_HERE.md', 'LICENSE', '.gitignore')) { $files[$relative] = $relative }
    foreach ($folder in @('src', 'include', 'tests', 'scripts', 'docs', 'plugins\settings')) {
        foreach ($file in Get-ChildItem -LiteralPath (Join-Path $Root $folder) -File -Recurse | Where-Object { $_.Extension -in @('.cpp', '.h', '.md', '.ps1', '.py', '.set') -and $_.FullName -notmatch '[\\/]__pycache__[\\/]' }) {
            $relative = $file.FullName.Substring($Root.Length + 1).Replace('\', '/')
            $files[$relative] = $relative
        }
    }
    return $files
}
