# scripts/generate_compiledb.py
#
# Генерирует compile_commands.json после успешной сборки firmware.
# Основан на встроенной цели PlatformIO `compiledb`.

Import("env")  # noqa: F821  (SCons предоставляет это)

import os
import subprocess
import sys


def generate_compiledb(source, target, env):
    project_dir = env.subst("$PROJECT_DIR")
    pioenv = env.subst("$PIOENV")
    print(f"[compiledb] Генерация compile_commands.json для окружения '{pioenv}' ...")
    try:
        subprocess.run(
            [sys.executable, "-m", "platformio", "run",
             "-e", pioenv, "-t", "compiledb"],
            cwd=project_dir,
            check=False,
        )
    except Exception as exc:
        print(f"[compiledb] Не удалось сгенерировать compile_commands.json: {exc}")


# Хук на финальный ELF/бинарь, чтобы не запускать compiledb после каждой .o-сборки
env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", generate_compiledb)   # noqa: F821