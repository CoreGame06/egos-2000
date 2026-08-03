import urllib.request
url = 'https://raw.githubusercontent.com/qemu/qemu/master/hw/display/vga.c'
content = urllib.request.urlopen(url).read().decode('utf-8')
in_vbe_write = False
for line in content.split('\n'):
    if 'vbe_ioport_write_data' in line:
        in_vbe_write = True
    if in_vbe_write:
        print(line)
        if '^}$' in line or line == '}':
            break
