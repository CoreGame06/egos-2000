#define REGW(base, offset) (*(volatile unsigned int*)((base) + (offset)))
#define REGB(base, offset) (*(volatile unsigned char*)((base) + (offset)))
#define VGA_PCI_ECAM 0x30010000UL
#define VGA_MMIO_BASE 0x40001000UL
#define VIDEO_FRAME_BASE 0x41000000UL

void _start() {
    REGW(VGA_PCI_ECAM, 0x4) = 0;
    REGW(VGA_PCI_ECAM, 0x10) = VIDEO_FRAME_BASE;
    REGW(VGA_PCI_ECAM, 0x18) = VGA_MMIO_BASE;
    REGW(VGA_PCI_ECAM, 0x4) = 7;
    
    volatile unsigned short* vbe = (unsigned short*)(VGA_MMIO_BASE + 0x500);
    vbe[4] = 0;
    vbe[0] = 0xB0C5;
    vbe[1] = 800;
    vbe[2] = 600;
    vbe[3] = 32;
    vbe[5] = 0;
    vbe[6] = 800;
    vbe[7] = 600;
    vbe[8] = 0;
    vbe[9] = 0;
    vbe[4] = 0x41;
    
    // Unblank VGA
    volatile unsigned char* vga_io = (unsigned char*)(VGA_MMIO_BASE + 0x400);
    unsigned char stat = vga_io[0x1A]; // read 3DA
    vga_io[0x0] = 0x20; // write 3C0
    
    // fill white
    unsigned int* fb = (unsigned int*)VIDEO_FRAME_BASE;
    for (int i = 0; i < 800 * 600; i++) fb[i] = 0xFFFFFFFF;
    
    while(1);
}
