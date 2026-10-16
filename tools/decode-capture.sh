#!/usr/bin/env bash
# decode-capture.sh <capture.pcapng> [--dev N]
#
# Extracts USB-MIDI data from a USBPcap/Wireshark capture and decodes the ROLI
# SysEx with seadecode. OUT (host->device) and IN (device->host) are decoded
# separately so their byte streams are never interleaved during reassembly.
#
# Optionally pass --dev N to restrict to one USB device address (find it first
# with:  tshark -r cap.pcapng -T fields -e usb.device_address -e usb.idVendor
#        -Y 'usb.idVendor==0x2af4' | sort -u ).
set -euo pipefail

TS="/c/Program Files/Wireshark/tshark.exe"
SEADECODE="./build/Debug/seadecode.exe"
CAP="${1:?usage: decode-capture.sh <capture.pcapng> [--dev N]}"

DEVFILT=""
if [ "${2:-}" = "--dev" ] && [ -n "${3:-}" ]; then
    DEVFILT=" && usb.device_address==${3}"
fi

for dir in OUT IN; do
    if [ "$dir" = OUT ]; then DF="usb.endpoint_address.direction==0"; else DF="usb.endpoint_address.direction==1"; fi
    echo "================= $dir (host->device if OUT) ================="
    "$TS" -r "$CAP" -Y "usb.capdata && ${DF}${DEVFILT}" -T fields -e usb.capdata 2>/dev/null \
        | tr -d ':\n ' \
        | "$SEADECODE" --usbmidi -
    echo
done
