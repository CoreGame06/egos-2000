#define REGW(base, offset) (*(volatile unsigned int*)((base) + (offset)))
#define VGA_PCI_ECAM 0x30010000UL
#define VGA_MMIO_BASE 0x40001000UL
#define VIDEO_FRAME_BASE 0x41000000UL

void boot() {
    REGW(VGA_PCI_ECAM, 0x4) = 0;
    REGW(VGA_PCI_ECAM, 0x10) = VIDEO_FRAME_BASE;
    REGW(VGA_PCI_ECAM, 0x18) = VGA_MMIO_BASE;
    REGW(VGA_PCI_ECAM, 0x4) = 7;
    
    volatile unsigned short* vbe = (unsigned short*)(VGA_MMIO_BASE + 0x500);
    vbe[4] = 0;
    vbe[1] = 800;
    vbe[2] = 600;
    vbe[3] = 32;
    vbe[5] = 0; // BANK
    vbe[6] = 800;
    vbe[7] = 600;
    vbe[8] = 0; // X_OFFSET
    vbe[9] = 0; // Y_OFFSET
    vbe[4] = 0x41; // ENABLE | LFB
    while(1);
}
