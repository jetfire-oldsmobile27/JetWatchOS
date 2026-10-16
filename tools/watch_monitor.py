#!/usr/bin/env python3
"""
Auto-reconnect serial monitor for ESP32-S3 USB CDC.
Находит устройство Espressif (VID 0x303A) или любой COM-порт,
указанный в --port, и держит соединение, переподключаясь при каждом
reset/deep-sleep wake.
"""
import argparse
import sys
import time
import serial
import serial.tools.list_ports

ESPRESSIF_VID = 0x303A


def find_port(explicit: str | None) -> str | None:
    if explicit:
        return explicit
    for p in serial.tools.list_ports.comports():
        if p.vid == ESPRESSIF_VID:
            return p.device
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default=None, help="фиксировать COM-порт (иначе автопоиск по VID)")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    current = None
    ser: serial.Serial | None = None

    while True:
        port = find_port(args.port)
        if port is None:
            if ser:
                ser.close()
                ser = None
                current = None
                print("[mon] device gone, waiting...", file=sys.stderr, flush=True)
            time.sleep(0.3)
            continue

        if port != current:
            if ser:
                try: ser.close()
                except Exception: pass
                ser = None
            try:
                ser = serial.Serial(port, args.baud, timeout=0.05)
                current = port
                print(f"[mon] connected {port} @ {args.baud}", file=sys.stderr, flush=True)
            except (serial.SerialException, OSError) as e:
                print(f"[mon] open {port} failed: {e}", file=sys.stderr, flush=True)
                time.sleep(0.5)
                continue

        try:
            n = ser.in_waiting
            if n:
                data = ser.read(n)
                sys.stdout.buffer.write(data)
                sys.stdout.buffer.flush()
            else:
                # периодически проверяем, что порт ещё жив
                ser.read(1)
        except (serial.SerialException, OSError):
            print("[mon] port lost, reconnecting...", file=sys.stderr, flush=True)
            try: ser.close()
            except Exception: pass
            ser = None
            current = None
            time.sleep(0.3)

    return 0


if __name__ == "__main__":
    sys.exit(main())