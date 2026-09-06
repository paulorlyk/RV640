/* =====================================================================
 * common.S  --  bare-metal M-mode test harness (boot / UART / reporter)
 *
 * This file has no knowledge of what is being tested. It provides the
 * reusable boot + I/O + pass/fail-reporting infrastructure that any
 * instruction-set test suite can sit on top of:
 *
 *   - reset vector / M-mode entry at 0x80000000, gp/sp/mtvec setup
 *   - a minimal M-mode trap handler (expects only C.EBREAK/EBREAK,
 *     reports and hangs on anything else)
 *   - an ns16550a UART driver (init for 9600 8N1, putc/puts, hex/dec
 *     printing)
 *   - "check": a generic named pass/fail comparator that prints its
 *     verdict over the UART and tracks a running failure count
 *   - the top-level summary/halt sequence
 *
 * It calls out to a single external entry point, run_tests, which the
 * actual test-suite file (e.g. rvc_tests.S) must define as .global.
 * run_tests should print its own banner, run whatever it wants (using
 * "check" to report each case), and ret when done -- fail_count will
 * already reflect every check() call by then. This is the seam meant
 * for adding new suites later: a new test file just needs to provide
 * its own run_tests (and can freely reuse uart_puts/check/etc. from
 * here), and gets linked in alongside or instead of this one.
 *
 * Runs entirely in M-mode. Assembles to a single flat binary that must
 * be loaded at physical address 0x80000000 and entered at that address
 * (reset vector / -kernel style boot).
 *
 * An ns16550a UART sits at 0x10000000 with 1-byte register stride (the
 * common QEMU-virt style mapping):
 *   +0 RBR/THR/DLL   +1 IER/DLM   +2 IIR/FCR   +3 LCR
 *   +4 MCR           +5 LSR       +6 MSR       +7 SCR
 * LSR bit 5 (0x20) = THRE (transmit holding register empty).
 * At boot, uart_init() programs it for 9600 8N1: interrupts off, DLAB
 * set to load the baud divisor, then DLAB cleared with LCR = 8 data
 * bits / no parity / 1 stop bit, FIFOs enabled, and modem control
 * lines (DTR/RTS/OUT2) asserted. See UART_CLK_HZ below -- the divisor
 * is computed from it at assemble time, so if your UART's reference
 * clock differs from the 1.8432 MHz default assumed here, change that
 * one constant and everything else follows.
 *
 * Build (see Makefile):
 *   riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 \
 *                        -o common.o common.S
 *   riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 \
 *                        -o rvc_tests.o rvc_tests.S
 *   riscv64-linux-gnu-ld -Ttext=0x80000000 --no-dynamic-linker \
 *                        -nostdlib -o rvc_test.elf common.o rvc_tests.o
 *   riscv64-linux-gnu-objcopy -O binary rvc_test.elf rvc_test.bin
 * (common.o must be listed first at link time, so that _start ends up
 * at the very base of .text, i.e. at the 0x80000000 load address.)
 * ===================================================================== */

    .option norvc           /* this file is hand-written plain RV64IMA;  */
                             /* keep the assembler from silently          */
                             /* compressing it, so the disassembly stays  */
                             /* predictable. Each test-suite file makes   */
                             /* its own choice for the code it owns. */

    .equ UART_BASE, 0x10000000
    .equ UART_RBR,  0              /* receive buffer (read), DLAB=0 */
    .equ UART_THR,  0              /* transmit holding (write), DLAB=0 */
    .equ UART_DLL,  0              /* divisor latch low, DLAB=1 */
    .equ UART_IER,  1              /* interrupt enable, DLAB=0 */
    .equ UART_DLM,  1              /* divisor latch high, DLAB=1 */
    .equ UART_FCR,  2              /* FIFO control (write) */
    .equ UART_LCR,  3              /* line control */
    .equ UART_MCR,  4              /* modem control */
    .equ UART_LSR,  5              /* line status */
    .equ UART_THRE, 0x20           /* LSR: transmit holding reg empty */

    /* --- 9600 8N1 configuration --- */
    .equ UART_CLK_HZ,   1843200    /* ns16550 reference clock; the classic
                                     * PC/16550 value. Change to match your
                                     * actual UART input clock if different
                                     * (e.g. many SoCs use their peripheral
                                     * bus clock instead). */
    .equ UART_BAUD,     9600
    .equ UART_DIVISOR,  (UART_CLK_HZ / (16 * UART_BAUD))
    .equ UART_DIV_LO,   (UART_DIVISOR & 0xff)
    .equ UART_DIV_HI,   ((UART_DIVISOR >> 8) & 0xff)
    .equ UART_LCR_8N1,  0x03       /* 8 data bits, no parity, 1 stop bit */
    .equ UART_LCR_DLAB, 0x80
    .equ UART_FCR_INIT, 0xc7       /* enable FIFO, clear RX/TX, 14-byte trig */
    .equ UART_MCR_INIT, 0x0b       /* DTR | RTS | OUT2 */

