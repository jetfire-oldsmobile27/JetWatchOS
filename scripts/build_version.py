Import("env")

import os
from pathlib import Path

MAJOR_MINOR = "1.0"
PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
VERSION_FILE = PROJECT_DIR / "version.txt"


def _read_build() -> int:
    try:
        return int(VERSION_FILE.read_text(encoding="utf-8").strip())
    except (FileNotFoundError, ValueError):
        return 0


build_num = _read_build() + 1
VERSION_FILE.write_text(f"{build_num}\n", encoding="utf-8")

full_version = f"{MAJOR_MINOR}.{build_num}"
print(f"[build_version] firmware version = {full_version}")

# Прокидываем строку через -D, чтобы в C++ было:
#   #define FIRMWARE_VERSION_STR "1.0.42"
#   #define FIRMWARE_BUILD       42
env.Append(CPPDEFINES=[
    ("FIRMWARE_VERSION_STR", env.StringifyMacro(full_version)),
    ("FIRMWARE_BUILD",       build_num),
])