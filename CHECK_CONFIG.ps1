# Run from anywhere: powershell -ExecutionPolicy Bypass -File .\CHECK_CONFIG.ps1
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$path = Join-Path $PSScriptRoot 'platformio.ini'
$bytes = [System.IO.File]::ReadAllBytes($path)
if ($bytes.Length -ge 3 -and $bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191) {
    throw 'ERROR: platformio.ini has a UTF-8 BOM.'
}
if (Test-Path Env:PLATFORMIO_DEFAULT_ENVS) {
    Write-Warning ("PLATFORMIO_DEFAULT_ENVS overrides the default: $env:PLATFORMIO_DEFAULT_ENVS")
}
$config = pio project config | Out-String
if ($LASTEXITCODE -ne 0) { throw 'PlatformIO config is invalid' }
Write-Host $config
if ($config -notmatch 'default_envs\s*=\s*native_tests') { throw 'Default environment is not native_tests' }
if ($config -match '(?s)env:native_tests.*?board\s*=\s*esp32dev') {
    throw 'Native tests inherited an ESP32 board setting.'
}
Write-Host 'PlatformIO configuration check completed.' -ForegroundColor Green
