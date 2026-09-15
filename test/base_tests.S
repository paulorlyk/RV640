/* =====================================================================
 * base_tests.S  --  RV64I base (non-compressed) instruction-set suite
 *                    (orchestrator)
 *
 * Mirrors rvc_tests.S's role, one level up: defines run_base_tests,
 * the entry point main_tests.S calls into, which prints a banner and
 * calls each category file's entry point in turn. Category files
 * (base_loads.S, base_stores.S, base_lui.S, base_auipc.S so far; more
 * can be added the same way RVC's quadrant0/1/2 files were) each
 * expose exactly one symbol and keep their own macros/subroutines/data
 * local.
 *
 * Every instruction here is written with .option norvc active
 * throughout its file, which is what actually matters for a suite
 * that's supposed to test the *base* ISA: without it, the assembler
 * would happily substitute a compressed encoding whenever the operand
 * choice happens to be compressible (e.g. "lw s0, 0(s1)" -> c.lw),
 * silently testing the wrong instruction. See base_loads.S's header
 * for more on this.
 * ===================================================================== */

    .option norvc

    .global run_base_tests

    .section .text

run_base_tests:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    la      a0, str_base_banner
    jal     ra, uart_puts

    jal     ra, tests_base_loads
    jal     ra, tests_base_stores
    jal     ra, tests_base_lui
    jal     ra, tests_base_auipc

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

    .section .rodata
str_base_banner: .asciz "\r\n\r\n=== RV64I base ISA test suite ===\r\n\r\n"
