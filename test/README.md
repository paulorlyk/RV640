# RV64 instruction-set exerciser

A bare-metal, M-mode RISC-V test firmware that exercises RV64
instructions and reports PASS/FAIL for each case over an ns16550a
serial port. Two instruction-set suites are included so far — the
RV64C ("C", compressed) extension, and the RV64I base-ISA load
instructions — sharing one common boot/UART/reporting harness and one
running pass/fail total.

## Architecture

- **`common.S`** — the reusable harness. Knows nothing about what's
  being tested. Provides: the reset vector / M-mode entry at
  `0x80000000`, `gp`/`sp`/`mtvec` setup, a minimal trap handler
  (expects only `EBREAK`, reports and hangs on anything else), the
  ns16550a UART driver (9600 8N1 init, `putc`/`puts`, hex/decimal
  printing), the `check` pass/fail comparator (prints a verdict and
  tracks running `pass_count`/`fail_count` totals), and the final
  summary/halt. It calls a single symbol, `run_tests`, and otherwise
  doesn't know or care how many suites exist or what they test.
- **`main_tests.S`** — the top-level dispatcher. Defines `run_tests`
  and just calls each suite's own entry point in turn
  (`run_rvc_tests`, `run_base_tests`). This is the file you touch to
  add a new suite (see "Adding another test suite" below).
- **`rvc_tests.S`** + **`rvc_quadrant0/1/2.S`** — the RV64C suite. See
  "The RVC suite" below.
- **`base_tests.S`** + **`base_loads.S`** — the RV64I base-ISA suite.
  See "The base-ISA suite" below.

It has been built and run for real (not just hand-checked) with:
- `binutils-riscv64-linux-gnu` (assembler/linker/objdump) to confirm
  every RVC instruction assembles to its real 2-byte encoding, and
  every base-ISA instruction stays a genuine 4-byte encoding (not
  silently substituted for a compressed form — see "The base-ISA
  suite" below for why that's a real risk worth guarding against).
- `qemu-system-riscv64 -M virt -bios none` to actually execute it — the
  QEMU `virt` machine happens to match this program's assumed memory
  map almost exactly (RAM at `0x80000000`, ns16550a at `0x10000000`,
  boots straight into M-mode when `-bios none` is passed), so it's a
  convenient way to sanity-check the binary before trying it on real
  hardware or another simulator.

Current total: **1367 result lines**, all passing.

## The RVC suite

All 33 RV64C integer instructions, split across four files:

| File | Instructions |
|---|---|
| `rvc_tests.S` | orchestrator; defines `run_rvc_tests`, prints the banner, calls each quadrant in turn |
| `rvc_quadrant0.S` | `C.ADDI4SPN`, `C.LW`, `C.LD`, `C.SW`, `C.SD` |
| `rvc_quadrant1.S` | `C.NOP`, `C.ADDI`, `C.ADDIW`, `C.LI`, `C.ADDI16SP`, `C.LUI`, `C.SRLI`, `C.SRAI`, `C.ANDI`, `C.SUB`, `C.XOR`, `C.OR`, `C.AND`, `C.SUBW`, `C.ADDW`, `C.J`, `C.BEQZ`, `C.BNEZ` |
| `rvc_quadrant2.S` | `C.SLLI`, `C.LWSP`, `C.LDSP`, `C.JR`, `C.MV`, `C.EBREAK`, `C.JALR`, `C.ADD`, `C.SWSP`, `C.SDSP` |

The quadrant grouping matches the RVC spec's own opcode layout (bits
`[1:0]` of the 16-bit instruction select the quadrant). Each quadrant
file exposes exactly one symbol, `tests_quadrantN`, to the outside —
its macros and per-instruction `test_c_*` subroutines stay local. All
three only depend on `check`/`uart_puts` from `common.S` (plus
`word_buf`, defined in `rvc_quadrant0.S` and used by one test in
`rvc_quadrant1.S` — the one cross-quadrant reference in the suite).

