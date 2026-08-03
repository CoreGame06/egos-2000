cd /home/shenmi/egos-2000
sed -i 's/REGW(VGA_PCI_ECAM, 0x4) = (1 << 1) | (1 << 2);/REGW(VGA_PCI_ECAM, 0x4) = 0;/' earth/boot.c
sed -i 's/REGW(VGA_PCI_ECAM, 0x18) = VGA_MMIO_BASE;/REGW(VGA_PCI_ECAM, 0x18) = VGA_MMIO_BASE;\n            REGW(VGA_PCI_ECAM, 0x4) = (1 << 1) | (1 << 2);/' earth/boot.c
make qemu