/* Symbols other test-suite files are expected to call/reference.
 * run_tests is the one symbol *this* file expects someone else to
 * provide (see file header). */
    .global _start
    .global uart_init
    .global uart_putc
    .global uart_puts
    .global uart_put_hex64
    .global uart_put_dec
    .global check
    .global fail_count
    .global pass_count
    .global ebreak_flag
    .global ebreak_mepc
    .global ebreak_mcause
    .global ebreak_hit_count

/* ----------------------------------------------------------------------
 * Header / vector area. We keep this minimal: a single instruction at
 * the load/entry address that jumps to the real start, immediately
 * followed by the M-mode trap handler (must be 4-byte aligned, which
 * it naturally is here).
 * -------------------------------------------------------------------- */
    .section .text
    .align 2
_start:
    j       reset

    .align 4
trap_handler:
    /* Minimal M-mode trap handler. We only *expect* to ever land here
     * from a deliberate EBREAK/C.EBREAK test (mcause == 3, breakpoint).
     * Anything else is a genuine failure (e.g. illegal instruction
     * because the target doesn't really implement an extension being
     * tested) and we report it and hang, since continuing would be
     * meaningless. */
    addi    sp, sp, -32
    sd      t0, 0(sp)
    sd      t1, 8(sp)
    sd      a0, 16(sp)
    sd      ra, 24(sp)

    csrr    t0, mcause
    li      t1, 3                   /* 3 = breakpoint exception */
    bne     t0, t1, trap_unexpected

    /* Expected EBREAK: record mcause and the raw (pre-advance) mepc so
     * the test suite can check both explicitly rather than trusting
     * this handler's own internal comparison above, bump a hit
     * counter (so a test can confirm the trap really fires every time
     * across repeated EBREAKs, not just once), set the "we got here"
     * flag, then skip past the instruction (mepc += 2 assumes a
     * compressed C.EBREAK; a suite testing the uncompressed EBREAK
     * would need mepc += 4 instead -- there is only one
     * ebreak_flag/handler here since only one test at a time is
     * expected to rely on it) and resume. */
    la      t1, ebreak_mcause
    sw      t0, 0(t1)

    csrr    t0, mepc
    la      t1, ebreak_mepc
    sd      t0, 0(t1)

    la      t1, ebreak_hit_count
    lw      t0, 0(t1)
    addiw   t0, t0, 1
    sw      t0, 0(t1)

    la      t1, ebreak_flag
    li      t0, 1
    sw      t0, 0(t1)
    csrr    t0, mepc
    addi    t0, t0, 2
    csrw    mepc, t0

    ld      t0, 0(sp)
    ld      t1, 8(sp)
    ld      a0, 16(sp)
    ld      ra, 24(sp)
    addi    sp, sp, 32
    mret

trap_unexpected:
    la      a0, str_unexpected_trap
    jal     ra, uart_puts
    csrr    a0, mcause
    jal     ra, uart_put_hex64
    la      a0, str_newline
    jal     ra, uart_puts
trap_hang:
    j       trap_hang

/* ----------------------------------------------------------------------
 * Real entry point
 * -------------------------------------------------------------------- */