**Not covered:** `C.FLD`/`C.FSD`/`C.FLDSP`/`C.FSDSP` (require the `D`
floating-point extension) and `C.FLW`/`C.FSW` (RV32FC-only, don't
exist in RV64C). `C.JAL` is RV32C-only — on RV64C that encoding is
`C.ADDIW`, which *is* tested. `C.UNIMP` is an intentionally-illegal
all-zero bit pattern, not an instruction to execute.

Every instruction gets comprehensive, edge-case-driven coverage:

- **Every legal register** in each restricted field, to catch a wrong
  bit in the 3-bit register decode — including register-aliasing cases
  (e.g. `rd'==rs1'` for loads, `rs1'==rs2'` for stores, `rd'==rs2'` for
  the CA-format ALU ops) with their own, often distinct, expected-value
  logic rather than being skipped.
- **Every individual bit** of the (often non-contiguous/scrambled)
  immediate field in isolation, plus the min/max boundary values and a
  couple of alternating-bit patterns, to catch cross-wiring between
  encoding fields that a single "round number" test would miss.
- **Data-pattern sweeps**, to verify sign extension is applied exactly
  when it should be and not otherwise (`C.LW`, `C.ADDIW`, `C.SUBW`,
  `C.ADDW` all sign-extend a 32-bit result; `C.LD`/`C.SD` don't need
  to, since they're already 64-bit end to end).
- **Garbage-upper-bits handling** for the `W`-suffixed 32-bit ops —
  a register pre-loaded with a nonzero, distinctive upper 32 bits,
  confirming the result depends only on the low 32 bits of each
  operand, not on what else was sitting in the register.
- **Documented invariants**, e.g. "`C.ADDI4SPN` must not modify `sp`
  itself", "`C.LW`/`C.LD` are pure reads — memory and the base register
  are both unchanged afterward", or "`C.SW`/`C.SD` touch exactly the
  target word/doubleword and nothing adjacent."
- **Control-flow correctness**, for the branch/jump instructions
  (`C.J`, `C.BEQZ`, `C.BNEZ`, `C.JR`, `C.JALR`): since their "immediate"
  is a code distance rather than a literal value, coverage means
  walking individual offset-field bits via deliberately constructed
  jump distances (built with `.rept`-generated filler instructions,
  every one confirmed correct via `objdump` rather than trusted from
  hand arithmetic — see the note on assembler relaxation below), plus
  instruction-specific invariants like `C.JR` not linking `ra` (the
  one property that actually distinguishes it from `C.JALR`) and
  `C.JALR`'s link address landing on exactly the right byte.

Each instruction's test subroutine (`test_c_addi4spn`, `test_c_lw`,
etc.) is built from a small set of reusable macros local to that
subroutine (`T_A4SPN_REG`/`T_A4SPN_IMM`, `T_LW_RD`/`T_LW_RS1`/
`T_LW_OFF`/`T_LW_VAL`, and so on) — the template to follow when
expanding an instruction that doesn't have this treatment yet. A few
macros are shared across related instructions within the same
quadrant file where the arithmetic genuinely overlaps (e.g. `T_CI_RI`
for `C.ADDI`/`C.ADDIW`, `T_CA_RS1`/`T_CA_RS2`/`T_CA_ALIAS` for
`C.SUB`/`C.SUBW`/`C.ADDW`), passing the instruction mnemonic itself as
a macro argument — GNU `as` macro parameters are plain text
substitution, so this works even though it's substituting an opcode,
not just a register or immediate.

Every `c.*` mnemonic is written explicitly (wrapped in
`.option rvc` / `.option norvc`) so the assembler is forced to emit
exactly that compressed encoding, and will fail the build if the
chosen registers/immediate don't fit the format.

## The base-ISA suite

