import tkinter

IMG_W = 96
IMG_H = 96
FILENAME = "yuv_test.txt"
SCALE = 4
CAN_W = IMG_W * SCALE
CAN_H = IMG_H * SCALE

class App:
    def __init__(self, t):
        self.i = tkinter.PhotoImage(width=IMG_W,height=IMG_H)
        yuv = load_yuv(FILENAME)
        row = 0
        col = 0

        # read four bytes at a time to get (y0 u y1 v)
        for y0, u, y1, v in zip(yuv[::4], yuv[1::4], yuv[2::4], yuv[3::4]):
            # expand (y0, u, v) and (y1, u, v) into RGB two pixels
            r, g, b = yuv_to_rgb_pixel(y0, u, v)
            pixel = '#%02x%02x%02x' % (r, g, b)
            self.i.put(pixel, (col, row))
            r, g, b = yuv_to_rgb_pixel(y1, u, v)
            pixel = '#%02x%02x%02x' % (r, g, b)
            self.i.put(pixel, (col+1, row))
            col = col + 2
            if (col >= IMG_W):
                col = 0
                row = row + 1
        self.i = self.i.zoom(SCALE)
        c = tkinter.Canvas(t, width=CAN_W, height=CAN_H);
        c.pack()
        c.create_image(0, 0, image = self.i, anchor=tkinter.NW)

def load_yuv(filename):
    y_uv = []
    with open(filename, 'r') as fi:
        while True:
            line = fi.readline()
            if not line:
                break
            y_uv += convert_hex_line(line)
    return y_uv

def convert_hex_line(line):
    """
    Given a line of the form "01 ff 80 20" etc, return an array of
    integers [1, 255, 128, 32], etc
    """
    hex_values = line.split()
    return [int(x, 16) for x in hex_values]

def yuv_to_rgb_pixel(y, u, v):
    """
    convert to 8 bit r, g, b values and return as a tuple
    """
    R = y + 1.4075 * (v - 128)
    G = y - 0.3455 * (u - 128) - (0.7169 * (v - 128))
    B = y + 1.7790 * (u - 128)
    return (clamp(R), clamp(G), clamp(B))

def clamp(val):
    return int(min(255, max(0, val)))

t = tkinter.Tk()
a = App(t)
t.mainloop()
