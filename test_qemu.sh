sed -i 's/REGW(VGA_PCI_ECAM, 0x4) = (1 << 1) | (1 << 2);/REGW(VGA_PCI_ECAM, 0x4) = 0;/' earth/boot.c
sed -i 's/REGW(VGA_PCI_ECAM, 0x18) = VGA_MMIO_BASE;/REGW(VGA_PCI_ECAM, 0x18) = VGA_MMIO_BASE;\n            REGW(VGA_PCI_ECAM, 0x4) = (1 << 1) | (1 << 2);/' earth/boot.c
sed -i 's/vbe\[4\] = 0x00;/vbe[4] = 0x00; vbe[0] = 0xB0C5;/' earth/boot.c
make qemu &
PID=$!
sleep 5
kill $PID
