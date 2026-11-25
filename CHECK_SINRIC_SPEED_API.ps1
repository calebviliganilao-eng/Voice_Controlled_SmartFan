# Run AFTER `pio run -e sinric_lab` downloads managed components.
# Prints the fan API actually installed on this PC. Never displays credentials.
$ErrorActionPreference = 'Stop'
Push-Location (Split-Path -Parent $MyInvocation.MyCommand.Path)
try {
  $files = Get-ChildItem -Path '.\managed_components' -Recurse -Filter 'sinricpro_fan.h' -ErrorAction SilentlyContinue
  if (-not $files) {
    Write-Host 'No managed SinricPro header found. Run pio run -e sinric_lab first.' -ForegroundColor Yellow
  } else {
    foreach ($file in $files) {
      Write-Host "Inspecting $($file.FullName)" -ForegroundColor Cyan
      Select-String -LiteralPath $file.FullName -Pattern 'on_.*range|on_.*power_level|send_.*range|send_.*power_level' |
         ForEach-Object { $_.Line.Trim() }
    }
  }
} finally { Pop-Location }
