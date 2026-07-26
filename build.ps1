# RaidenScript build sarmalayicisi (Windows / PowerShell)
#
#   .\build.ps1                                  derle
#   .\build.ps1 -Debug                           sanitizer'li derle
#   .\build.ps1 -Run examples\01-temeller.rai    derle + calistir
#   .\build.ps1 -Clean                           temizle
#
# w64devkit'i kendisi bulur; sistem PATH'ine dokunmaz.

param(
    [switch]$Debug,
    [switch]$Clean,
    [string]$Run
)

$ErrorActionPreference = 'Stop'

# --- Arac zincirini bul ---
$adaylar = @(
    "$env:USERPROFILE\tools\w64devkit\bin",
    'C:\w64devkit\bin',
    "$env:LOCALAPPDATA\w64devkit\bin"
)
$bin = $adaylar | Where-Object { Test-Path (Join-Path $_ 'g++.exe') } | Select-Object -First 1

if (-not $bin) {
    if (Get-Command g++ -ErrorAction SilentlyContinue) {
        Write-Host "w64devkit bulunamadi, PATH'teki g++ kullaniliyor" -ForegroundColor Yellow
    } else {
        Write-Error "C++ derleyicisi bulunamadi. w64devkit'i $env:USERPROFILE\tools\w64devkit altina kurun."
    }
} else {
    $env:PATH = "$bin;$env:PATH"     # 'as' ve 'ld' de PATH'te olmali
}

Push-Location $PSScriptRoot
try {
    if ($Clean) { make clean; return }

    $bayraklar = if ($Debug) { 'DEBUG=1' } else { '' }
    if ($bayraklar) { make $bayraklar } else { make }
    if ($LASTEXITCODE -ne 0) { Write-Error "derleme basarisiz" }

    if ($Run) { & '.\build\rs.exe' run $Run }
}
finally { Pop-Location }
