#!/usr/bin/env bash
# decode-capture.sh <capture.pcapng> [--dev N]
#
# Extracts ROLI MIDI from a USBPcap/Wireshark capture and decodes it with
# seadecode. Auto-detects the transport:
#   * BLE  — if the capture has Bluetooth ATT frames (Dashboard connected over
#            Bluetooth). Writes (0x52 = host->device) and notifications
#            (0x1b = device->host) are decoded separately.
#   * USB  — otherwise, via USB-MIDI capdata (OUT and IN separated).
#
# The ROLI SysEx protocol is identical over both transports.
set -euo pipefail

TS="/c/Program Files/Wireshark/tshark.exe"
SEADECODE="./build/Debug/seadecode.exe"
CAP="${1:?usage: decode-capture.sh <capture.pcapng> [--dev N]}"

DEVFILT=""
if [ "${2:-}" = "--dev" ] && [ -n "${3:-}" ]; then
    DEVFILT=" && usb.device_address==${3}"
fi

att=$("$TS" -r "$CAP" -Y "btatt.value" 2>/dev/null | wc -l)

if [ "$att" -gt 0 ]; then
    echo "### Transport: BLE (Bluetooth ATT)  —  $att MIDI frames"
    for op in "0x52:HOST->DEVICE (Dashboard writes)" "0x1b:DEVICE->HOST (notifications)"; do
        code="${op%%:*}"; label="${op##*:}"
        echo "================= $label ================="
        "$TS" -r "$CAP" -Y "btatt.opcode==$code" -T fields -e btatt.value 2>/dev/null \
            | "$SEADECODE" --blemidi -
        echo
    done
else
    echo "### Transport: USB (USB-MIDI)"
    for dir in OUT IN; do
        if [ "$dir" = OUT ]; then DF="usb.endpoint_address.direction==0"; else DF="usb.endpoint_address.direction==1"; fi
        echo "================= $dir (host->device if OUT) ================="
        "$TS" -r "$CAP" -Y "usb.capdata && ${DF}${DEVFILT}" -T fields -e usb.capdata 2>/dev/null \
            | tr -d ':\n ' | "$SEADECODE" --usbmidi -
        echo
    done
fi
