/* =====================================================================
 * base_branches.S  --  RV64I base-ISA branches: BEQ, BNE, BLT, BGE,
 *                       BLTU, BGEU
 *
 * Part of the RV64I base-ISA suite; see base_tests.S for how this
 * plugs into the rest (main_tests.S -> base_tests.S -> here), and
 * common.S for the boot/UART/reporting harness everything sits on
 * ("check", "uart_puts" are used from there; nothing else).
 *
 * The six branches share one B-type encoding and differ only in the
 * comparison in funct3. So the coverage here is organised around what
 * actually distinguishes them from each other, rather than repeating
 * an identical offset sweep six times:
 *
 *   - **The comparison truth table.** Every instruction is run against
 *     the same set of operand pairs, with the expected taken/not-taken
 *     outcome for each spelled out explicitly. Running all six through
 *     identical inputs means a decoder that swaps two of them (BLT for
 *     BGE, say, or BLTU for BLT) produces a visible mismatch rather
 *     than something that could quietly pass.
 *   - **Signed vs unsigned is the headline case.** BLT/BGE compare as
 *     signed; BLTU/BGEU compare the same bits as unsigned. The pairs
 *     below deliberately include operands where those two readings
 *     disagree -- e.g. rs1=-1, rs2=1: signed, -1 < 1 so BLT is taken;
 *     unsigned, 0xffff...f > 1 so BLTU is *not*. Same bits, opposite
 *     results. INT64_MIN vs INT64_MAX is the other such pair. These
 *     are the cases that catch a signed/unsigned mixup, which is
 *     exactly the bug class that has bitten this project's own test
 *     code more than once (see the README's note on lw/lwu).
 *   - **Equal operands** are covered for all six, since that's the
 *     boundary where the strict/non-strict distinction shows up
 *     (BLT not taken vs BGE taken on x == x).
 *
 * The 12-bit branch offset is a B-type field scrambled across
 * non-adjacent bits (imm[12|10:5] and imm[4:1|11]), identical in all
 * six instructions -- so it's swept once, through BEQ, rather than six
 * times over. Same scoping rationale as base_jal.S: every bit of the
 * low 12 is covered in both directions; the range extends to ~+-4KB
 * and the construction cost of the top bits isn't worth the binary
 * size. Offsets are built from .rept-generated 4-byte nop fillers and
 * every one was verified by disassembly (target - branch computed
 * directly), not trusted from the .rept count -- see the README's note
 * on assembler relaxation for why that matters at boundaries.
 * ===================================================================== */

    .option norvc            /* mandatory, not stylistic: BEQ/BNE against
                               * x0 with an x8-x15 register have compressed
                               * forms (C.BEQZ/C.BNEZ) the assembler would
                               * otherwise substitute, silently testing the
                               * RVC instruction instead of this one. */

    .global tests_base_branches

    .section .text

/* T_BR insn, v1, v2, taken, name
 *   rs1 = v1, rs2 = v2, then branch. If the branch is taken it skips
 *   the marker store, leaving s4 == 0; if not taken it falls through
 *   and sets s4 == 1. So the expected marker is simply (1 - taken),
 *   which lets one macro express both outcomes -- no separate
 *   "taken" and "not taken" variants that could drift apart. */
.macro T_BR insn, v1, v2, taken, name
    li      s4, 0
    li      t1, \v1
    li      t2, \v2
    \insn   t1, t2, 1f
    li      s4, 1                   /* only reached if NOT taken */
1:
    la      a0, \name
    li      a1, (1 - \taken)
    mv      a2, s4
    jal     ra, check
.endm

/* T_BR_RS1 / T_BR_RS2 -- sweep each register field independently,
 * using BEQ with equal values so the branch is taken. The other
 * operand is fixed at a register deliberately outside the swept list.
 * The swept register is consumed by the branch before a0-a2 are
 * reused, so it may itself be a0/a1/a2. */
