import argparse
import serial
import tkinter
import sys  # Import the sys module

IMG_W = 96
IMG_H = 96
FILENAME = "yuv_test.txt"
SCALE = 4
CAN_W = IMG_W * SCALE
CAN_H = IMG_H * SCALE

class App(tkinter.Frame):  # Inherit from tkinter.Frame
    def __init__(self, args, t):
        super().__init__(t)  # Initialize tkinter.Frame
        self._ser = serial.Serial(args.serial_port, args.baud, timeout=0.1)
        self._row = 0
        self._col = 0

        self._img = tkinter.PhotoImage(width=IMG_W, height=IMG_H)
        self._img = self._img.zoom(SCALE)  # Use self._img consistently
        c = tkinter.Canvas(t, width=CAN_W, height=CAN_H)
        c.pack()
        c.create_image(0, 0, image=self._img, anchor=tkinter.NW)

        self.read_loop()  # Start the read_loop

    def read_loop(self):
        while True:
            line = self._ser.read_until(size=1024)
            line = line.decode('utf-8')
            if len(line) == 0:
                break
            elif line.startswith("#"):
                self._row = 0
                self._col = 0
                self.update_status(line)
                break
            else:
                yuvs = self.convert_hex_line(line)
                for y, uv in zip(yuvs[::2], yuvs[1::2]):
                    r, g, b = self.yuv_to_rgb_pixel(y, uv)
                    pixel = '#%02x%02x%02x' % (r, g, b)
                    self._img.put(pixel, (self._col, self._row))
                    self._col = self._col + 1
                    if (self._col == IMG_W):
                        self._col = 0
                        self._row = self._row + 1
        # Play nice with tkinter's main loop
        self.after(1, self.read_loop)  # Use self.after to call read_loop

    def update_status(self, line):
        """
        Placeholder: this will go into a status field in the tkinter canvas
        """
        print(line)

    def convert_hex_line(self, line):
        """
        Given a line of the form "01 ff 80 20" etc, return an array of
        integers [1, 255, 128, 32], etc
        """
        hex_values = line.split()
        return [int(x, 16) for x in hex_values]

    def yuv_to_rgb_pixel(self, y, uv):
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

    def clamp(self, val):
        return int(min(255, max(0, val)))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="display captured images.")
    parser.add_argument('--baud', type=int, default=460800, help="Baud rate of serial port. Defaults to 460800.")
    parser.add_argument('serial_port', help="Serial port to connect to, e.g. 'COM1' or '/dev/usb3'.")
    args = parser.parse_args()

    if not args.serial_port:
        print("Error: Must specify a serial port")
        sys.exit(1)

    t = tkinter.Tk()
    App(args, t)
    t.mainloop()
