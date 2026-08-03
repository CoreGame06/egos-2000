import urllib.request
url = 'https://raw.githubusercontent.com/qemu/qemu/master/hw/display/vga.c'
content = urllib.request.urlopen(url).read().decode('utf-8')
for i, line in enumerate(content.split('\n')):
    if 'vga_mmio_write' in line or 'vga_mmio_ops' in line or 'vbe_mmio' in line:
        print('\n'.join(content.split('\n')[i:i+30]))
        break
