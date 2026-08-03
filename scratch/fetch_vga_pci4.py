import urllib.request
url = 'https://raw.githubusercontent.com/qemu/qemu/master/hw/display/vga-pci.c'
content = urllib.request.urlopen(url).read().decode('utf-8')
for line in content.split('\n'):
    if 'PCI_VGA_BOCHS_OFFSET' in line:
        print(line)
