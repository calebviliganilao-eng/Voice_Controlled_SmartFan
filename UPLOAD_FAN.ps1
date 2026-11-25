# Safe serial-port selection + PlatformIO upload.
# Example: powershell -ExecutionPolicy Bypass -File .\UPLOAD_FAN.ps1 -Environment sinric_lab
# Example: powershell -ExecutionPolicy Bypass -File .\UPLOAD_FAN.ps1 -Environment sinric_dc -Port COM3
param(
    [ValidateSet('sinric_lab','sinric_dc')]
    [string]$Environment = 'sinric_lab',
    [string]$Port = ''
)
$ErrorActionPreference = 'Stop'
if ($Port -and $Port -notmatch '^COM[0-9]+$') { throw 'Port must be a name such as COM3.' }
Set-Location -LiteralPath $PSScriptRoot
if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
    throw 'PlatformIO CLI (pio) was not found. Open the PlatformIO terminal in VS Code.'
}
Write-Host 'Checking serial devices...' -ForegroundColor Cyan
& pio device list
Write-Host ''
$devices = @(Get-CimInstance Win32_PnPEntity | Where-Object {
    $_.Name -and $_.Name -match '\(COM\d+\)' -and $_.Status -eq 'OK'
})
if ($Port) {
    $matching = @($devices | Where-Object { $_.Name -match ('\(' + [regex]::Escape($Port) + '\)') })
    if ($matching.Count -lt 1) {
        throw "Port $Port is not currently available. Disconnect/reconnect ESP32 and check Device Manager > Ports (COM & LPT)."
    }
} else {
    $candidates = @($devices | Where-Object {
        $_.Name -match '(?i)(CP210|CH340|CH341|CH910|FTDI|USB[- ]SERIAL|USB[- ]UART|USB to UART|ESP32)'
    })
    if ($candidates.Count -eq 0 -and $devices.Count -eq 1) {
        # One system serial port, but not verified to be ESP32: require explicit choice.
        $candidates = @()
    }
    if ($candidates.Count -ne 1) {
        Write-Host 'Available Windows COM devices:' -ForegroundColor Yellow
        $devices | Select-Object Name, Status | Format-Table -AutoSize
        throw 'No unique ESP32 USB serial adapter was detected. Specify the real port with -Port COMx; do not guess.'
    }
    if ($candidates[0].Name -notmatch '\((COM\d+)\)') {
        throw 'Could not parse the ESP32 COM port.'
    }
    $Port = $matches[1]
}
Write-Host "Using $Port for $Environment" -ForegroundColor Green
Write-Host 'IMPORTANT: Close every serial monitor before upload. Ctrl+C closes a running PlatformIO monitor.' -ForegroundColor Yellow
& pio run -e $Environment -t upload --upload-port $Port
if ($LASTEXITCODE -ne 0) {
    throw "Upload failed on $Port. If Access is denied, close Serial Monitors and other serial programs; if still locked, reconnect ESP32 or restart Windows. A ZIP cannot unlock a Windows COM port."
}
Write-Host "Upload succeeded. To monitor, run: pio device monitor -p $Port -b 115200" -ForegroundColor Green