RV64I's load and store instructions:
- **`base_loads.S`**: `LB`, `LH`, `LW`, `LD`, `LBU`, `LHU`, `LWU`
- **`base_stores.S`**: `SB`, `SH`, `SW`, `SD`
- **`base_lui.S`**: `LUI`
- **`base_auipc.S`**: `AUIPC`
- **`base_jal.S`**: `JAL`
- **`base_jalr.S`**: `JALR`
- **`base_branches.S`**: `BEQ`, `BNE`, `BLT`, `BGE`, `BLTU`, `BGEU`
- **`base_op_alu.S`**: `ADD`, `SUB`, `SLL`, `SLT`, `SLTU`, `XOR`, `SRL`, `SRA`, `OR`, `AND`

Base-ISA instructions have a genuinely different risk profile than
RVC ones, which shapes the coverage differently:

- **No restricted register fields** — `rd`/`rs1`/`rs2` are each a full,
  independent 5-bit field, so there's no 3-bit-field decode risk, but
  a wrong bit in a 5-bit field is still possible, so each register
  operand still gets an independent sweep across a representative
  sample of registers (not exhaustively all 32 — the same
  "representative sample, not full sweep" approach used for RVC's
  full-5-bit fields like `C.ADDI`/`C.MV`/`C.JALR`).
- **No scrambled immediate** — the 12-bit offset is one contiguous
  field, so it only needs boundary values (`+2047`/`-2048`) and a
  couple of representative in-between ones, not an exhaustive per-bit
  sweep the way RVC's scattered encodings needed.
- **Sign vs. zero extension (loads) / discarded upper bits (stores)
  is where the real bugs live.** This project's own test-writing
  history includes more than one sign/zero mixup (`lw` vs `lwu` used
  to verify a store, twice), so `LB`/`LH`/`LW` and their `U`-suffixed
  counterparts are tested with the *exact same* underlying byte
  patterns, so an accidental swap between sign- and zero-extension is
  immediately visible as a mismatched expected value rather than
  something that could quietly pass. Stores have a direct analog even
  though there's no sign-extension question for a *write*: `SB`/`SH`/
  `SW` must use only the low 8/16/32 bits of `rs2` and discard
  whatever garbage is sitting above that, which each gets an explicit
  test for (a source register loaded with distinctive nonzero upper
  bits, confirming only the intended low bits land in memory).
- **`rd=x0` (loads) / `rs2=x0` (stores) is tested once per
  instruction** — for loads, confirms it doesn't fault and genuinely
  discards the loaded value; for stores, confirms storing the
  always-zero register (a common real pattern) actually writes zero.
- **`rs1=x0` is deliberately not tested** — address `0` is unmapped in
  this memory map (RAM starts at `0x80000000`) and there's no
  access-fault handler, so it would hang rather than usefully fail.
- **Offsets are not constrained to natural alignment** for the width
  being loaded/stored. The RISC-V base ISA permits (without mandating
  hardware support for) misaligned accesses, and QEMU's TCG emulation
  handles them transparently — this is not guaranteed portable to all
  real hardware; see "Porting" below.
- **Stores additionally get an adjacent-memory-untouched invariant**
  (sentinel doublewords on both sides of the target, confirming the
  store touches exactly its width and nothing else) and `rs1`/`rs2`
  preservation checks (a store never writes back to either operand
  register) — the same treatment `C.SW`/`C.SD` got in the RVC suite.
- **`LUI` is the one place `rd=sp` gets tested**, since that's the
  actual behavioral difference between it and its compressed cousin
  `C.LUI` (which reserves `rd=x2`/`sp` for `C.ADDI16SP` instead — LUI
  has no such restriction). Handled carefully so `sp` never holds a
  non-stack-pointer value across a subroutine call: poison, execute,
  capture the result, restore the real `sp`, and only then call into
  `check()`. `LUI`'s 20-bit immediate is also a plain, non-scrambled
  field like the load/store offsets, so it gets the same "boundary
  values plus a representative sample" treatment rather than an
  exhaustive per-bit sweep — including both sides of the sign bit
  (bit 19 of the 20-bit field), which determines whether the result
  sign-extends.
