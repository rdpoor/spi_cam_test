import os
"""
Convert a YUV 96x96x2 image to a raw 96x96 [r, g, b] file.
NOTE: not sure how to convert this to a .bmp file.
"""

PROJECT_DIR = "/Users/r/Projects/BrainChip"
YUV_FILENAME = "yuv_96x96.txt"
RAW_FILENAME = "bmp_96x96.raw"
YUV_PATH = os.path.join(PROJECT_DIR, YUV_FILENAME)
RAW_PATH = os.path.join(PROJECT_DIR, RAW_FILENAME)

def yuv_to_rgb_pixel(y, uv):
    """
    y is a byte containing luminance.
    uv contains u in the high nibble, v in the low nibble.
    convert to 8 bit r, g, b values and return as a tuple
    """
    Y = y
    U = (uv >> 4) * 16
    V = (uv & 0x0f) * 16

    R = Y + 1.4075 * (V - 128)
    G = Y - 0.3455 * (U - 128) - (0.7169 * (V - 128))
    B = Y + 1.7790 * (U - 128)
    return (R, G, B)


def yuv2raw():
    with open(RAW_PATH, 'wb') as fo:
        with open(YUV_PATH, 'r') as fi:
            while True:
                line = fi.readline();
                if not line:
                    break
                convert_line(line, fo)
    print('done');

def convert_line(hex_line, outfile):
    hex_values = hex_line.split()
    # pull two hex values at once
    for y, uv in zip(hex_values[::2], hex_values[1::2]):
        try:
            r, g, b = yuv_to_rgb_pixel(int(y, 16), int(uv, 16))
            # print(f'r={r}, g={g}, b={b}')
            outfile.write(bytes([clamp(r), clamp(g), clamp(b)]))
        except ValueError:
            print(f"Invalid hex value: {y}, {uv}")

def clamp(val):
    return int(min(255, max(0, val)))

yuv2raw()
