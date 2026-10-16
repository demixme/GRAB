# capture-seaprobe.ps1 — decisive test: can USBPcap capture the Seaboard's USB-MIDI?
#
# Runs our own seaprobe (which opens the USB MIDI port and sends KNOWN ROLI SysEx:
# requestTopology, factory sync, pings) while capturing. If those bytes show up in
# the capture, USBPcap works for this device; if not, it can't see this USB-MIDI.
#
# RUN ELEVATED, with ROLI Dashboard CLOSED (so seaprobe can open the USB port):
#   cd C:\Users\skhoc\source\repos\ROLI
#   powershell -ExecutionPolicy Bypass -File tools\capture-seaprobe.ps1

param([string]$OutDir = "captures")

$ErrorActionPreference = "Stop"
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
           ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) { Write-Host "ERROR: run this in an Administrator PowerShell." -ForegroundColor Red; exit 1 }

$tshark   = "C:\Program Files\Wireshark\tshark.exe"
$seaprobe = "build\seaprobe_artefacts\Debug\seaprobe.exe"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$f1 = Join-Path $OutDir "seaprobe-cap-$stamp-hub1.pcapng"
$f2 = Join-Path $OutDir "seaprobe-cap-$stamp-hub2.pcapng"

Write-Host "Arming capture (16 s) on both USB hubs..." -ForegroundColor Cyan
$p1 = Start-Process -FilePath $tshark -ArgumentList @("-i","USBPcap1","-a","duration:16","-w",$f1) -PassThru -NoNewWindow
$p2 = Start-Process -FilePath $tshark -ArgumentList @("-i","USBPcap2","-a","duration:16","-w",$f2) -PassThru -NoNewWindow
Start-Sleep -Seconds 3

Write-Host "Running seaprobe (opens USB port, sends known ROLI SysEx)..." -ForegroundColor Yellow
& $seaprobe blocks 8 | Out-Null
Write-Host "seaprobe done; waiting for capture to finish..." -ForegroundColor Yellow

$p1.WaitForExit(); $p2.WaitForExit()

function Count($f) { try { (& $tshark -r $f 2>$null | Measure-Object -Line).Lines } catch { 0 } }
Write-Host ("Done. hub1 = {0} packets, hub2 = {1} packets" -f (Count $f1), (Count $f2)) -ForegroundColor Green
Write-Host "Files:"
Write-Host "  $((Resolve-Path $f1).Path)"
Write-Host "  $((Resolve-Path $f2).Path)"
