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

Current total: **949 result lines**, all passing.

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

RV64I's load instructions, in `base_loads.S`: `LB`, `LH`, `LW`, `LD`,
`LBU`, `LHU`, `LWU`.

Base-ISA instructions have a genuinely different risk profile than
RVC ones, which shapes the coverage differently:

- **No restricted register fields** — `rd` and `rs1` are each a full,
  independent 5-bit field, so there's no 3-bit-field decode risk, but
  a wrong bit in a 5-bit field is still possible, so each of `rd` and
  `rs1` still gets an independent sweep across a representative sample
  of registers (not exhaustively all 32 — the same "representative
  sample, not full sweep" approach used for RVC's full-5-bit fields
  like `C.ADDI`/`C.MV`/`C.JALR`).
- **No scrambled immediate** — the 12-bit offset is one contiguous
  field, so it only needs boundary values (`+2047`/`-2048`) and a
  couple of representative in-between ones, not an exhaustive per-bit
  sweep the way RVC's scattered encodings needed.
- **Sign vs. zero extension is where the real bugs live.** This
  project's own test-writing history includes more than one sign/zero
  mixup (`lw` vs `lwu` used to verify a store, twice), so `LB`/`LH`/`LW`
  and their `U`-suffixed counterparts are tested with the *exact same*
  underlying byte patterns, so an accidental swap between sign- and
  zero-extension is immediately visible as a mismatched expected
  value rather than something that could quietly pass.
- **`rd=x0` is tested once per instruction** — confirms it doesn't
  fault and genuinely discards the loaded value.
- **`rs1=x0` is deliberately not tested** — address `0` is unmapped in
  this memory map (RAM starts at `0x80000000`) and there's no
  load-access-fault handler, so it would hang rather than usefully
  fail.
- **Offsets are not constrained to natural alignment** for the width
  being loaded. The RISC-V base ISA permits (without mandating
  hardware support for) misaligned accesses, and QEMU's TCG emulation
  handles them transparently — this is not guaranteed portable to all
  real hardware; see "Porting" below.

Every mnemonic in this suite is written with `.option norvc` active
for the entire file — not just style, but a functional requirement:
without it, the assembler would happily substitute a compressed
encoding whenever the operand choice happens to allow one (e.g.
`lw s0, 0(s1)` → `c.lw`), silently testing the wrong instruction. This
was verified empirically (an `lw`/`ld` pair with compressible operands
confirmed to stay 4 bytes wide under `.option norvc`) before relying
on it throughout the suite.

A real bug turned up while building this suite, worth knowing about if
you extend it: the `*_RS1` macro family (which sweeps the base
register while storing a fixed test value first) originally used `t0`
as the "value to store" scratch register — but `t0` is *also* one of
the swept `basereg` candidates. When `basereg == t0`, loading the test
value into `t0` destroyed the address that had just been computed
there, and the subsequent store faulted (`mcause 7`, store/AMO access
fault) instead of writing to the intended buffer. Fixed by using `t1`
(not in the sweep list) for the scratch value instead. This is the
same general class of bug as several `a0`/`a1`/`a2` collisions caught
in the RVC suite's history — a fixed helper register colliding with a
register under test in the sweep — just showing up with a different
register pair here.

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
riscv64-linux-gnu-ld -Ttext=0x80000000 --no-dynamic-linker -nostdlib \
    -o rvc_test.elf common.o main_tests.o rvc_tests.o rvc_quadrant0.o rvc_quadrant1.o \
    rvc_quadrant2.o base_tests.o base_loads.o
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
   `rvc_quadrant0/1/2.S` or `base_tests.S` calls into `base_loads.S`.
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

4. **Misaligned loads are assumed not to trap.** `base_loads.S` tests
   offsets like `+4`/`-4` against 8-byte `LD` loads, which QEMU handles
   transparently but real hardware may legitimately fault on (the
   RISC-V base ISA permits, but does not require, misaligned-access
   support). There's no misaligned-access-fault handler here, so on
   hardware that traps, these specific cases would hang rather than
   fail cleanly.

## Files

- `common.S` — reusable boot/UART/reporter harness (suite-agnostic).
- `main_tests.S` — top-level dispatcher (defines `run_tests`).
- `rvc_tests.S` — RVC suite orchestrator (defines `run_rvc_tests`).
- `rvc_quadrant0.S` / `rvc_quadrant1.S` / `rvc_quadrant2.S` — the RVC
  per-instruction test bodies, one file per RVC opcode quadrant.
- `base_tests.S` — base-ISA suite orchestrator (defines `run_base_tests`).
- `base_loads.S` — the base-ISA load instruction test bodies.
- `Makefile` — build/run/disasm/clean targets.
- `rvc_test.bin` — prebuilt flat binary, ready to load at `0x80000000`.
- `rvc_test.elf` — the linked ELF (handy for `objdump -d` / debugging
  with gdb; not itself loadable as the flat image).
