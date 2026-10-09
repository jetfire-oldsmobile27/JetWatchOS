#!/usr/bin/env python3
"""
fix_lvgl9_rgb565a8.py

Переводит массивы bin2lvgl (LVGL 8, LV_COLOR_16_SWAP=1, interleaved)
в формат LVGL 9 LV_COLOR_FORMAT_RGB565A8 (planar, native LE):

  LVGL8 : [Rhi,Rlo,A8] * N   (interleaved, byte-swapped)
  LVGL9 : [Rlo,Rhi] * N  +  [A8] * N   (planar, native)

Формат в lv_image_dsc_t не трогаем — там уже LV_COLOR_FORMAT_RGB565A8.
Трогаем только данные массивов.

Usage:  python fix_lvgl9_rgb565a8.py src/faces
"""
import re, sys, pathlib

ARR_RE = re.compile(
    r'(const\s+LV_ATTRIBUTE_MEM_ALIGN\s+uint8_t\s+(\w+)\[\]\s*=\s*\{)'
    r'(.*?)(\};)', re.S)

DSC_RE = re.compile(
    r'const\s+lv_image_dsc_t\s+(\w+)\s*=\s*\{(.*?)\};', re.S)

W_RE   = re.compile(r'\.header\.w\s*=\s*(\d+)')
H_RE   = re.compile(r'\.header\.h\s*=\s*(\d+)')
CF_RE  = re.compile(r'\.header\.cf\s*=\s*(LV_COLOR_FORMAT_\w+)')
DAT_RE = re.compile(r'\.data\s*=\s*(\w+)')
HEX_RE = re.compile(r'0x([0-9A-Fa-f]{1,2})')


def parse_bytes(body: str):
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    body = re.sub(r'//[^\n]*', '', body)
    return [int(x, 16) for x in HEX_RE.findall(body)]


def to_c_array(data, per_line=16):
    lines = []
    for i in range(0, len(data), per_line):
        chunk = data[i:i+per_line]
        lines.append('\t' + ','.join(f'0x{b:02X}' for b in chunk) + ',')
    return '\n' + '\n'.join(lines) + '\n'


def build_arr_index(src: str):
    """arr_name -> (w, h, cf).  Ищем в dsc, а не в массиве."""
    idx = {}
    for m in DSC_RE.finditer(src):
        body = m.group(2)
        wm, hm, cfm, dm = W_RE.search(body), H_RE.search(body), CF_RE.search(body), DAT_RE.search(body)
        if not (wm and hm and cfm and dm):
            continue
        idx[dm.group(1)] = (int(wm.group(1)), int(hm.group(1)), cfm.group(1))
    return idx


def convert_interleaved_to_planar(data, n_pix):
    if len(data) < n_pix * 3:
        return None
    rgb = []
    a8  = []
    for i in range(n_pix):
        b = i * 3
        # Rhi,Rlo -> native LE: low byte first
        rgb.append(data[b + 1])   # Rlo
        rgb.append(data[b + 0])   # Rhi
        a8.append(data[b + 2])    # A8
    return rgb + a8


def patch_file(p: pathlib.Path) -> bool:
    s = p.read_text(encoding='utf-8', errors='ignore')
    orig = s
    arr_idx = build_arr_index(s)
    if not arr_idx:
        return False

    def repl(m):
        arr_name, body = m.group(2), m.group(3)
        info = arr_idx.get(arr_name)
        if not info:
            return m.group(0)
        w, h, cf = info
        if cf != 'LV_COLOR_FORMAT_RGB565A8':
            return m.group(0)

        data = parse_bytes(body)
        out = convert_interleaved_to_planar(data, w * h)
        if out is None:
            print(f'   ! {arr_name}: data too short ({len(data)} < {w*h*3})')
            return m.group(0)
        return m.group(1) + to_c_array(out) + m.group(4)

    s = ARR_RE.sub(repl, s)
    if s != orig:
        p.write_text(s, encoding='utf-8')
        return True
    return False


def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else '.')
    n = 0
    for p in root.rglob('*.c'):
        if patch_file(p):
            print(f'  fixed {p}')
            n += 1
    print(f'Done. {n} file(s).')


if __name__ == '__main__':
    main()