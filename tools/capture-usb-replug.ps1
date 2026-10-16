# capture-usb-replug.ps1 — capture the Seaboard's USB-MIDI by replugging during capture.
#
# USBPcap cannot capture a USB pipe that was already open when the capture started,
# and the Windows MIDI service holds the Seaboard's USB-MIDI pipe open continuously.
# The reliable fix is to RE-OPEN the pipe after the capture is running, by
# physically unplugging and replugging the Seaboard's USB cable.
#
# RUN ELEVATED (Administrator PowerShell):
#   cd C:\Users\skhoc\source\repos\ROLI
#   powershell -ExecutionPolicy Bypass -File tools\capture-usb-replug.ps1
#
# Keep ROLI Dashboard OPEN and the Seaboard connected (Bluetooth OFF so it uses USB).

param(
    [int]$Seconds = 50,
    [string]$OutDir = "captures"
)

$ErrorActionPreference = "Stop"
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
           ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) { Write-Host "ERROR: run this in an Administrator PowerShell." -ForegroundColor Red; exit 1 }

$tshark = "C:\Program Files\Wireshark\tshark.exe"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$f1 = Join-Path $OutDir "usb-replug-$stamp-hub1.pcapng"
$f2 = Join-Path $OutDir "usb-replug-$stamp-hub2.pcapng"

Write-Host "Arming capture ($Seconds s) on both USB root hubs..." -ForegroundColor Cyan
$p1 = Start-Process -FilePath $tshark -ArgumentList @("-i","USBPcap1","-a","duration:$Seconds","-w",$f1) -PassThru -NoNewWindow
$p2 = Start-Process -FilePath $tshark -ArgumentList @("-i","USBPcap2","-a","duration:$Seconds","-w",$f2) -PassThru -NoNewWindow
Start-Sleep -Seconds 4

Write-Host ""
Write-Host "==================== DO THIS NOW ====================" -ForegroundColor Green
Write-Host " 1. UNPLUG the Seaboard's USB cable."                 -ForegroundColor Green
Write-Host " 2. Wait ~3 seconds, then PLUG it back in."           -ForegroundColor Green
Write-Host " 3. Wait for Dashboard to reconnect to the Seaboard." -ForegroundColor Green
Write-Host " 4. Play a few keys, then change Pressure Sensitivity" -ForegroundColor Green
Write-Host "    to 77 and back to 0 in Dashboard."                 -ForegroundColor Green
Write-Host "====================================================" -ForegroundColor Green

$p1.WaitForExit(); $p2.WaitForExit()

function Count($f) { try { (& $tshark -r $f 2>$null | Measure-Object -Line).Lines } catch { 0 } }
Write-Host ""
Write-Host ("Done. hub1 = {0} packets, hub2 = {1} packets" -f (Count $f1), (Count $f2)) -ForegroundColor Green
Write-Host "Files:"
Write-Host "  $((Resolve-Path $f1).Path)"
Write-Host "  $((Resolve-Path $f2).Path)"
