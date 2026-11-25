# Lists Windows serial ports; does NOT attempt to open or unlock them.
Set-Location -LiteralPath $PSScriptRoot
Write-Host 'PlatformIO serial adapters:' -ForegroundColor Cyan
& pio device list
Write-Host 'Windows COM devices:' -ForegroundColor Cyan
Get-CimInstance Win32_PnPEntity | Where-Object {
  $_.Name -and $_.Name -match '\(COM\d+\)'
} | Select-Object Name, Status, PNPDeviceID | Format-Table -Wrap
Write-Host 'To identify your ESP32: unplug the board, run this again, replug, and run this again.'
