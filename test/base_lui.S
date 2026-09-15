/* =====================================================================
 * base_lui.S  --  RV64I base-ISA LUI (load upper immediate)
 *
 * Part of the RV64I base-ISA suite; see base_tests.S for how this
 * plugs into the rest (main_tests.S -> base_tests.S -> here), and
 * common.S for the boot/UART/reporting harness everything sits on
 * ("check", "uart_puts" are used from there; nothing else).
 *
 * LUI is U-type: rd = sign_extend64((int32_t)(imm20 << 12)), where
 * imm20 is a plain, non-scrambled 20-bit field occupying instruction
 * bits [31:12] directly -- unlike RVC's formats there's no bit
 * cross-wiring risk here, so (as with the load/store offset fields)
 * boundary values plus a representative sample stand in for an
 * exhaustive per-bit sweep.
 *
 * rd is a full, unrestricted 5-bit field -- notably including sp,
 * which is the one thing that actually distinguishes LUI from its
 * compressed cousin C.LUI (which reserves rd=x2/sp for C.ADDI16SP
 * instead). That's tested explicitly here, carefully isolated so nothing
 * else touches sp while it briefly holds a non-stack-pointer value:
 * poison, execute, capture the result, restore the *real* sp, and only
 * then call into check() (which needs a valid stack). rd=x0 is also
 * tested (a defined HINT-adjacent no-op case elsewhere in the ISA;
 * here it just needs to not fault and to leave x0 reading as 0).
 *
 * Every register test poisons rd first, the same discipline used for
 * C.LUI/C.LI in the RVC suite -- LUI must fully replace rd, not
 * combine with whatever was there before.
 * ===================================================================== */

    .option norvc            /* mandatory, not stylistic -- see base_loads.S */

    .global tests_base_lui

    .section .text

/* T_LUI_REG rdreg, name -- fixed imm20=0x15 (expect 0x15000), sweep
 * rd across a representative sample. rdreg may be a0/a1/a2 -- result
 * captured to t2 before those get reused. */
.macro T_LUI_REG rdreg, name
    li      \rdreg, 0xdeadbeefdeadbeef
    lui     \rdreg, 0x15
    mv      t2, \rdreg
    la      a0, \name
    li      a1, 0x0000000000015000
    mv      a2, t2
    jal     ra, check
.endm

/* T_LUI_VAL imm20, expect, name -- fixed rd=t1, sweep the immediate. */
.macro T_LUI_VAL imm20, expect, name
    li      t1, 0xdeadbeefdeadbeef
    lui     t1, \imm20
    la      a0, \name
    li      a1, \expect
    mv      a2, t1
    jal     ra, check
.endm

tests_base_lui:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    /* --- every register in the full 5-bit rd field --- */
    T_LUI_REG ra,  str_lui_rd_ra
    T_LUI_REG t0,  str_lui_rd_t0
    T_LUI_REG s0,  str_lui_rd_s0
    T_LUI_REG a0,  str_lui_rd_a0
    T_LUI_REG s10, str_lui_rd_s10
    T_LUI_REG t6,  str_lui_rd_t6

    /* --- rd=x0: must not fault, x0 must still read as 0 --- */
    lui     zero, 0x15
    la      a0, str_lui_rd_x0
    li      a1, 0
    mv      a2, zero
    jal     ra, check

    /* --- rd=sp: the one case that actually distinguishes LUI from
     *     C.LUI. Isolated so sp never holds a non-stack value across
     *     any call -- restore it before touching check(). --- */
    mv      t3, sp                  /* remember the real sp */
    lui     sp, 0x15
    mv      t2, sp                  /* capture the LUI result */
    mv      sp, t3                  /* restore real sp before any call */
    la      a0, str_lui_rd_sp
    li      a1, 0x0000000000015000
    mv      a2, t2
    jal     ra, check

    /* --- every notable immediate value: zero, minimum nonzero,
     *     positive/negative boundary either side of the sign bit
     *     (bit 19 of imm20), all-ones, and two alternating patterns --- */
    T_LUI_VAL 0x00000, 0x0000000000000000, str_lui_imm_zero
    T_LUI_VAL 0x00001, 0x0000000000001000, str_lui_imm_min
    T_LUI_VAL 0x7ffff, 0x000000007ffff000, str_lui_imm_maxpos
    T_LUI_VAL 0x80000, 0xffffffff80000000, str_lui_imm_minneg
    T_LUI_VAL 0xfffff, 0xfffffffffffff000, str_lui_imm_allones
    T_LUI_VAL 0xaaaaa, 0xffffffffaaaaa000, str_lui_imm_alt1
    T_LUI_VAL 0x55555, 0x0000000055555000, str_lui_imm_alt0

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

    .section .rodata
str_lui_rd_ra:  .asciz "LUI rd=ra(x1)"
str_lui_rd_t0:  .asciz "LUI rd=t0(x5)"
str_lui_rd_s0:  .asciz "LUI rd=s0(x8)"
str_lui_rd_a0:  .asciz "LUI rd=a0(x10)"
str_lui_rd_s10: .asciz "LUI rd=s10(x26)"
str_lui_rd_t6:  .asciz "LUI rd=t6(x31)"
str_lui_rd_x0:  .asciz "LUI rd=x0 discards result, no fault"
str_lui_rd_sp:  .asciz "LUI rd=sp (unlike C.LUI, which reserves x2)"
str_lui_imm_zero:   .asciz "LUI imm=0x00000 (zero)"
str_lui_imm_min:    .asciz "LUI imm=0x00001 (min nonzero)"
str_lui_imm_maxpos: .asciz "LUI imm=0x7ffff (max, no sign-ext)"
str_lui_imm_minneg: .asciz "LUI imm=0x80000 (min negative, sign-extends)"
str_lui_imm_allones: .asciz "LUI imm=0xfffff (all ones, sign-extends)"
str_lui_imm_alt1:   .asciz "LUI imm=0xaaaaa (alt pattern, sign-extends)"
str_lui_imm_alt0:   .asciz "LUI imm=0x55555 (alt pattern, no sign-ext)"
