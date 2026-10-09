#!/usr/bin/env python3
"""
patch_lvgl9_colors.py

Исправляет байт-порядок данных от bin2lvgl (LVGL 8, LV_COLOR_16_SWAP=1)
под LVGL 9:
  * LV_COLOR_FORMAT_RGB565   -> LV_COLOR_FORMAT_RGB565_SWAPPED
  * LV_COLOR_FORMAT_RGB565A8 -> байт-свап RGB-части массива на месте

Usage:
    python tools/patch_lvgl9_colors.py src/faces
"""
import re, pathlib, sys

ARR_RE = re.compile(
    r'(const\s+LV_ATTRIBUTE_MEM_ALIGN\s+uint8_t\s+(\w+)\[\]\s*=\s*\{)'
    r'(.*?)(\};)', re.S)

W_RE = re.compile(r'\.header\.w\s*=\s*(\d+)')
H_RE = re.compile(r'\.header\.h\s*=\s*(\d+)')
CF_RE = re.compile(r'\.header\.cf\s*=\s*(LV_COLOR_FORMAT_\w+)')


def parse_bytes(body: str):
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    body = re.sub(r'//[^\n]*', '', body)
    return [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{1,2})', body)]


def to_c_array(data, per_line=16):
    out = []
    for i in range(0, len(data), per_line):
        chunk = data[i:i + per_line]
        out.append('\t' + ','.join(f'0x{b:02X}' for b in chunk) + ',')
    return '\n' + '\n'.join(out) + '\n'


def get_dims(src, arr_name):
    ref = src.find(f'.data = {arr_name}')   # ← ref указывает на ".data = arr"
    end = src.find('};', ref)
    blk = src[ref:end]                       # ← blk = только ".data = arr"
    wm, hm = W_RE.search(blk), H_RE.search(blk)   # ← ничего не находит!


def patch_file(p: pathlib.Path) -> bool:
    s = p.read_text(encoding='utf-8', errors='ignore')
    orig = s

    # 1. RGB565A8 -> swap RGB565 part in-place
    for m in ARR_RE.finditer(s):
        arr_name = m.group(2)
        dims = get_dims(s, arr_name)
        if not dims:
            continue
        w, h, blk = dims
        cfm = CF_RE.search(blk)
        if not cfm or cfm.group(1) != 'LV_COLOR_FORMAT_RGB565A8':
            continue

        data = parse_bytes(m.group(3))
        n_pix = w * h
        if len(data) < n_pix * 3:
            continue

        for i in range(n_pix):
            a = i * 2
            data[a], data[a + 1] = data[a + 1], data[a]

        s = s.replace(m.group(0),
                      m.group(1) + to_c_array(data) + m.group(4),
                      1)

    # 2. Plain RGB565 -> RGB565_SWAPPED (только не RGB565A8!)
    # \b после 565 не матчится между '5' и 'A', так что A8 не заденет
    s = re.sub(r'\bLV_COLOR_FORMAT_RGB565\b', 'LV_COLOR_FORMAT_RGB565_SWAPPED', s)

    if s != orig:
        p.write_text(s, encoding='utf-8')
        return True
    return False


def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else '.')
    n = 0
    for p in root.rglob('*.c'):
        if patch_file(p):
            print(f'  patched {p}')
            n += 1
    print(f'Done. {n} file(s).')


if __name__ == '__main__':
    main()