.macro T_BR_RS1 reg, name
    li      s4, 0
    li      s3, 0x5a5a5a5a5a5a5a5a
    li      \reg, 0x5a5a5a5a5a5a5a5a
    beq     \reg, s3, 1f
    li      s4, 999                 /* poison: only hit if branch failed */
1:
    la      a0, \name
    li      a1, 0
    mv      a2, s4
    jal     ra, check
.endm

.macro T_BR_RS2 reg, name
    li      s4, 0
    li      s3, 0x5a5a5a5a5a5a5a5a
    li      \reg, 0x5a5a5a5a5a5a5a5a
    beq     s3, \reg, 1f
    li      s4, 999
1:
    la      a0, \name
    li      a1, 0
    mv      a2, s4
    jal     ra, check
.endm

/* T_BR_FWD / T_BR_BWD dist, name -- offset sweep, through BEQ.
 * Forward: beq(4) + poison li(4) + N nop fillers(4N), dist = 8 + 4N.
 * Backward: jump ahead past the filler run first (not under test),
 * then branch back over it. dist = 8 + 4N. */
.macro T_BR_FWD dist, name
    li      s4, 0
    li      t1, 7
    li      t2, 7
    beq     t1, t2, 1f
    li      s4, 999                 /* poison: only hit if branch landed wrong */
    .rept (\dist - 8) / 4
    nop
    .endr
1:
    la      a0, \name
    li      a1, 0
    mv      a2, s4
    jal     ra, check
.endm

.macro T_BR_BWD dist, name
    li      s4, 0
    li      t1, 7
    li      t2, 7
    jal     zero, 5f
4:
    li      s4, 1                   /* success marker: the true target */
    jal     zero, 6f
5:
    .rept (\dist - 8) / 4
    nop
    .endr
    beq     t1, t2, 4b
    li      s4, 99                  /* poison: only hit if branch landed wrong */
6:
    la      a0, \name
    li      a1, 1
    mv      a2, s4
    jal     ra, check
.endm

