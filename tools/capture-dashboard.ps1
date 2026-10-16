# capture-dashboard.ps1 — capture ROLI Dashboard <-> Seaboard USB traffic.
#
# USBPcap needs Administrator, so RUN THIS IN AN ELEVATED PowerShell:
#   Right-click Windows Terminal / PowerShell -> "Run as administrator", then:
#     cd C:\Users\skhoc\source\repos\ROLI
#     powershell -ExecutionPolicy Bypass -File tools\capture-dashboard.ps1
#
# It captures BOTH USB root hubs (USBPcap1 + USBPcap2) for -Seconds seconds into
# captures\. The Seaboard is on one of them; seadecode sorts that out later.
#
# IMPORTANT ordering (USBPcap only captures a MIDI pipe opened AFTER it starts):
#   1. CLOSE ROLI Dashboard first (so the Seaboard MIDI port is not open).
#   2. Run this script. It arms the capture, waits a few seconds, then says "GO".
#   3. When it says GO: OPEN ROLI Dashboard, let it connect to the Seaboard, then
#      change "Pressure Sensitivity" to a DISTINCTIVE value like 77 and Save/Edit.
#      Change it back if you like. Then let the capture finish.
# A distinctive value (77) is easy to spot in the decode.
# (If Dashboard was already open and you can't restart it, instead UNPLUG and
#  REPLUG the Seaboard USB cable after the GO, then let it reconnect.)

param(
    [int]$Seconds = 60,
    [string]$OutDir = "captures"
)

$ErrorActionPreference = "Stop"

# Elevation check
$isAdmin = ([Security.Principal.WindowsPrincipal] `
    [Security.Principal.WindowsIdentity]::GetCurrent()
    ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "ERROR: not elevated. Re-run this in an *Administrator* PowerShell." -ForegroundColor Red
    exit 1
}

$tshark = "C:\Program Files\Wireshark\tshark.exe"
if (-not (Test-Path $tshark)) { Write-Host "tshark not found at $tshark" -ForegroundColor Red; exit 1 }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$f1 = Join-Path $OutDir "dashboard-$stamp-hub1.pcapng"
$f2 = Join-Path $OutDir "dashboard-$stamp-hub2.pcapng"

Write-Host "Make sure ROLI Dashboard is CLOSED right now." -ForegroundColor Yellow
Write-Host "Arming capture on USBPcap1 + USBPcap2 ($Seconds s)..." -ForegroundColor Cyan

$p1 = Start-Process -FilePath $tshark -ArgumentList @("-i","USBPcap1","-a","duration:$Seconds","-w",$f1) -PassThru -NoNewWindow
$p2 = Start-Process -FilePath $tshark -ArgumentList @("-i","USBPcap2","-a","duration:$Seconds","-w",$f2) -PassThru -NoNewWindow

Start-Sleep -Seconds 4
Write-Host ""
Write-Host "==================== GO NOW ====================" -ForegroundColor Green
Write-Host " OPEN ROLI Dashboard, let it connect to the Seaboard," -ForegroundColor Green
Write-Host " then change Pressure Sensitivity to 77 and Save/Edit." -ForegroundColor Green
Write-Host "===============================================" -ForegroundColor Green

$p1.WaitForExit(); $p2.WaitForExit()

function Count($f) { try { (& $tshark -r $f 2>$null | Measure-Object -Line).Lines } catch { 0 } }
Write-Host ""
Write-Host ("Done. hub1 = {0} packets, hub2 = {1} packets" -f (Count $f1), (Count $f2)) -ForegroundColor Green
Write-Host "Files:"
Write-Host "  $((Resolve-Path $f1).Path)"
Write-Host "  $((Resolve-Path $f2).Path)"
Write-Host "Give these paths to Claude to decode."