reset:
    /* The linker enables relaxation by default, which can turn later
     * "la reg, symbol" sequences into a single gp-relative addi if the
     * symbol lands near __global_pointer$. gp is NOT initialised by
     * hardware at reset, so we must set it up first (with relaxation
     * disabled for this one instruction, to avoid a chicken-and-egg
     * gp-relative load of gp itself). */
    .option push
    .option norelax
    la      gp, __global_pointer$
    .option pop

    la      sp, stack_top
    la      t0, trap_handler        /* direct mode, low 2 bits = 0 */
    csrw    mtvec, t0

    li      t0, 0
    la      t1, fail_count
    sw      t0, 0(t1)
    la      t1, pass_count
    sw      t0, 0(t1)
    la      t1, ebreak_flag
    sw      t0, 0(t1)
    la      t1, ebreak_mcause
    sw      t0, 0(t1)
    la      t1, ebreak_hit_count
    sw      t0, 0(t1)
    la      t1, ebreak_mepc
    sd      t0, 0(t1)

    jal     ra, uart_init

    jal     ra, run_tests           /* provided by the test-suite file */

    la      a0, str_summary_pass
    jal     ra, uart_puts
    la      t0, pass_count
    lw      a0, 0(t0)
    jal     ra, uart_put_dec

    la      a0, str_summary_fail
    jal     ra, uart_puts
    la      t0, fail_count
    lw      a0, 0(t0)
    jal     ra, uart_put_dec
    la      a0, str_summary_tail
    jal     ra, uart_puts

    la      t0, fail_count
    lw      a1, 0(t0)
    bnez    a1, halt_fail

    la      a0, str_all_pass
    jal     ra, uart_puts
    j       halt

halt_fail:
    la      a0, str_some_fail
    jal     ra, uart_puts
halt:
    j       halt

/* =======================================================================
 *  UART driver (plain, non-compressed, hand written)
 * ======================================================================= */

/* uart_init: program the ns16550a for 9600 8N1, polling mode (no
 * interrupts). Clobbers t0, t1. Must run before any uart_putc/uart_puts
 * call, and before gp-relative addressing is needed (it uses only an
 * absolute UART_BASE, so it's safe to call this before "la gp, ..." if
 * you ever reorder startup). */
uart_init:
    li      t0, UART_BASE

    li      t1, 0x00
    sb      t1, UART_IER(t0)        /* disable all UART interrupts */

    li      t1, UART_LCR_DLAB
    sb      t1, UART_LCR(t0)        /* set DLAB to expose DLL/DLM */

    li      t1, UART_DIV_LO
    sb      t1, UART_DLL(t0)
    li      t1, UART_DIV_HI
    sb      t1, UART_DLM(t0)        /* divisor -> 9600 baud (see UART_CLK_HZ) */

    li      t1, UART_LCR_8N1
    sb      t1, UART_LCR(t0)        /* 8 data bits, no parity, 1 stop bit;
                                      * clearing DLAB restores RBR/THR/IER */

    li      t1, UART_FCR_INIT
    sb      t1, UART_FCR(t0)        /* enable + clear FIFOs */

    li      t1, UART_MCR_INIT
    sb      t1, UART_MCR(t0)        /* assert DTR/RTS/OUT2 */
    ret

/* uart_putc: a0 = character to send. Clobbers t0. */
uart_putc:
    li      t0, UART_BASE
1:  lb      t1, UART_LSR(t0)
    andi    t1, t1, UART_THRE
    beqz    t1, 1b
    sb      a0, 0(t0)
    ret

/* uart_puts: a0 = pointer to NUL-terminated string. */
uart_puts:
    addi    sp, sp, -16
    sd      ra, 0(sp)
    sd      s0, 8(sp)
    mv      s0, a0
1:  lb      a0, 0(s0)
    beqz    a0, 2f
    jal     ra, uart_putc
    addi    s0, s0, 1
    j       1b
2:  ld      ra, 0(sp)
    ld      s0, 8(sp)
    addi    sp, sp, 16
    ret

/* uart_put_hex64: a0 = value, printed as 16 hex digits with 0x prefix. */
uart_put_hex64:
    addi    sp, sp, -32
    sd      ra, 0(sp)
    sd      s0, 8(sp)
    sd      s1, 16(sp)
    mv      s0, a0
    li      s1, 60                  /* start at top nibble (bit 60) */
    li      a0, '0'
    jal     ra, uart_putc
    li      a0, 'x'
    jal     ra, uart_putc
