$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$repo = Split-Path -Parent $root
$resources = Join-Path $root 'src-tauri\resources'
$bin = Join-Path $resources 'bin'
$stdlib = Join-Path $resources 'stdlib'
$examples = Join-Path $resources 'examples'

$compiler = Join-Path $repo 'build\Release\mplc.exe'
if (-not (Test-Path $compiler)) {
    $compiler = Join-Path $repo 'build\Debug\mplc.exe'
}
if (-not (Test-Path $compiler)) {
    throw 'Build mplc first: cmake --build build --config Release --target mplc'
}

New-Item -ItemType Directory -Force -Path $bin, $stdlib, $examples | Out-Null
Remove-Item (Join-Path $bin '*') -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $stdlib '*') -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $examples '*') -Force -ErrorAction SilentlyContinue

Copy-Item $compiler (Join-Path $bin 'mplc.exe') -Force
Copy-Item (Join-Path $repo 'stdlib\*.mpl') $stdlib -Force
Copy-Item (Join-Path $repo 'examples\*.mpl') $examples -Force

Write-Output "prepared Tauri resources:"
Write-Output "  $bin\mplc.exe"
Write-Output "  $stdlib ($((Get-ChildItem $stdlib -Filter *.mpl).Count) modules)"
Write-Output "  $examples ($((Get-ChildItem $examples -Filter *.mpl).Count) examples)"