tests_base_branches:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    /* ===== comparison truth table: every instruction, same inputs =====
     * Expected outcomes computed from the RISC-V definitions:
     * BEQ/BNE compare bitwise; BLT/BGE signed; BLTU/BGEU unsigned. */

    /* --- equal operands (0 vs 0): the strict/non-strict boundary --- */
    T_BR beq,  0, 0, 1, str_br_eq_beq
    T_BR bne,  0, 0, 0, str_br_eq_bne
    T_BR blt,  0, 0, 0, str_br_eq_blt
    T_BR bge,  0, 0, 1, str_br_eq_bge
    T_BR bltu, 0, 0, 0, str_br_eq_bltu
    T_BR bgeu, 0, 0, 1, str_br_eq_bgeu

    /* --- 1 vs 2: both readings agree (less than) --- */
    T_BR beq,  1, 2, 0, str_br_1_2_beq
    T_BR bne,  1, 2, 1, str_br_1_2_bne
    T_BR blt,  1, 2, 1, str_br_1_2_blt
    T_BR bge,  1, 2, 0, str_br_1_2_bge
    T_BR bltu, 1, 2, 1, str_br_1_2_bltu
    T_BR bgeu, 1, 2, 0, str_br_1_2_bgeu

    /* --- 2 vs 1: both readings agree (greater than) --- */
    T_BR beq,  2, 1, 0, str_br_2_1_beq
    T_BR bne,  2, 1, 1, str_br_2_1_bne
    T_BR blt,  2, 1, 0, str_br_2_1_blt
    T_BR bge,  2, 1, 1, str_br_2_1_bge
    T_BR bltu, 2, 1, 0, str_br_2_1_bltu
    T_BR bgeu, 2, 1, 1, str_br_2_1_bgeu

    /* --- -1 vs 1: THE signed/unsigned divergence. Signed: -1 < 1, so
     *     BLT taken / BGE not. Unsigned: 0xffff...f > 1, so BLTU *not*
     *     taken / BGEU taken. Identical bits, opposite results. --- */
    T_BR beq,  -1, 1, 0, str_br_m1_1_beq
    T_BR bne,  -1, 1, 1, str_br_m1_1_bne
    T_BR blt,  -1, 1, 1, str_br_m1_1_blt
    T_BR bge,  -1, 1, 0, str_br_m1_1_bge
    T_BR bltu, -1, 1, 0, str_br_m1_1_bltu
    T_BR bgeu, -1, 1, 1, str_br_m1_1_bgeu

    /* --- 1 vs -1: the same divergence, operands reversed --- */
    T_BR beq,  1, -1, 0, str_br_1_m1_beq
    T_BR bne,  1, -1, 1, str_br_1_m1_bne
    T_BR blt,  1, -1, 0, str_br_1_m1_blt
    T_BR bge,  1, -1, 1, str_br_1_m1_bge
    T_BR bltu, 1, -1, 1, str_br_1_m1_bltu
    T_BR bgeu, 1, -1, 0, str_br_1_m1_bgeu

    /* --- -1 vs 0: signed -1 < 0 (BLT taken), unsigned 0xffff...f > 0
     *     (BLTU not taken). Divergence against zero specifically. --- */
    T_BR blt,  -1, 0, 1, str_br_m1_0_blt
    T_BR bge,  -1, 0, 0, str_br_m1_0_bge
    T_BR bltu, -1, 0, 0, str_br_m1_0_bltu
    T_BR bgeu, -1, 0, 1, str_br_m1_0_bgeu

    /* --- -1 vs -2: both negative; signed and unsigned agree here
     *     (-1 > -2 signed, 0xff..ff > 0xff..fe unsigned) --- */
    T_BR blt,  -1, -2, 0, str_br_m1_m2_blt
    T_BR bge,  -1, -2, 1, str_br_m1_m2_bge
    T_BR bltu, -1, -2, 0, str_br_m1_m2_bltu
    T_BR bgeu, -1, -2, 1, str_br_m1_m2_bgeu

    /* --- INT64_MIN vs INT64_MAX: the extreme divergence. Signed,
     *     MIN < MAX (BLT taken). Unsigned, 0x8000... > 0x7fff...
     *     (BLTU not taken). --- */
    T_BR blt,  0x8000000000000000, 0x7fffffffffffffff, 1, str_br_minmax_blt
    T_BR bge,  0x8000000000000000, 0x7fffffffffffffff, 0, str_br_minmax_bge
    T_BR bltu, 0x8000000000000000, 0x7fffffffffffffff, 0, str_br_minmax_bltu
    T_BR bgeu, 0x8000000000000000, 0x7fffffffffffffff, 1, str_br_minmax_bgeu

    /* --- INT64_MAX vs INT64_MIN: same, reversed --- */
    T_BR blt,  0x7fffffffffffffff, 0x8000000000000000, 0, str_br_maxmin_blt
    T_BR bge,  0x7fffffffffffffff, 0x8000000000000000, 1, str_br_maxmin_bge
    T_BR bltu, 0x7fffffffffffffff, 0x8000000000000000, 1, str_br_maxmin_bltu
    T_BR bgeu, 0x7fffffffffffffff, 0x8000000000000000, 0, str_br_maxmin_bgeu

    /* ===== rs1 == rs2 aliasing: the same register on both sides. =====
     * Not a hazard (branches write no register), but the comparison
     * must still be self-consistent: x == x, so BEQ/BGE/BGEU are
     * always taken and BNE/BLT/BLTU never are, whatever x holds. */
    li      s4, 0
    li      t1, 0xdeadbeefcafebabe
    beq     t1, t1, 1f
    li      s4, 999
1:
    la      a0, str_br_alias_beq
    li      a1, 0
    mv      a2, s4
    jal     ra, check

    li      s4, 0
    li      t1, 0xdeadbeefcafebabe
    bne     t1, t1, 1f
    li      s4, 1
1:
    la      a0, str_br_alias_bne
    li      a1, 1
    mv      a2, s4
    jal     ra, check

    li      s4, 0
    li      t1, 0xdeadbeefcafebabe
    blt     t1, t1, 1f
    li      s4, 1
