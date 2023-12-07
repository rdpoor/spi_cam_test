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
        y_uv = load_yuv(FILENAME)
        row = 0
        col = 0

        # read two bytes at a time to get y and uv
        for y, uv in zip(y_uv[::2], y_uv[1::2]):
            r, g, b = yuv_to_rgb_pixel(y, uv)
            pixel = '#%02x%02x%02x' % (r, g, b)
            self.i.put(pixel, (col, row))
            col = col + 1
            if (col == IMG_W):
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

def yuv_to_rgb_pixel(y, uv):
    """
    y is a byte containing luminance.
    uv contains u in the high nibble, v in the low nibble.
    convert to 8 bit r, g, b values and return as a tuple

    This version cheats: It uses only luminance and outputs
    a grayscale version...
    """
    # Y = y
    # U = (uv >> 4) * 16
    # V = (uv & 0x0f) * 16

    # R = Y + 1.4075 * (V - 128)
    # G = Y - 0.3455 * (U - 128) - (0.7169 * (V - 128))
    # B = Y + 1.7790 * (U - 128)
    # return (clamp(R), clamp(G), clamp(B))
    return (y, y, y)

def clamp(val):
    return int(min(255, max(0, val)))


t = tkinter.Tk()
a = App(t)
t.mainloop()

# IMG_W = 96
# IMG_H = 96
# WIN_SCALE = 1
# WIN_W = IMG_W * WIN_SCALE
# WIN_H = IMG_H * WIN_SCALE

# # window = tk.Tk()
# # canvas = tk.Canvas(window, width = WIN_W, height = WIN_H, bg="#008000")
# # canvas.pack()
# # image = tk.PhotoImage(width = WIN_W, height = WIN_H)
# # canvas.create_image((0,0), image=image, state = "normal", anchor = tk.NW)
# # tk.mainloop()

# # root = Tk()
# # label = Label(root)
# # label.pack()
# # img = PhotoImage(width=300,height=300)
# # data = ("{red red red blue blue blue }")
# # img.put(data, to=(20,20,280,280))
# # label.config(image=img)
# # root.mainloop()

# def convert_y_uv_to_rgb(y_uv):
#     """
#     y_uv is an array of numbers.  Each even number element is the 8-bit Y
#     component, each odd numbered element is U (upper four bits) and V (lower
#     four bits).  Convert each pair of numbers to an RGB string of the form
#     #rrggbb
#     """
#     rgb = []

#     # pull two numbers from y_uv list at once...
#     for y, uv in zip(y_uv[::2], y_uv[1::2]):
#         r, g, b = yuv_to_rgb_pixel(y, uv)
#         pixel = '#%02x%02x%02x' % (r, g, b)
#         rgb = rgb + [pixel]
#     return rgb

# # testing...
# # line = "11 89 34 82 38 88 1e 8e 29 82 1e 8d 13 8d 14 86 0d 7f 14 87 18 7e 27 7f"
# # print(line)
# # y_uv = convert_hex_line(line)
# # print(y_uv)
# # rgb = convert_y_uv_to_rgb(y_uv)
# # print(rgb)

# y_uv = load_yuv()

# window = tk.Tk()
# canvas = tk.Canvas(window, width = WIN_W, height = WIN_H, bg="#001000")
# canvas.pack()
# image = tk.PhotoImage(width = WIN_W, height = WIN_H)

# for y, uv in zip(y_uv[::2], y_uv[1::2]):
#     r, g, b = yuv_to_rgb_pixel(y, uv)
#     pixel = '#%02x%02x%02x' % (r, g, b)
#     image.put(pixel)

# canvas.create_image((0,0), image=image, state = "normal", anchor = tk.NW)
# tk.mainloop()
