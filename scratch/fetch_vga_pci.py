import urllib.request
url = 'https://raw.githubusercontent.com/qemu/qemu/master/hw/display/vga-pci.c'
content = urllib.request.urlopen(url).read().decode('utf-8')
for line in content.split('\n'):
    if 'memory_region_init_io' in line or 'pci_register_bar' in line:
        print(line.strip())
