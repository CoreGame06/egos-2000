import urllib.request
url = 'https://raw.githubusercontent.com/qemu/qemu/master/hw/display/vga-pci.c'
content = urllib.request.urlopen(url).read().decode('utf-8')
for i, line in enumerate(content.split('\n')):
    if 'memory_region_add_subregion' in line:
        print('\n'.join(content.split('\n')[i-3:i+2]))
