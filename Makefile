QEMU        = qemu-system-riscv32

ifeq ($(TOOLCHAIN), GNU)
# Use the official GNU toolchain.
# https://github.com/riscv-collab/riscv-gnu-toolchain
RISCV_CC    = riscv32-unknown-elf-gcc
OBJDUMP     = riscv32-unknown-elf-objdump
OBJCOPY     = riscv32-unknown-elf-objcopy
else
# Use the pre-compiled GNU toolchain binaries from xPack.
# https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases
RISCV_CC    = riscv-none-elf-gcc
OBJDUMP     = riscv-none-elf-objdump
OBJCOPY     = riscv-none-elf-objcopy
endif

# P0-P8：交叉编译器生成 RISC-V ELF，宿主 CC 生成在宿主执行的 tools/mkfs。
# release 保存 ELF；debug 保存同一 ELF 的反汇编清单，不是另一套优化配置。
DEBUG       = build/debug
RELEASE     = build/release

APPS_DEPS   = apps/*.* library/egos.h library/*/* Makefile
EGOS_DEPS   = earth/* grass/* library/egos.h library/*/* Makefile

# P6：0=mydisk（你的 FAT 式作业实现），1=课程提供的 treedisk；镜像与服务须使用同一格式。
FILESYS     = 0
LDFLAGS     = -nostdlib -lc -lgcc
INCLUDE     = -Ilibrary -Ilibrary/elf -Ilibrary/file -Ilibrary/libc -Ilibrary/syscall
# P0：rv32ima_zicsr 选择基础整数/乘除/原子/CSR 指令；ilp32 约定 int/long/指针为 32 位。
# -DKERNEL 只选择库中的直接终端分支，不会自动赋予 CPU 特权；运行权限由 mret/MPP 设置。
CFLAGS      = -march=rv32ima_zicsr -mabi=ilp32 -Wl,--gc-sections -ffunction-sections -fdata-sections -fdiagnostics-show-option
DEBUG_FLAGS = --source --all-headers --demangle --line-numbers --wide

SYSAPP_ELFS = $(patsubst %.c, $(RELEASE)/%.elf, $(notdir $(wildcard apps/system/*.c)))
USRAPP_ELFS = $(patsubst %.c, $(RELEASE)/user/%.elf, $(notdir $(wildcard apps/user/*.c)))

egos: $(USRAPP_ELFS) $(SYSAPP_ELFS) $(RELEASE)/egos.elf

$(RELEASE)/egos.elf: $(EGOS_DEPS)
	@printf "$(YELLOW)-------- Compile EGOS --------$(END)\n"
	$(RISCV_CC) $(CFLAGS) $(INCLUDE) -I grass -DKERNEL $(filter %.s, $(wildcard $^)) $(filter %.c, $(wildcard $^)) -Tlibrary/elf/egos.lds $(LDFLAGS) -o $@
	@$(OBJDUMP) $(DEBUG_FLAGS) $@ > $(DEBUG)/egos.lst

$(SYSAPP_ELFS): $(RELEASE)/%.elf : apps/system/%.c $(APPS_DEPS)
	@printf "Compile app $(CYAN)%s$(END) => %s\n" $(patsubst %.c, %, $(notdir $<)) $@
	@$(RISCV_CC) $(CFLAGS) $(INCLUDE) -DFILESYS=$(FILESYS) -DKERNEL -Iapps apps/app.s $(filter %.c, $(wildcard $^)) -Tlibrary/elf/app.lds $(LDFLAGS) -o $@
	@$(OBJDUMP) $(DEBUG_FLAGS) $@ > $(patsubst %.c, $(DEBUG)/%.lst, $(notdir $<))

$(USRAPP_ELFS): $(RELEASE)/user/%.elf : apps/user/%.c $(APPS_DEPS)
	@mkdir -p $(DEBUG) $(RELEASE) $(RELEASE)/user
	@printf "Compile app $(CYAN)%s$(END) => %s\n" $(patsubst %.c, %, $(notdir $<)) $@
	@$(RISCV_CC) $(CFLAGS) $(INCLUDE) -Iapps apps/app.s $(filter %.c, $(wildcard $^)) -Tlibrary/elf/app.lds $(LDFLAGS) -o $@
	@$(OBJDUMP) $(DEBUG_FLAGS) $@ > $(patsubst %.c, $(DEBUG)/%.lst, $(notdir $<))

# P0/P6：源码 -> ELF -> 内核裸二进制，再由宿主 mkfs 将系统服务/用户程序打包为镜像。
install: egos
	@printf "$(YELLOW)-------- Create the Disk & ROM Images --------$(END)\n"
	$(OBJCOPY) -O binary $(RELEASE)/egos.elf tools/egos.bin
	$(CC) tools/mkfs.c library/file/file$(FILESYS).c -DMKFS -DFILESYS=$(FILESYS) $(INCLUDE) -o tools/mkfs
	cd tools; rm -f disk.img qemuROM.bin; ./mkfs $(abspath $(USRAPP_ELFS))

# P8：四核须与 NCORES/启动计数配合；-bios 使用自有 egos 裸镜像。
QEMU_MACHINE = -M virt -smp 4 -m 8M -bios tools/egos.bin
QEMU_GRAPHIC = -device VGA,addr=0x2 -serial mon:stdio
QEMU_FLASH_1 = -drive if=pflash,format=raw,unit=1,file=tools/qemuROM.bin
QEMU_SD_CARD = -device sdhci-pci,addr=0x1 -device sd-card,drive=MMC -drive if=none,file=tools/disk.img,format=raw,id=MMC

# On MacOS, QEMU creates and uses a virtual Ethernet device using vmnet.
# On Linux, we manually create a virtual Ethernet device using bridge-utils.
# Then QEMU will use this virtual Ethernet device through qemu-bridge-helper.
# P7：下方定义了网卡参数，但默认 qemu 目标没有把它们加入命令；定义变量不等于挂载设备。
QEMU_ETHERNET_MACOS = -device e1000,netdev=E1000,addr=0x3 -netdev vmnet-host,id=E1000
QEMU_ETHERNET_LINUX = -device e1000,netdev=E1000,addr=0x3,mac=52:54:00:12:34:56 -netdev tap,id=E1000,br=virbr0,helper=/usr/lib/qemu/qemu-bridge-helper

qemu: install
	@printf "$(YELLOW)-------- Simulate on QEMU-RISCV --------$(END)\n"
	$(QEMU) $(QEMU_MACHINE) $(QEMU_GRAPHIC) $(QEMU_FLASH_1) $(QEMU_SD_CARD)

# 给编辑器提供每个文件实际使用的编译器、头文件路径和宏，不执行构建。
.PHONY: compdb
compdb:
	python3 tools/gen_compile_commands.py --cc "$(RISCV_CC)" --host-cc "$(CC)"

clean:
	rm -rf build tools/egos.bin tools/mkfs tools/disk.img tools/qemuROM.bin hello.* thread.*

YELLOW = \033[1;33m
CYAN = \033[1;36m
END = \033[0m
