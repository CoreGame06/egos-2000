import urllib.request
import time
url = 'https://raw.githubusercontent.com/qemu/qemu/master/hw/display/vga.c'
for _ in range(3):
    try:
        content = urllib.request.urlopen(url, timeout=10).read().decode('utf-8')
        for i, line in enumerate(content.split('\n')):
            if 'vga_mmio_write' in line:
                print('\n'.join(content.split('\n')[i:i+20]))
                break
        break
    except Exception as e:
        time.sleep(1)
