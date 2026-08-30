#include "log.h"

#include "ui.h"
#include "utils.h"

#include "bus.h"
#include "memory.h"
#include "cpu/rv64.h"
#include "dev/ns16550a.h"
#include "dev/plic.h"
#include "dev/aclint.h"

#include <unistd.h>
#include <getopt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// #define MAX_CYCLES 1000000

static struct {
  const char *dtbFile;
  const char *kernelFile;
#ifdef CONFIG_DOS
  const char *swapFile;
#endif
} _opts;

static struct {
  Bus bus;
  RV64_Cpu cpu;
  Memory ram;
  Aclint aclint;
  Plic plic;
  Ns16550a uart0;
} _vm;

static cpu_addr_t _loadDTB(const char* fileName, Memory *mem, cpu_addr_t memBase) {
  INFO("Reading DTB from %s", fileName);

  FILE *f = fopen(fileName, "rb");
  if(!f) {
    ERROR("Unable to open DTB file %s", fileName);
    return 0;
  }

  fseek(f, 0, SEEK_END);
  const long int dtbSize = ftell(f);
  fseek(f, 0, SEEK_SET);

  const cpu_addr_t dtbOffset = ALIGN_DOWN(mem_size(mem) - dtbSize, sizeof(cpu_word_t));

  INFO("Loading DTB to 0x%" PRI_CPU_PTR ", size = %ld", dtbOffset + memBase, dtbSize);

  const size_t bufSize = 512 * sizeof(cpu_word_t);
  uint8_t *buf = malloc(bufSize);
  if(!buf) {
    ERROR("Failed to allocate read buffer");
    return 0;
  }

  for(cpu_addr_t dtbPtr = dtbOffset; !feof(f);) {
    const size_t read = fread(buf, 1, bufSize, f);

    mem_write(mem, dtbPtr, buf, read);
    dtbPtr += read;
  }

  free(buf);

  fclose(f);

  return dtbOffset + memBase;
}

static cpu_addr_t _loadLinuxImage(const char* fileName, Memory* mem, cpu_addr_t memBase) {
  INFO("Reading Linux image from %s", fileName);

  FILE *f = fopen(fileName, "rb");
  if(!f) {
    ERROR("Unable to open Linux image file %s", fileName);
    return 0;
  }

  // Linux header must fit into the buffer.
  const size_t bufSize = 512 * sizeof(cpu_word_t);
  uint8_t *buf = malloc(bufSize);
  if(!buf) {
    ERROR("Failed to allocate read buffer");
    return 0;
  }

  // Image can't be that small
  size_t read = fread(buf, 1, bufSize, f);
  if(read < bufSize) {
    ERROR("Failed to read Linux header");
    fclose(f);
    return 0;
  }

  // See https://www.kernel.org/doc/html/v6.1/riscv/boot-image-header.html
  const uint64_t text_offset = ((uint64_t *)buf)[1];
  const uint64_t magic1 = ((uint64_t *)buf)[6];
  const uint32_t magic2 = ((uint32_t *)buf)[14];

  if(magic1 != 0x5643534952LL || memcmp(&magic2, "RSC\x05", 4) != 0) {
    ERROR("Invalid image type: only RISC-V image supported");
    fclose(f);
    return 0;
  }

  INFO("Loading Linux image to 0x%" PRI_CPU_PTR, text_offset + memBase);

  cpu_addr_t textPtr = text_offset;

  mem_write(mem, textPtr, buf, read);
  textPtr += read;

  for(int n = 0; !feof(f); ++n) {
    read = fread(buf, 1, bufSize, f);

    mem_write(mem, textPtr, buf, read);
    textPtr += read;

    if((n % 256) == 0)
      INFO("Linux image: %" PRI_CPU_SIZE " bytes", textPtr - text_offset);
  }

  INFO("Loaded Linux image, size = %" PRI_CPU_SIZE, textPtr - text_offset);

  free(buf);

  fclose(f);

  // First 64 bits of the image are executable: jump to the actual entry point
  return text_offset + memBase;
}

static bool _parseArgs(int argc, char* argv[]) {
  static const struct option options[] = {
    { .name = "dtb",    .has_arg = required_argument, .flag = 0, .val = 'd'},
    { .name = "kernel", .has_arg = required_argument, .flag = 0, .val = 'k'},
#ifdef CONFIG_DOS
    { .name = "swap", .has_arg = required_argument,   .flag = 0, .val = 's' },
#endif
    {0},
  };
  for(int opt; (opt = getopt_long(argc, argv, "d:k:s:", options, NULL)) != -1;) {
    switch(opt) {
      case 'd': {
        _opts.dtbFile = optarg;
        break;
      }

      case 'k': {
        _opts.kernelFile = optarg;
        break;
      }

#ifdef CONFIG_DOS
      case 's': {
        _opts.swapFile = optarg;
        break;
      }
#endif

      default:
        ERROR("Unknown argument");
        return false;
    }
  }

  if(
    !_opts.dtbFile
    || !_opts.kernelFile
#ifdef CONFIG_DOS
    || !_opts.swapFile
#endif
    ) {
    ERROR("Missing file argument");
    return false;
  }

  return true;
}

