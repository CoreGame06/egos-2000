import urllib.request
url = 'https://raw.githubusercontent.com/qemu/qemu/master/hw/display/vga.c'
content = urllib.request.urlopen(url).read().decode('utf-8')
in_fixup = False
for line in content.split('\n'):
    if 'void vbe_fixup_regs' in line or 'static void vbe_fixup_regs' in line:
        in_fixup = True
    if in_fixup:
        print(line)
        if '^}$' in line or line == '}':
            break
