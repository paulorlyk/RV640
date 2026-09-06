# Makefile for the RVC test firmware
#
# Requires a riscv64 GNU binutils/gcc toolchain. On Debian/Ubuntu:
#   sudo apt-get install binutils-riscv64-linux-gnu
# (a bare-metal riscv64-unknown-elf- toolchain works too; just adjust
# CROSS below)
#
# Source layout:
#   common.S         - reusable boot/UART/reporter harness, suite-agnostic.
#                       Provides _start and expects a "run_tests" symbol
#                       from whichever test-suite object is linked in.
#   rvc_tests.S       - RVC suite orchestrator; defines run_tests, calls
#                       tests_quadrant0/1/2.
#   rvc_quadrant0.S   - C.ADDI4SPN, C.LW, C.SW, C.LD, C.SD
#   rvc_quadrant1.S   - C.NOP, C.ADDI, C.ADDIW, C.LI, C.ADDI16SP, C.LUI,
#                       C.SRLI, C.SRAI, C.ANDI, C.SUB, C.XOR, C.OR,
#                       C.AND, C.SUBW, C.ADDW, C.J, C.BEQZ, C.BNEZ
#   rvc_quadrant2.S   - C.SLLI, C.LWSP, C.LDSP, C.JR, C.MV, C.EBREAK,
#                       C.JALR, C.ADD, C.SWSP, C.SDSP
#
# Future instruction-set suites (e.g. an "M" extension test) can be
# added as their own SUITE_SRCS files that also define run_tests,
# reusing common.S unchanged -- see README.md.

CROSS   ?= riscv64-linux-gnu-
AS      = $(CROSS)as
LD      = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy
OBJDUMP = $(CROSS)objdump

ARCH    = rv64imac_zicsr_zifencei
ABI     = lp64

COMMON_SRC  = common.S
SUITE_SRCS  = rvc_tests.S rvc_quadrant0.S rvc_quadrant1.S rvc_quadrant2.S
SUITE_OBJS  = $(SUITE_SRCS:.S=.o)
OUT         = rvc_test

all: $(OUT).bin

common.o: $(COMMON_SRC)
	$(AS) -march=$(ARCH) -mabi=$(ABI) -o $@ $<

%.o: %.S
	$(AS) -march=$(ARCH) -mabi=$(ABI) -o $@ $<

# common.o must come first on the link line so _start (in common.S)
# lands at the very base of .text, i.e. at the 0x80000000 load address.
$(OUT).elf: common.o $(SUITE_OBJS)
	$(LD) -Ttext=0x80000000 --no-dynamic-linker -nostdlib -o $@ common.o $(SUITE_OBJS)

$(OUT).bin: $(OUT).elf
	$(OBJCOPY) -O binary $< $@

disasm: $(OUT).elf
	$(OBJDUMP) -d -M no-aliases $(OUT).elf | less

# Run under QEMU's virt machine, which happens to match the memory map
# this program assumes: RAM at 0x80000000, ns16550a UART at 0x10000000,
# and -bios none boots straight into M-mode at 0x80000000.
run: $(OUT).elf
	qemu-system-riscv64 -M virt -bios none -kernel $(OUT).elf -nographic -serial mon:stdio

clean:
	rm -f common.o $(SUITE_OBJS) $(OUT).o $(OUT).elf $(OUT).bin

.PHONY: all run disasm clean
