# Replaces only src/main.c in a previous project, preserving private credentials.
param([string]$Target = 'C:\ESP32Projects\SmartFanRangeFix')
$ErrorActionPreference = 'Stop'
$source = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) 'src\main.c'
$dest = Join-Path $Target 'src\main.c'
if (!(Test-Path -LiteralPath $source)) { throw "Source missing: $source" }
if (!(Test-Path -LiteralPath $dest)) { throw "Target missing: $dest" }
$backup = $dest + '.backup_' + (Get-Date -Format 'yyyyMMdd_HHmmss')
Copy-Item -LiteralPath $dest -Destination $backup
Copy-Item -LiteralPath $source -Destination $dest -Force
Write-Host "Fixed main.c, original backed up at: $backup" -ForegroundColor Green
Write-Host "Now open $Target and run: pio run -e sinric_lab" -ForegroundColor Cyan
