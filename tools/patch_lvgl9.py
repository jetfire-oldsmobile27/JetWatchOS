#!/usr/bin/env python3
"""
patch_lvgl9.py — правит bin2lvgl-вывод (LVGL 8) под LVGL 9.
Запуск:  python tools/patch_lvgl9.py src/faces
"""
import re, sys, pathlib

FMT = {
    'LV_IMG_CF_TRUE_COLOR_ALPHA':       'LV_COLOR_FORMAT_RGB565A8',
    'LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED':'LV_COLOR_FORMAT_RGB565',
    'LV_IMG_CF_TRUE_COLOR':             'LV_COLOR_FORMAT_RGB565',
    'LV_IMG_CF_ALPHA_8BIT':             'LV_COLOR_FORMAT_A8',
    'LV_IMG_CF_ALPHA_4BIT':             'LV_COLOR_FORMAT_A4',
    'LV_IMG_CF_ALPHA_2BIT':             'LV_COLOR_FORMAT_A2',
    'LV_IMG_CF_ALPHA_1BIT':             'LV_COLOR_FORMAT_A1',
    'LV_IMG_CF_INDEXED_8BIT':           'LV_COLOR_FORMAT_I8',
    'LV_IMG_CF_INDEXED_4BIT':           'LV_COLOR_FORMAT_I4',
    'LV_IMG_CF_INDEXED_2BIT':           'LV_COLOR_FORMAT_I2',
    'LV_IMG_CF_INDEXED_1BIT':           'LV_COLOR_FORMAT_I1',
    'LV_IMG_CF_RAW_ALPHA':              'LV_COLOR_FORMAT_RAW_ALPHA',
    'LV_IMG_CF_RAW_CHROMA_KEYED':       'LV_COLOR_FORMAT_RAW',
    'LV_IMG_CF_RAW':                    'LV_COLOR_FORMAT_RAW',
    'LV_IMG_CF_UNKNOWN':                'LV_COLOR_FORMAT_UNKNOWN',
}

def patch(p: pathlib.Path) -> bool:
    s = p.read_text(encoding='utf-8', errors='ignore')
    o = s

    # 1. Тип и функции
    s = s.replace('lv_img_dsc_t', 'lv_image_dsc_t')
    s = re.sub(r'\blv_img_create\b',   'lv_image_create',   s)
    s = re.sub(r'\blv_img_set_src\b',  'lv_image_set_src',  s)
    s = re.sub(r'\blv_img_get_src\b',  'lv_image_get_src',  s)

    # 2. Константы цветовых форматов
    for k, v in FMT.items():
        s = re.sub(r'\b' + re.escape(k) + r'\b', v, s)

    # 3. Старый compile-time чек (на случай, если build_flags не заданы)
    s = re.sub(
        r'#if\s+LV_COLOR_16_SWAP\s*!=\s*1\s*\n'
        r'\s*#error\s+"[^"]*"\s*\n'
        r'#endif',
        '/* LV_COLOR_16_SWAP check removed for LVGL 9 */',
        s
    )

    # 4. Убрать .header.always_zero — в v9 поля нет
    s = re.sub(r'\s*\.header\.always_zero\s*=\s*\d+\s*,', '', s)

    # 5. Проставить .header.magic — в v9 обязательное поле
    if '.header.magic' not in s:
        def repl(m):
            return m.group(0) + '\n    .header.magic = LV_IMAGE_HEADER_MAGIC,'
        # вставляем сразу после .header.cf = ...
        s = re.sub(r'\.header\.cf\s*=\s*[^,\n]+,', repl, s, count=0)

    if s != o:
        p.write_text(s, encoding='utf-8')
        return True
    return False


def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else '.')
    n = 0
    for ext in ('*.c', '*.h'):
        for p in root.rglob(ext):
            if patch(p):
                print(f'  patched {p}')
                n += 1
    print(f'\nDone. {n} files patched.')


if __name__ == '__main__':
    main()