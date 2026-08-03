#define REGW(base, offset) (*(volatile unsigned int*)((base) + (offset)))
#define VGA_PCI_ECAM 0x30010000UL
#define VGA_MMIO_BASE 0x40001000UL
#define VIDEO_FRAME_BASE 0x41000000UL

void _start() {
    REGW(VGA_PCI_ECAM, 0x4) = 0;
    REGW(VGA_PCI_ECAM, 0x10) = VIDEO_FRAME_BASE;
    REGW(VGA_PCI_ECAM, 0x18) = VGA_MMIO_BASE;
    REGW(VGA_PCI_ECAM, 0x4) = 6;
    while(1);
}
