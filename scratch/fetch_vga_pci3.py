import urllib.request
url = 'https://raw.githubusercontent.com/qemu/qemu/master/hw/display/vga-pci.c'
content = urllib.request.urlopen(url).read().decode('utf-8')
for i, line in enumerate(content.split('\n')):
    if 'pci_vga_bochs_write' in line:
        print('\n'.join(content.split('\n')[i:i+15]))
        break