1:
    la      a0, str_br_alias_blt
    li      a1, 1
    mv      a2, s4
    jal     ra, check

    li      s4, 0
    li      t1, 0xdeadbeefcafebabe
    bge     t1, t1, 1f
    li      s4, 999
1:
    la      a0, str_br_alias_bge
    li      a1, 0
    mv      a2, s4
    jal     ra, check

    li      s4, 0
    li      t1, 0xdeadbeefcafebabe
    bltu    t1, t1, 1f
    li      s4, 1
1:
    la      a0, str_br_alias_bltu
    li      a1, 1
    mv      a2, s4
    jal     ra, check

    li      s4, 0
    li      t1, 0xdeadbeefcafebabe
    bgeu    t1, t1, 1f
    li      s4, 999
1:
    la      a0, str_br_alias_bgeu
    li      a1, 0
    mv      a2, s4
    jal     ra, check

    /* ===== x0 as an operand: comparing against the hardwired zero
     * register, the "branch if (non)zero" idiom. ===== */
    li      s4, 0
    li      t1, 0
    beq     t1, zero, 1f
    li      s4, 999
1:
    la      a0, str_br_x0_beq_taken
    li      a1, 0
    mv      a2, s4
    jal     ra, check

    li      s4, 0
    li      t1, 5
    beq     t1, zero, 1f
    li      s4, 1
1:
    la      a0, str_br_x0_beq_nottaken
    li      a1, 1
    mv      a2, s4
    jal     ra, check

    li      s4, 0
    li      t1, -1
    blt     t1, zero, 1f            /* signed: -1 < 0, taken */
    li      s4, 999
1:
    la      a0, str_br_x0_blt
    li      a1, 0
    mv      a2, s4
    jal     ra, check

    li      s4, 0
    li      t1, -1
    bltu    t1, zero, 1f            /* unsigned: 0xff..ff > 0, NOT taken */
    li      s4, 1
1:
    la      a0, str_br_x0_bltu
    li      a1, 1
    mv      a2, s4
    jal     ra, check

    /* ===== register sweeps (BEQ, equal values) ===== */
    T_BR_RS1 ra,  str_br_rs1_ra
    T_BR_RS1 t0,  str_br_rs1_t0
    T_BR_RS1 s0,  str_br_rs1_s0
    T_BR_RS1 a0,  str_br_rs1_a0
    T_BR_RS1 s10, str_br_rs1_s10
    T_BR_RS1 t6,  str_br_rs1_t6

    T_BR_RS2 ra,  str_br_rs2_ra
    T_BR_RS2 t0,  str_br_rs2_t0
    T_BR_RS2 s0,  str_br_rs2_s0
    T_BR_RS2 a0,  str_br_rs2_a0
    T_BR_RS2 s10, str_br_rs2_s10
    T_BR_RS2 t6,  str_br_rs2_t6

    /* ===== offset sweep (through BEQ; all six share the identical
     * B-type offset encoding, so sweeping it once is sufficient) ===== */
    T_BR_FWD 8,    str_br_fwd8
    T_BR_FWD 16,   str_br_fwd16
    T_BR_FWD 32,   str_br_fwd32
    T_BR_FWD 64,   str_br_fwd64
    T_BR_FWD 128,  str_br_fwd128
    T_BR_FWD 256,  str_br_fwd256
    T_BR_FWD 512,  str_br_fwd512
    T_BR_FWD 1024, str_br_fwd1024
    T_BR_FWD 2048, str_br_fwd2048
    T_BR_FWD 4092, str_br_fwdmax

    T_BR_BWD 8,    str_br_bwd8
    T_BR_BWD 16,   str_br_bwd16
    T_BR_BWD 32,   str_br_bwd32
    T_BR_BWD 64,   str_br_bwd64
    T_BR_BWD 128,  str_br_bwd128
    T_BR_BWD 256,  str_br_bwd256
    T_BR_BWD 512,  str_br_bwd512
    T_BR_BWD 1024, str_br_bwd1024
    T_BR_BWD 2048, str_br_bwd2048
    T_BR_BWD 4092, str_br_bwdmax

    /* ===== a real loop driven by a backward branch, exercised on
     * every iteration but the last ===== */
    li      s0, 0
