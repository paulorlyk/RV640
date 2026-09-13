/* =====================================================================
 * main_tests.S  --  top-level test dispatcher
 *
 * This is the single file that ties independent instruction-set test
 * suites together. It defines run_tests, the one symbol common.S's
 * reset calls into (see common.S's header for the harness this sits
 * on: boot, UART, "check", pass_count/fail_count). All it does is call
 * each suite's own entry point in turn:
 *
 *   run_rvc_tests   (rvc_tests.S + rvc_quadrant0/1/2.S)  -- RV64C
 *   run_base_tests  (base_tests.S + base_*.S)            -- RV64I base ISA
 *
 * Adding a new suite means writing its own top-level file (following
 * rvc_tests.S or base_tests.S as a template: print a banner, call into
 * one or more category files, each exposing a single entry symbol),
 * then adding one "jal ra, run_yyy_tests" line here and its object
 * file(s) to the Makefile's SUITE_SRCS list. Nothing else needs to
 * change -- every suite already shares common.S's "check"/"uart_puts"
 * and the same pass_count/fail_count totals.
 * ===================================================================== */

    .option norvc

    .global run_tests

    .section .text

run_tests:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    jal     ra, run_rvc_tests
    jal     ra, run_base_tests

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret
