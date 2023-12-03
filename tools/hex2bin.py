def hex2bin():
    with open('cam_image.jpeg', 'wb') as fo:
        with open('cam_image.txt', 'r') as fi:
            while True:
                line = fi.readline();
                if not line:
                    break
                convert_line(line, fo)
    print('done');

def convert_line(hex_line, outfile):
    hex_values = hex_line.split()
    for hex_value in hex_values:
        try:
            binary_value = bytes.fromhex(hex_value)
            outfile.write(binary_value)
        except ValueError:
            print(f"Invalid hex value: {hex_value}")

hex2bin()
