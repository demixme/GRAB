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
# WHAT TO DO DURING THE CAPTURE (have ROLI Dashboard already open & connected
# to the Seaboard BEFORE you start):
#   1. Start this script.
#   2. In ROLI Dashboard, change ONE setting to a DISTINCTIVE value, e.g. set
#      "Pressure Sensitivity" to 77, and Save if there's a Save/Apply control.
#   3. Change it back if you like. Then let the capture finish.
# A distinctive value (77) is easy to spot in the decode.

param(
    [int]$Seconds = 40,
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

Write-Host "Capturing USBPcap1 -> $f1" -ForegroundColor Cyan
Write-Host "Capturing USBPcap2 -> $f2" -ForegroundColor Cyan
Write-Host ""
Write-Host ">>> NOW: in ROLI Dashboard change Pressure Sensitivity to 77 and Save. <<<" -ForegroundColor Yellow
Write-Host "    Capturing for $Seconds seconds..." -ForegroundColor Yellow

$p1 = Start-Process -FilePath $tshark -ArgumentList @("-i","USBPcap1","-a","duration:$Seconds","-w",$f1) -PassThru -NoNewWindow
$p2 = Start-Process -FilePath $tshark -ArgumentList @("-i","USBPcap2","-a","duration:$Seconds","-w",$f2) -PassThru -NoNewWindow
$p1.WaitForExit(); $p2.WaitForExit()

function Count($f) { try { (& $tshark -r $f 2>$null | Measure-Object -Line).Lines } catch { 0 } }
Write-Host ""
Write-Host ("Done. hub1 = {0} packets, hub2 = {1} packets" -f (Count $f1), (Count $f2)) -ForegroundColor Green
Write-Host "Files:"
Write-Host "  $((Resolve-Path $f1).Path)"
Write-Host "  $((Resolve-Path $f2).Path)"
Write-Host "Give these paths to Claude to decode."