- **`AUIPC` shares `LUI`'s immediate encoding exactly, but the result
  is PC-relative** (`rd = pc + sign_extend(imm20 << 12)`, where `pc`
  is the AUIPC instruction's own address) — which means, unlike every
  other instruction in this suite, the expected value for a given test
  case isn't known until the code is actually linked. Every AUIPC test
  computes its own expectation at *runtime* instead of hardcoding one:
  a local label placed exactly at the AUIPC instruction gives its true
  address via `la`, and the immediate's contribution (the same
  sign-extended delta `LUI` would produce for the same value) is added
  to that. There's also a test that doesn't depend on knowing any
  absolute address at all: two AUIPCs with the *same* immediate at two
  different code locations must differ by exactly the byte distance
  between them, since the immediate's contribution cancels out — this
  is the one test that would actually catch an implementation that
  computed AUIPC as if it were LUI, ignoring `pc` entirely (both
  results would come out identical instead of differing by the
  inter-instruction distance).
- **`JAL` combines two dimensions the RVC suite tested separately**: a
  PC-relative jump offset (like `C.J`) and a link register (like
  `C.JALR`, except `JAL`'s `rd` is a full 5-bit field — any register,
  not fixed to `ra`). Each register in the `rd` sweep is checked two
  ways independently: did execution land at the target, and does `rd`
  hold exactly this `JAL`'s own address + 4. `rd=x0` — the standard
  "plain unconditional jump" idiom — is tested as a first-class case
  rather than a degenerate one. Unlike the RVC branch/jump
  instructions, there's no assembler-relaxation boundary to worry
  about here: `JAL` has no RV64 compressed form at all (`C.JAL` is
  RV32C-only; RV64C reuses that opcode slot for `C.ADDIW`), so every
  `JAL` is unconditionally 4 bytes with nothing smaller the assembler
  could have substituted.

  **Scope note:** `JAL`'s 20-bit offset spans roughly ±1MB. Walking
  every bit of it exhaustively — the treatment `C.J` got — would mean
  constructing filler runs up to ~512KB for the top bit alone, a bad
  trade for a test binary meant to build and run quickly. So this file
  sweeps every bit of the low 12 (magnitudes 4–4096 forward, 8–4096
  backward; backward's bookkeeping overhead means `-4` isn't
  constructible the way `+4` is), plus one dedicated test for the
  low-order bit a run of 4-byte fillers can never reach on its own
  (offset ≡ 2 mod 4, needing a single inert 2-byte `c.nop` as padding
  — not under test itself). The higher-order bits use the identical
  encoding mechanism, just at a scale not worth the binary size.
- **`JALR` is register-relative, not PC-relative**, which changes the
  coverage in three ways that make it more than a copy of `JAL`'s
  structure. Its 12-bit immediate is a plain contiguous field and the
  target is computed from a register, so the *full* −2048..+2047 range
  is cheap to sweep at the boundaries — no enormous filler runs needed
  (each case computes its base as `target − imm` at runtime, so the
  jump lands on the label whatever the immediate). The low bit of the
  computed target is **cleared** (`& ~1`), not preserved, which is easy
  to get wrong and is tested directly from both directions: a base
  register set to `(label | 1)`, and an odd bit arriving via the
  immediate instead. And `rd == rs1` is a genuine hazard — the old
  `rs1` must be read as the target *before* `rd` is overwritten with
  the link address, or the jump goes to the link address instead — so
  that aliasing case gets its own landed-and-link pair of checks.
  There's also a real call/return round trip using `JALR` in both
  roles. `.option norvc` is genuinely load-bearing in this file rather
  than just conventional: `JALR` *does* have compressed forms
  (`C.JR`/`C.JALR`) that the assembler would otherwise substitute for
  the `imm=0` cases, silently testing the RVC instruction instead of
  this one — verified by disassembly that zero compressed forms leaked
  in. The three-operand `jalr rd, rs1, imm` form is used throughout
  rather than the `ret`/`jr` pseudo-instructions, so what's under test
  is unambiguous.