2:
    addi    s0, s0, 1
    li      t0, 5
    bne     s0, t0, 2b
    la      a0, str_br_loop
    li      a1, 5
    mv      a2, s0
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

    .section .rodata
str_br_eq_beq:  .asciz "BEQ 0,0 taken"
str_br_eq_bne:  .asciz "BNE 0,0 not taken"
str_br_eq_blt:  .asciz "BLT 0,0 not taken (strict)"
str_br_eq_bge:  .asciz "BGE 0,0 taken (non-strict)"
str_br_eq_bltu: .asciz "BLTU 0,0 not taken (strict)"
str_br_eq_bgeu: .asciz "BGEU 0,0 taken (non-strict)"
str_br_1_2_beq:  .asciz "BEQ 1,2 not taken"
str_br_1_2_bne:  .asciz "BNE 1,2 taken"
str_br_1_2_blt:  .asciz "BLT 1,2 taken"
str_br_1_2_bge:  .asciz "BGE 1,2 not taken"
str_br_1_2_bltu: .asciz "BLTU 1,2 taken"
str_br_1_2_bgeu: .asciz "BGEU 1,2 not taken"
str_br_2_1_beq:  .asciz "BEQ 2,1 not taken"
str_br_2_1_bne:  .asciz "BNE 2,1 taken"
str_br_2_1_blt:  .asciz "BLT 2,1 not taken"
str_br_2_1_bge:  .asciz "BGE 2,1 taken"
str_br_2_1_bltu: .asciz "BLTU 2,1 not taken"
str_br_2_1_bgeu: .asciz "BGEU 2,1 taken"
str_br_m1_1_beq:  .asciz "BEQ -1,1 not taken"
str_br_m1_1_bne:  .asciz "BNE -1,1 taken"
str_br_m1_1_blt:  .asciz "BLT -1,1 taken (signed: -1 < 1)"
str_br_m1_1_bge:  .asciz "BGE -1,1 not taken (signed)"
str_br_m1_1_bltu: .asciz "BLTU -1,1 NOT taken (unsigned: 0xff..ff > 1)"
str_br_m1_1_bgeu: .asciz "BGEU -1,1 taken (unsigned)"
str_br_1_m1_beq:  .asciz "BEQ 1,-1 not taken"
str_br_1_m1_bne:  .asciz "BNE 1,-1 taken"
str_br_1_m1_blt:  .asciz "BLT 1,-1 not taken (signed)"
str_br_1_m1_bge:  .asciz "BGE 1,-1 taken (signed)"
str_br_1_m1_bltu: .asciz "BLTU 1,-1 taken (unsigned: 1 < 0xff..ff)"
str_br_1_m1_bgeu: .asciz "BGEU 1,-1 not taken (unsigned)"
str_br_m1_0_blt:  .asciz "BLT -1,0 taken (signed)"
str_br_m1_0_bge:  .asciz "BGE -1,0 not taken (signed)"
str_br_m1_0_bltu: .asciz "BLTU -1,0 NOT taken (unsigned)"
str_br_m1_0_bgeu: .asciz "BGEU -1,0 taken (unsigned)"
str_br_m1_m2_blt:  .asciz "BLT -1,-2 not taken"
str_br_m1_m2_bge:  .asciz "BGE -1,-2 taken"
str_br_m1_m2_bltu: .asciz "BLTU -1,-2 not taken"
str_br_m1_m2_bgeu: .asciz "BGEU -1,-2 taken"
str_br_minmax_blt:  .asciz "BLT INT64_MIN,INT64_MAX taken (signed)"
str_br_minmax_bge:  .asciz "BGE INT64_MIN,INT64_MAX not taken (signed)"
str_br_minmax_bltu: .asciz "BLTU INT64_MIN,INT64_MAX NOT taken (unsigned)"
str_br_minmax_bgeu: .asciz "BGEU INT64_MIN,INT64_MAX taken (unsigned)"
str_br_maxmin_blt:  .asciz "BLT INT64_MAX,INT64_MIN not taken (signed)"
str_br_maxmin_bge:  .asciz "BGE INT64_MAX,INT64_MIN taken (signed)"
str_br_maxmin_bltu: .asciz "BLTU INT64_MAX,INT64_MIN taken (unsigned)"
str_br_maxmin_bgeu: .asciz "BGEU INT64_MAX,INT64_MIN not taken (unsigned)"
str_br_alias_beq:  .asciz "BEQ rs1==rs2 always taken"
str_br_alias_bne:  .asciz "BNE rs1==rs2 never taken"
str_br_alias_blt:  .asciz "BLT rs1==rs2 never taken"
str_br_alias_bge:  .asciz "BGE rs1==rs2 always taken"
str_br_alias_bltu: .asciz "BLTU rs1==rs2 never taken"
str_br_alias_bgeu: .asciz "BGEU rs1==rs2 always taken"
str_br_x0_beq_taken:    .asciz "BEQ rs2=x0 taken (value is zero)"
str_br_x0_beq_nottaken: .asciz "BEQ rs2=x0 not taken (value nonzero)"
str_br_x0_blt:  .asciz "BLT rs2=x0 taken (signed -1 < 0)"
str_br_x0_bltu: .asciz "BLTU rs2=x0 NOT taken (unsigned 0xff..ff > 0)"
str_br_rs1_ra:  .asciz "BEQ rs1=ra(x1)"
str_br_rs1_t0:  .asciz "BEQ rs1=t0(x5)"
str_br_rs1_s0:  .asciz "BEQ rs1=s0(x8)"
str_br_rs1_a0:  .asciz "BEQ rs1=a0(x10)"
str_br_rs1_s10: .asciz "BEQ rs1=s10(x26)"
str_br_rs1_t6:  .asciz "BEQ rs1=t6(x31)"
str_br_rs2_ra:  .asciz "BEQ rs2=ra(x1)"
str_br_rs2_t0:  .asciz "BEQ rs2=t0(x5)"
str_br_rs2_s0:  .asciz "BEQ rs2=s0(x8)"
str_br_rs2_a0:  .asciz "BEQ rs2=a0(x10)"
str_br_rs2_s10: .asciz "BEQ rs2=s10(x26)"
str_br_rs2_t6:  .asciz "BEQ rs2=t6(x31)"
str_br_fwd8:    .asciz "BEQ forward +8 (bit3)"
str_br_fwd16:   .asciz "BEQ forward +16 (bit4)"
str_br_fwd32:   .asciz "BEQ forward +32 (bit5)"
str_br_fwd64:   .asciz "BEQ forward +64 (bit6)"
str_br_fwd128:  .asciz "BEQ forward +128 (bit7)"
str_br_fwd256:  .asciz "BEQ forward +256 (bit8)"
str_br_fwd512:  .asciz "BEQ forward +512 (bit9)"
str_br_fwd1024: .asciz "BEQ forward +1024 (bit10)"
str_br_fwd2048: .asciz "BEQ forward +2048 (bit11)"
str_br_fwdmax:  .asciz "BEQ forward +4092 (near max)"
str_br_bwd8:    .asciz "BEQ backward -8 (bit3)"
str_br_bwd16:   .asciz "BEQ backward -16 (bit4)"
str_br_bwd32:   .asciz "BEQ backward -32 (bit5)"
str_br_bwd64:   .asciz "BEQ backward -64 (bit6)"
str_br_bwd128:  .asciz "BEQ backward -128 (bit7)"
str_br_bwd256:  .asciz "BEQ backward -256 (bit8)"
str_br_bwd512:  .asciz "BEQ backward -512 (bit9)"
str_br_bwd1024: .asciz "BEQ backward -1024 (bit10)"
str_br_bwd2048: .asciz "BEQ backward -2048 (bit11)"
str_br_bwdmax:  .asciz "BEQ backward -4092 (near max)"
str_br_loop:    .asciz "BNE as a loop's backward edge, 5 iterations"
