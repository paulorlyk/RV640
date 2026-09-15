# Makefile for the RVC test firmware
#
# Requires a riscv64 GNU binutils/gcc toolchain. On Debian/Ubuntu:
#   sudo apt-get install binutils-riscv64-linux-gnu
# (a bare-metal riscv64-unknown-elf- toolchain works too; just adjust
# CROSS below)
#
# All build outputs (object files, the linked ELF, and the flat
# binary) are placed under BUILD_DIR (default: build/), which is
# created automatically if it doesn't exist. Source files stay in the
# repo root.
#
# Source layout:
#   common.S         - reusable boot/UART/reporter harness, suite-agnostic.
#                       Provides _start and expects a "run_tests" symbol
#                       from whichever test-suite object is linked in.
#   main_tests.S      - top-level dispatcher; defines run_tests, calls
#                       each suite's own entry point in turn.
#   rvc_tests.S       - RVC suite orchestrator; defines run_rvc_tests,
#                       calls tests_quadrant0/1/2.
#   rvc_quadrant0.S   - C.ADDI4SPN, C.LW, C.SW, C.LD, C.SD
#   rvc_quadrant1.S   - C.NOP, C.ADDI, C.ADDIW, C.LI, C.ADDI16SP, C.LUI,
#                       C.SRLI, C.SRAI, C.ANDI, C.SUB, C.XOR, C.OR,
#                       C.AND, C.SUBW, C.ADDW, C.J, C.BEQZ, C.BNEZ
#   rvc_quadrant2.S   - C.SLLI, C.LWSP, C.LDSP, C.JR, C.MV, C.EBREAK,
#                       C.JALR, C.ADD, C.SWSP, C.SDSP
#   base_tests.S      - RV64I base-ISA suite orchestrator; defines
#                       run_base_tests, calls tests_base_loads/
#                       tests_base_stores/tests_base_lui/
#                       tests_base_auipc/tests_base_jal/tests_base_jalr
#                       (and future
#                       base-ISA category files the same way).
#   base_loads.S      - LB, LH, LW, LD, LBU, LHU, LWU
#   base_stores.S     - SB, SH, SW, SD
#   base_lui.S        - LUI
#   base_auipc.S      - AUIPC
#   base_jal.S        - JAL
#   base_jalr.S       - JALR
#   base_branches.S   - BEQ, BNE, BLT, BGE, BLTU, BGEU
#
# Future instruction-set suites (e.g. an "M" extension test) can be
# added as their own file(s) defining run_yyy_tests, with one new line
# in main_tests.S to call it and its object file(s) added to
# SUITE_SRCS below -- see README.md.

CROSS   ?= riscv64-linux-gnu-
AS      = $(CROSS)as
LD      = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy
OBJDUMP = $(CROSS)objdump

ARCH    = rv64imac_zicsr_zifencei
ABI     = lp64

BUILD_DIR   = build

COMMON_SRC  = common.S
SUITE_SRCS  = main_tests.S rvc_tests.S rvc_quadrant0.S rvc_quadrant1.S rvc_quadrant2.S \
              base_tests.S base_loads.S base_stores.S base_lui.S base_auipc.S base_jal.S base_jalr.S base_branches.S
SUITE_OBJS  = $(addprefix $(BUILD_DIR)/,$(SUITE_SRCS:.S=.o))
COMMON_OBJ  = $(BUILD_DIR)/common.o
OUT         = $(BUILD_DIR)/rvc_test

all: $(OUT).bin

# Order-only prerequisite: create the build directory without treating
# its mtime as a reason to rebuild everything else.
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(COMMON_OBJ): $(COMMON_SRC) | $(BUILD_DIR)
	$(AS) -march=$(ARCH) -mabi=$(ABI) -o $@ $<

$(BUILD_DIR)/%.o: %.S | $(BUILD_DIR)
	$(AS) -march=$(ARCH) -mabi=$(ABI) -o $@ $<

# common.o must come first on the link line so _start (in common.S)
# lands at the very base of .text, i.e. at the 0x80000000 load address.
$(OUT).elf: $(COMMON_OBJ) $(SUITE_OBJS)
	$(LD) -Ttext=0x80000000 --no-dynamic-linker -nostdlib -o $@ $(COMMON_OBJ) $(SUITE_OBJS)

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
	rm -rf $(BUILD_DIR)

.PHONY: all run disasm clean