- **The six branches share one B-type encoding** and differ only in
  the comparison in `funct3`, so the coverage is organised around what
  actually distinguishes them rather than repeating an identical
  offset sweep six times. All six are run against the *same* set of
  operand pairs with the expected taken/not-taken outcome spelled out
  for each, so a decoder that swaps two of them (`BLT` for `BGE`, or
  `BLTU` for `BLT`) produces a visible mismatch instead of quietly
  passing. The headline cases are the **signed/unsigned divergences**:
  `BLT`/`BGE` compare as signed, `BLTU`/`BGEU` compare the same bits
  as unsigned, so operands like `rs1=-1, rs2=1` give opposite answers
  — signed, `-1 < 1` so `BLT` is taken; unsigned, `0xffff...f > 1` so
  `BLTU` is *not*. `INT64_MIN` vs `INT64_MAX` and `-1` vs `0` are the
  other such pairs. These are precisely what catches a signed/unsigned
  mixup, the bug class that has bitten this project's own test code
  more than once. Equal operands are covered for all six too, since
  that's where the strict/non-strict split shows up (`BLT` not taken
  vs `BGE` taken on `x == x`). The B-type offset field is identical
  across all six, so it's swept once through `BEQ` rather than six
  times over. `.option norvc` is load-bearing here as well: `BEQ`/`BNE`
  against `x0` with an `x8`-`x15` register would otherwise be
  substituted with `C.BEQZ`/`C.BNEZ` — verified by disassembly that
  zero compressed forms leaked in.
- **`base_op_alu.S`'s ten R-type ALU instructions share one encoding**
  and differ only in the operation, so — the same idea as the branch
  file — the register fields (`rd`/`rs1`/`rs2` and every aliasing
  combination) are swept *once*, through `ADD`, rather than ten times
  over; the real per-instruction depth goes into each operation's own
  correctness. `SLL`/`SRL`/`SRA` get a specific, easy-to-get-wrong
  check: only the low 6 bits of `rs2` select the shift amount, so
  `rs2=64` must behave identically to `rs2=0`, and `rs2=65` to `rs2=1`
  — tested directly rather than assumed. `SLT`/`SLTU` get the same
  signed/unsigned divergence pairs used for `BLT`/`BLTU`, for the same
  reason. `.option norvc` matters here too — several of these have
  direct compressed equivalents among `x8`-`x15` registers that the
  assembler would otherwise substitute.

Every mnemonic in this suite is written with `.option norvc` active
for the entire file — not just style, but a functional requirement:
without it, the assembler would happily substitute a compressed
encoding whenever the operand choice happens to allow one (e.g.
`lw s0, 0(s1)` → `c.lw`), silently testing the wrong instruction. This
was verified empirically (an `lw`/`ld` pair with compressible operands
confirmed to stay 4 bytes wide under `.option norvc`) before relying
on it throughout the suite.

Two real bugs turned up while building this suite, both the same
underlying mistake in different clothes, worth knowing about if you
extend it further: a macro that sweeps register X while using a fixed
*other* register Y as scratch is broken if Y ever appears as one of
the values X is swept across.

- In `base_loads.S`'s `*_RS1` macro family (sweeps the base register
  while storing a fixed test value first), the scratch value register
  was originally `t0` — but `t0` is also one of the swept `basereg`
  candidates. When `basereg == t0`, loading the test value into `t0`
  destroyed the address just computed there, and the store faulted
  (`mcause 7`, store/AMO access fault). Fixed by moving the scratch to
  `t1` (not in the sweep list).
- In `base_stores.S`'s `*_RS2` macro family (sweeps the value register
  while using a fixed base address), the fixed base was originally
  `s0` — but `s0` is also one of the swept `rs2reg` candidates. Same
  fault, same fix shape: moved the fixed base to `s1` (not in the
  sweep list).

