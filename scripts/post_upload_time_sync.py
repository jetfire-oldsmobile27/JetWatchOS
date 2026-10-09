"""
PlatformIO post-upload hook.

Синхронизирует RTC часов с локальным временем хоста сразу после прошивки.
Регистрируется в platformio.ini:
    extra_scripts = post:scripts/post_upload_time_sync.py

Требует pyserial (в окружении PlatformIO есть по умолчанию).
"""
Import("env")   # noqa: F821 — символ даёт PlatformIO

import time
import datetime

BOOT_MARKER = b"===== BOOT REPORT ====="
ACK_MARKER  = b"[TIME]"
BAUD        = 115200


def _resolve_port(env):
    for key in ("UPLOAD_PORT", "MONITOR_PORT", "upload_port"):
        p = env.get(key)
        if isinstance(p, str) and p:
            return p
    return None


def _sync(env):
    try:
        import serial
    except ImportError:
        print("[time-sync] pyserial не установлен — пропуск. "
              "pip install pyserial")
        return

    port = _resolve_port(env)
    if not port:
        print("[time-sync] UPLOAD_PORT не определён — пропуск")
        return

    print(f"[time-sync] port={port}, открываю...")

    try:
        ser = serial.Serial(port, BAUD, timeout=0.2)
    except Exception as e:
        print(f"[time-sync] не могу открыть {port}: {e}")
        return

    try:
        # На S3 с native USB снимаем DTR/RTS, чтобы случайно не ресетнуть
        try:
            ser.dtr = False
            ser.rts = False
        except Exception:
            pass

        # Ждём boot-маркер из crash_init(), максимум 10 с.
        # Если пропустили — не страшно: loop() уже крутится.
        t0 = time.time()
        tail = b""
        saw_boot = False
        while time.time() - t0 < 10.0:
            chunk = ser.read(256)
            if not chunk:
                continue
            tail = (tail + chunk)[-512:]
            if BOOT_MARKER in tail:
                saw_boot = True
                break
        if not saw_boot:
            print("[time-sync] boot-маркер не пойман — шлю всё равно")

        time.sleep(0.5)   # дать loop() подняться

        now  = datetime.datetime.now()
        line = "TS %d %d %d %d %d %d\n" % (
            now.year, now.month, now.day,
            now.hour, now.minute, now.second,
        )
        ser.write(line.encode("ascii"))
        ser.flush()
        print(f"[time-sync] отправлено локальное "
              f"{now:%Y-%m-%d %H:%M:%S}")

        # Ждём ACK "[TIME] RTC set: ..."
        t0 = time.time()
        while time.time() - t0 < 3.0:
            chunk = ser.readline()
            if not chunk:
                continue
            txt = chunk.decode("utf-8", "replace").rstrip()
            if txt:
                print(f"[time-sync] < {txt}")
            if ACK_MARKER in chunk:
                print("[time-sync] OK")
                break
        else:
            print("[time-sync] ACK не получен (таймаут)")
    finally:
        try:
            ser.close()
        except Exception:
            pass


def _post_upload(source, target, env):
    # upload завершился — чипу нужно ~1.5 с на ресет и загрузку
    time.sleep(1.5)
    _sync(env)


env.AddPostAction("upload", _post_upload)   # noqa: F821