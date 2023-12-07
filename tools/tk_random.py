"""
Simple demo of writing pixels into a tkinter.PhotoImage
"""

IMG_W = 96
IMG_H = 96
SCALE = 5
WIN_W = IMG_W * SCALE
WIN_H = IMG_H * SCALE

import tkinter, random
class App:
    def __init__(self, t):
        self.i = tkinter.PhotoImage(width=WIN_W,height=WIN_H)
        for row in range(0, IMG_H):
            for col in range(0, IMG_W):
                pixel = '#%02x%02x%02x' % (0x80, row, col)
                for x in range(col * SCALE, (col + 1) * SCALE):
                    for y in range(row * SCALE, (row + 1) * SCALE):
                        self.i.put(pixel, (x, y))
        c = tkinter.Canvas(t, width=WIN_W, height=WIN_H);
        c.pack()
        c.create_image(0, 0, image = self.i, anchor=tkinter.NW)

t = tkinter.Tk()
a = App(t)    
t.mainloop()