Both were caught the same way: an actual QEMU run hit an unexpected
trap, not a code review. If you write a similar sweep-plus-fixed-helper
macro, double check the helper register never collides with anything
in the corresponding call sites' register list — this class of bug has
now shown up three times across this project (the third instance, with
`a0`/`a1`/`a2` colliding with swept registers, is documented in the
RVC suite's own history in earlier revisions of this file).

## Building

```sh
make            # produces rvc_test.bin
```

or manually:

```sh
riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 -o common.o common.S
riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 -o main_tests.o main_tests.S
riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 -o rvc_tests.o rvc_tests.S
riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 -o rvc_quadrant0.o rvc_quadrant0.S
riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 -o rvc_quadrant1.o rvc_quadrant1.S
riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 -o rvc_quadrant2.o rvc_quadrant2.S
riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 -o base_tests.o base_tests.S
riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 -o base_loads.o base_loads.S
riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 -o base_stores.o base_stores.S
riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 -o base_lui.o base_lui.S
riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 -o base_auipc.o base_auipc.S
riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 -o base_jal.o base_jal.S
riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 -o base_jalr.o base_jalr.S
riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 -o base_branches.o base_branches.S
riscv64-linux-gnu-as -march=rv64imac_zicsr_zifencei -mabi=lp64 -o base_op_alu.o base_op_alu.S
riscv64-linux-gnu-ld -Ttext=0x80000000 --no-dynamic-linker -nostdlib \
    -o rvc_test.elf common.o main_tests.o rvc_tests.o rvc_quadrant0.o rvc_quadrant1.o \
    rvc_quadrant2.o base_tests.o base_loads.o base_stores.o base_lui.o base_auipc.o base_jal.o base_jalr.o base_branches.o base_op_alu.o
riscv64-linux-gnu-objcopy -O binary rvc_test.elf rvc_test.bin
```

`common.o` must be listed first at link time — it contains `_start`,
and needs to land at the very base of `.text` so the entry point ends
up at the `0x80000000` load address. The rest can be in any order
relative to each other.

`rvc_test.bin` is the flat binary — load it into RAM at `0x80000000`
and reset the hart with `pc = 0x80000000`, `mode = M`.

## Running under QEMU (for a quick check)

```sh
make run
# or:
qemu-system-riscv64 -M virt -bios none -kernel rvc_test.elf \
    -nographic -serial mon:stdio
```
(`Ctrl-A X` to exit QEMU.) `-kernel` with an ELF works fine here since
QEMU just loads the ELF's segments and jumps to its entry point, which
is the same `0x80000000` load address as the flat `.bin`.

## Adding another test suite

`common.S` doesn't know or care what it's testing — it just calls
`run_tests` (in `main_tests.S`) and reads `pass_count`/`fail_count`
afterward. `main_tests.S` in turn just calls each suite's own entry
point. To add a new suite (say, the `M` extension):

1. Write your own top-level file (e.g. `m_tests.S`) with
   `.global run_m_tests`, printing its own banner and calling into one
   or more category files the same way `rvc_tests.S` calls into
   `rvc_quadrant0/1/2.S` or `base_tests.S` calls into `base_loads.S`/
   `base_stores.S`/`base_lui.S`/`base_auipc.S`/`base_jal.S`/`base_jalr.S`/`base_branches.S`/`base_op_alu.S`.
   Use `check`/`uart_puts` from `common.S` the same way the existing
   suites do. Split it into multiple files if it's large enough to
   benefit — each category file should expose exactly one entry symbol
   and keep its macros/subroutines local.
2. Add one line to `main_tests.S`: `jal ra, run_m_tests`.
3. Add your new file(s) to `SUITE_SRCS` in the `Makefile`.

Everything else — boot, UART init, trap handling, pass/fail reporting,
the final summary line — is reused as-is.

## A note on branch/jump boundary tests and assembler relaxation

`C.J`/`C.BEQZ`/`C.BNEZ`'s near-maximum offset test cases are built by
padding the distance between the branch/jump and its target with
filler instructions (`.rept`-generated `c.nop`s, sized to hit a
specific byte count) rather than passing a numeric immediate directly.
This turned out to have a real sharp edge: at the *exact* boundary of
the compressible offset range, GNU `as`'s branch relaxation can
converge to the wrong fixed point in some contexts (confirmed via an
isolated minimal reproduction that assembled correctly, while the same
instruction sequence embedded deeper in a larger function silently
widened to a 4-byte `beq`/`bne` instead of the intended 2-byte
`c.beqz`/`c.bnez` — caught only by disassembling and counting
occurrences, not by anything the assembler warned about). The fix used
throughout is a one-`c.nop` safety margin off the true boundary
(`+252` instead of the theoretical max `+254` for `C.BEQZ`/`C.BNEZ`,
`+2044` instead of `+2046` for `C.J`), with every single offset in
these tests double-checked by disassembling and computing
`target − branch` in Python, not trusted from the `.rept` count alone.
If you add more boundary-offset tests, verify them the same way.

## Porting to other hardware/simulators

Two assumptions are baked into `common.S` and worth checking against
your target:

1. **UART reference clock.** `UART_CLK_HZ` (default `1843200`, the
   classic PC/16550 clock) determines the baud-rate divisor. If your
   UART is driven by a different input clock (many SoCs feed it from
   the peripheral bus clock instead), change that one `.equ` and the
   divisor, `UART_DIV_LO`/`UART_DIV_HI` all update automatically at
   assemble time. If the resulting effective baud rate is visibly
   wrong on a real terminal, this is almost always the reason.
2. **UART register stride is 1 byte.** Some ns16550a integrations use
   a 4-byte stride (word-addressed registers). If yours does, change
   the register offset macros (`UART_IER`, `UART_LCR`, etc.) to be
   multiplied by 4, or add a stride constant.
3. **`gp` (global pointer) is initialized at startup** via
   `la gp, __global_pointer$` before anything else runs. This is
   required because the linker's default relaxation turns nearby
   `la reg, symbol` sequences into a single gp-relative `addi`, and
   `gp` is *not* set by hardware at reset — skipping this step is a
   classic bare-metal bug (it was actually caught during testing here:
   without it, the very first data access faults with a store/AMO
   access fault, mcause 7).

Additionally, for the base-ISA load suite specifically:

4. **Misaligned loads/stores are assumed not to trap.** `base_loads.S`
   and `base_stores.S` test offsets like `+4`/`-4` against 8-byte
   `LD`/`SD` accesses, which QEMU handles transparently but real
   hardware may legitimately fault on (the RISC-V base ISA permits,
   but does not require, misaligned-access support). There's no
   misaligned-access-fault handler here, so on hardware that traps,
   these specific cases would hang rather than fail cleanly.

## Files

- `common.S` — reusable boot/UART/reporter harness (suite-agnostic).
- `main_tests.S` — top-level dispatcher (defines `run_tests`).
- `rvc_tests.S` — RVC suite orchestrator (defines `run_rvc_tests`).
- `rvc_quadrant0.S` / `rvc_quadrant1.S` / `rvc_quadrant2.S` — the RVC
  per-instruction test bodies, one file per RVC opcode quadrant.
- `base_tests.S` — base-ISA suite orchestrator (defines `run_base_tests`).
- `base_loads.S` — the base-ISA load instruction test bodies.
- `base_stores.S` — the base-ISA store instruction test bodies.
- `base_lui.S` — the `LUI` test body.
- `base_auipc.S` — the `AUIPC` test body.
- `base_jal.S` — the `JAL` test body.
- `base_jalr.S` — the `JALR` test body.
- `base_branches.S` — the conditional-branch test bodies.
- `base_op_alu.S` — the R-type ALU test bodies.
- `Makefile` — build/run/disasm/clean targets.
- `rvc_test.bin` — prebuilt flat binary, ready to load at `0x80000000`.
- `rvc_test.elf` — the linked ELF (handy for `objdump -d` / debugging
  with gdb; not itself loadable as the flat image).