static void _usage(const char* exe) {
  INFO("Usage: %s\n"
    "    -d|--dtb file.dtb\n"
    "    -k|--kernel Image\n"
#ifdef CONFIG_DOS
    "    -s|--swap ram.swp\n"
#endif
    "",
    exe);
}

static void _vmDestroy() {
  rv64_destroy(&_vm.cpu);
  bus_destroy(&_vm.bus);
  mem_destroy(&_vm.ram);
  ns16550a_destroy(&_vm.uart0);
  plic_destroy(&_vm.plic);
  aclint_destroy(&_vm.aclint);

  ui_destroy();
}

int main(int argc, char* argv[]) {
  ui_init();

  if(!_parseArgs(argc, argv)) {
    _usage(argv[0]);
    _vmDestroy();
    return 1;
  }

  if(!bus_init(&_vm.bus)) {
    _vmDestroy();
    return 1;
  }

  if(!rv64_init(&_vm.cpu, &_vm.bus)) {
    _vmDestroy();
    return 1;
  }

  static RV64_Cpu *harts[] = { &_vm.cpu };

#ifdef CONFIG_DOS
  if(!mem_init(&_vm.ram, 32LL * 1024 * 1024, _opts.swapFile)) {
#else
  if(!mem_init(&_vm.ram, 32LL * 1024 * 1024)) {
#endif
    _vmDestroy();
    return 1;
  }

  const cpu_addr_t ramBase = 0x80000000;

  const cpu_addr_t dtbAddress = _loadDTB(_opts.dtbFile, &_vm.ram, ramBase);
  if(!dtbAddress) {
    _vmDestroy();
    return 1;
  }

  const cpu_addr_t kernelEntryPoint = _loadLinuxImage(_opts.kernelFile, &_vm.ram, ramBase);
  if(!kernelEntryPoint) {
    _vmDestroy();
    return 1;
  }

  if(!bus_register(&_vm.bus, ramBase, mem_device(&_vm.ram))) {
    _vmDestroy();
    return 1;
  }

  if(!aclint_init(&_vm.aclint, harts) || !bus_register(&_vm.bus, 0x2000000, aclint_device(&_vm.aclint))) {
    _vmDestroy();
    return 1;
  }

  if(!plic_init(&_vm.plic, harts) || !bus_register(&_vm.bus, 0xC000000, plic_device(&_vm.plic))) {
    _vmDestroy();
    return 1;
  }

  if(!ns16550a_init(&_vm.uart0, &_vm.plic) || !bus_register(&_vm.bus, 0x10000000, ns16550_device(&_vm.uart0))) {
    _vmDestroy();
    return 1;
  }

  INFO("Resetting the CPU...");
  rv64_reset(&_vm.cpu, kernelEntryPoint);
  rv64_setRx(&_vm.cpu, CPU_REG_A0, 0);  // hart id
  rv64_setRx(&_vm.cpu, CPU_REG_A1, dtbAddress);

  uint64_t runtime_ms = 0;
  clock_t ts = clock();

  ui_cps(0);
  ui_time(runtime_ms);

  for(long int cyclesAcc = 0;;) {
#ifdef MAX_CYCLES
    if(aclint_mtime(&_vm.aclint) >= MAX_CYCLES) {
      const clock_t now = clock();
      const clock_t dur = now - ts;
      runtime_ms += dur / (CLOCKS_PER_SEC / 1000);
      break;
    }
#endif

    const int cyclesPerStep = 64;

    for(int i = 0; i < cyclesPerStep; ++i) {
      if(rv64_isWFI(&_vm.cpu))
        break;

      rv64_run(&_vm.cpu);
    }
    aclint_tick(&_vm.aclint, cyclesPerStep);
    cyclesAcc += cyclesPerStep;

    if(cyclesAcc > 5000) {
      const clock_t now = clock();
      const clock_t dur = now - ts;
      if(dur >= CLOCKS_PER_SEC) {
        runtime_ms += dur / (CLOCKS_PER_SEC / 1000);

        ui_cps((cyclesAcc * CLOCKS_PER_SEC) / dur);
        ui_time(runtime_ms);

        ts = now;
        cyclesAcc = 0;
      }
    }

    const char ch = ui_getch();
    if(ch)
      ns16550_push(&_vm.uart0, ch);
  }

#ifdef MAX_CYCLES
  putc('\n', stderr);
  DEBUG("CPU executed %" PRIu64 " cycles in %lld.%03lld sec PC: %" PRI_CPU_PTR, aclint_mtime(&_vm.aclint), runtime_ms / 1000ULL, runtime_ms % 1000ULL, rv64_getPC(&_vm.cpu));
#endif

  _vmDestroy();
  return 0;
}
