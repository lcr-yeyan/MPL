$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$tauri = Join-Path $root 'src-tauri'
$release = Join-Path $tauri 'target\release'
$staging = Join-Path $tauri 'target\package\staging'
$dist = Join-Path $root 'dist-tauri'
$compiler = Join-Path $release 'mpl_studio.exe'
$resources = Join-Path $release 'resources'
$makensis = (Get-Command makensis -ErrorAction SilentlyContinue).Source
if (-not $makensis) {
    $makensis = 'C:\Program Files (x86)\NSIS\makensis.exe'
}
if (-not (Test-Path $makensis)) {
    throw 'NSIS is not installed. Install it with: winget install NSIS.NSIS'
}

Push-Location $root
try {
    pnpm build
    if ($LASTEXITCODE -ne 0) { throw 'frontend build failed' }
} finally {
    Pop-Location
}

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'prepare-tauri.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Tauri resource preparation failed' }

Push-Location $tauri
try {
    cargo build --release --features tauri/custom-protocol
    if ($LASTEXITCODE -ne 0) { throw 'Tauri app build failed' }
} finally {
    Pop-Location
}

if (-not (Test-Path $compiler)) { throw 'Tauri app binary is missing' }
if (-not (Test-Path $resources)) { throw 'Tauri resources are missing' }

New-Item -ItemType Directory -Force -Path $staging, $dist | Out-Null
Remove-Item (Join-Path $staging '*') -Recurse -Force -ErrorAction SilentlyContinue
Copy-Item $compiler (Join-Path $staging 'mpl_studio.exe') -Force
Copy-Item $resources (Join-Path $staging 'resources') -Recurse -Force

# DSH-created files carry a low integrity label; normalize before packaging.
& icacls $staging /setintegritylevel '(OI)(CI)Medium' | Out-Null

Push-Location $tauri
try {
    & $makensis 'installer.nsi'
    if ($LASTEXITCODE -ne 0) { throw "makensis failed with exit code $LASTEXITCODE" }
} finally {
    Pop-Location
}

$installer = Join-Path $dist 'MPL-Studio_0.1.0_x64-setup.exe'
if (-not (Test-Path $installer)) { throw 'Installer output was not produced.' }
& icacls $installer /setintegritylevel Medium | Out-Null

$portable = Join-Path $dist 'MPL-Studio_0.1.0_x64-portable.zip'
Remove-Item $portable -Force -ErrorAction SilentlyContinue
$zipAttempt = 0
while (-not (Test-Path $portable) -and $zipAttempt -lt 4) {
    $zipAttempt++
    Remove-Item $portable -Force -ErrorAction SilentlyContinue
    try {
        Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $portable -CompressionLevel Optimal -ErrorAction Stop
    } catch {
        Start-Sleep -Seconds 2
    }
}
if (-not (Test-Path $portable)) { throw 'Portable zip was not produced (file lock).' }

Write-Output "installer: $installer"
Write-Output "portable:  $portable"
Get-Item $installer, $portable | Select-Object Name, Length
exit 0
