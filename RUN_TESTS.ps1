# Verify configuration and run the desktop Unity tests.
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
if ((Test-Path Env:PLATFORMIO_DEFAULT_ENVS) -and ($env:PLATFORMIO_DEFAULT_ENVS -ne 'native_tests')) {
    Write-Warning 'PLATFORMIO_DEFAULT_ENVS overrides platformio.ini. Clearing it for this process.'
    Remove-Item Env:PLATFORMIO_DEFAULT_ENVS
}
$bytes = [System.IO.File]::ReadAllBytes((Join-Path $PSScriptRoot 'platformio.ini'))
if ($bytes.Length -ge 3 -and $bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191) {
    throw 'platformio.ini contains a UTF-8 BOM; this ZIP should contain a BOM-free file.'
}
Write-Host 'Checking computed PlatformIO environment...' -ForegroundColor Cyan
$config = (pio project config | Out-String)
if ($LASTEXITCODE -ne 0) { throw 'pio project config failed' }
Write-Host $config
if ($config -notmatch 'default_envs\s*=\s*native_tests') {
    throw 'Native environment is not the default. Open this clean project folder.'
}
$gcc = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $gcc) {
    throw 'Windows GCC was not found. Install MSYS2 UCRT64 GCC and add its bin folder to PATH, then run again.'
}
Write-Host 'Running the Native Unity tests...' -ForegroundColor Cyan
pio test -e native_tests
if ($LASTEXITCODE -ne 0) { throw 'Native tests failed. Read the first actual error above.' }
Write-Host 'Native Unity tests passed.' -ForegroundColor Green