1:  srl     t0, s0, s1
    andi    t0, t0, 0xf
    li      t1, 10
    blt     t0, t1, 2f
    addi    a0, t0, ('a' - 10)
    j       3f
2:  addi    a0, t0, '0'
3:  jal     ra, uart_putc
    addi    s1, s1, -4
    bgez    s1, 1b
    ld      ra, 0(sp)
    ld      s0, 8(sp)
    ld      s1, 16(sp)
    addi    sp, sp, 32
    ret

/* uart_put_dec: a0 = unsigned value (small, e.g. a fail counter), printed
 * as decimal. Simple/slow, fine for a test counter. */
uart_put_dec:
    addi    sp, sp, -32
    sd      ra, 0(sp)
    sd      s0, 8(sp)
    mv      s0, a0
    bnez    s0, 1f
    li      a0, '0'
    jal     ra, uart_putc
    j       9f
1:  addi    sp, sp, -16             /* small digit stack, up to 16 digits */
    li      t2, 0                   /* digit count */
2:  beqz    s0, 3f
    li      t3, 10
    remu    t4, s0, t3
    divu    s0, s0, t3
    addi    t4, t4, '0'
    add     t5, sp, t2
    sb      t4, 0(t5)
    addi    t2, t2, 1
    j       2b
3:  beqz    t2, 4f
    addi    t2, t2, -1
    add     t5, sp, t2
    lb      a0, 0(t5)
    jal     ra, uart_putc
    j       3b
4:  addi    sp, sp, 16
9:  ld      ra, 0(sp)
    ld      s0, 8(sp)
    addi    sp, sp, 32
    ret

/* ----------------------------------------------------------------------
 * check: generic pass/fail reporter.
 *   a0 = pointer to test-name string (NUL terminated, no newline)
 *   a1 = expected value
 *   a2 = actual value
 * Prints "<name> ... PASS\r\n" or "<name> ... FAIL\r\n" and bumps
 * pass_count or fail_count accordingly. Clobbers t0-t6, a0-a2.
 * -------------------------------------------------------------------- */
check:
    addi    sp, sp, -32
    sd      ra, 0(sp)
    sd      s0, 8(sp)
    sd      s1, 16(sp)
    mv      s0, a1
    mv      s1, a2
    jal     ra, uart_puts
    la      a0, str_dots
    jal     ra, uart_puts
    bne     s0, s1, .Lcheck_fail
    j       .Lpass_count
.Lcheck_fail:
    la      a0, str_fail
    jal     ra, uart_puts
    la      t0, fail_count
    lw      t1, 0(t0)
    addiw   t1, t1, 1
    sw      t1, 0(t0)
    j       .Lcheck_done
.Lpass_count:
    la      a0, str_pass
    jal     ra, uart_puts
    la      t0, pass_count
    lw      t1, 0(t0)
    addiw   t1, t1, 1
    sw      t1, 0(t0)
.Lcheck_done:
    ld      ra, 0(sp)
    ld      s0, 8(sp)
    ld      s1, 16(sp)
    addi    sp, sp, 32
    ret

/* =======================================================================
 *  Strings (generic to the harness, not to any particular test suite)
 * ======================================================================= */
    .section .rodata
str_dots:             .asciz " ... "
str_pass:             .asciz "PASS\r\n"
str_fail:             .asciz "FAIL\r\n"
str_newline:          .asciz "\r\n"
str_summary_pass:     .asciz "\r\n--- Summary: pass_count = "
str_summary_fail:     .asciz ", fail_count = "
str_summary_tail:     .asciz " ---\r\n"
str_all_pass:         .asciz "ALL TESTS PASSED\r\n"
str_some_fail:        .asciz "ONE OR MORE TESTS FAILED\r\n"
str_unexpected_trap:  .asciz "\r\n!!! UNEXPECTED TRAP, mcause="

/* =======================================================================
 *  Uninitialised data (contiguous with .text/.rodata in the flat binary,
 *  so it comes out zero-filled with no separate startup zeroing needed)
 * ======================================================================= */
    .section .bss
    .align 3
fail_count:   .word 0
pass_count:   .word 0
ebreak_flag:  .word 0
ebreak_mcause: .word 0
ebreak_hit_count: .word 0
    .align 3
ebreak_mepc:  .dword 0

    .align 4
    .space 4096
stack_top:
