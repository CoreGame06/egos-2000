#!/bin/bash
export PATH=$PATH:~/xpack-riscv-none-elf-gcc-14.2.0-3/bin:~/xpack-qemu-riscv-8.2.2-1/bin
make install || exit 1
(printf '0\r'; sleep 7
 printf 'coresinfo\r'; sleep 2
 printf 'loop 3 &\r'; sleep 1; printf 'loop 3 &\r'; sleep 3
 printf 'cat README\r'; sleep 3
 printf 'producer_consumer\r'; sleep 10
) | timeout 60 qemu-system-riscv32 \
    -M virt -smp 4 -m 8M -bios tools/egos.bin \
    -device VGA,addr=0x2 -serial stdio -display none \
    -drive if=pflash,format=raw,unit=1,file=tools/qemuROM.bin \
    -device sdhci-pci,addr=0x1 -device sd-card,drive=MMC \
    -drive if=none,file=tools/disk.img,format=raw,id=MMC

