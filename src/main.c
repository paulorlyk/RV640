#include "log.h"

#include "ui.h"
#include "utils.h"

#include "bus.h"
#include "memory.h"
#include "device_tree.h"
#include "cpu/rv.h"
#include "dev/ns16550a.h"
#include "dev/plic.h"
#include "dev/aclint.h"

#include <unistd.h>
#include <getopt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// #define MAX_CYCLES 3000000

#define RAM_BASE      0x80000000
#define ACLINT_BASE   0x2000000
#define PLIC_BASE     0xC000000
#define UART_BASE     0x10000000

static struct {
  const char *kernelFile;
  const char *binFile;
#ifdef CONFIG_DOS
  const char *swapFile;
#endif
} _opts = {};

static struct {
  Bus bus;
  RV_Cpu cpu;
  Memory ram;
  Aclint aclint;
  Plic plic;
  Ns16550a uart0;
} _vm = {};

static cpu_addr_t _buildDeviceTree() {
  DeviceTree dt;

  INFO("Building device tree...");
  if(!dt_init(&dt))
    return 0;

  if(!dt_addMemeory(&dt, RAM_BASE, mem_size(&_vm.ram))) {
    dt_destroy(&dt);
    return 0;
  }

  if(!dt_addAclint(&dt, ACLINT_BASE, aclint_size(&_vm.aclint))) {
    dt_destroy(&dt);
    return 0;
  }

  if(!dt_addPlic(&dt, PLIC_BASE, plic_size(&_vm.plic), PLIC_INTERRUPTS)) {
    dt_destroy(&dt);
    return 0;
  }

  if(!dt_addNs16550a(&dt, UART_BASE, ns16550_size(&_vm.uart0), NS16550A_IRQ)) {
    dt_destroy(&dt);
    return 0;
  }

  if(!dt_finish(&dt)) {
    dt_destroy(&dt);
    return 0;
  }

  const cpu_size_t dtbSize = dt_size(&dt);
  const cpu_addr_t dtbAddress = ALIGN_DOWN(mem_size(&_vm.ram) + RAM_BASE - dtbSize, sizeof(cpu_addr_t));

  INFO("Loading device tree to 0x%" PRI_CPU_PTR " size = %" PRI_CPU_SIZE, dtbAddress, dtbSize);

  bus_write(&_vm.bus, dtbAddress, dt_data(&dt), dtbSize);

  dt_destroy(&dt);

  return dtbAddress;
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

  INFO("Loading Linux image to 0x%" PRI_CPU_PTR, (cpu_addr_t)(text_offset + memBase));

  cpu_addr_t textPtr = text_offset;

  mem_write(mem, textPtr, buf, read);
  textPtr += read;

  for(int n = 0; !feof(f); ++n) {
    read = fread(buf, 1, bufSize, f);

    mem_write(mem, textPtr, buf, read);
    textPtr += read;

    if((n % 256) == 0)
      INFO("Linux image: %" PRI_CPU_SIZE " bytes", textPtr - (cpu_addr_t)text_offset);
  }

  INFO("Loaded Linux image, size = %" PRI_CPU_SIZE, textPtr - (cpu_addr_t)text_offset);

  free(buf);

  fclose(f);

  // First 64 bits of the image are executable: jump to the actual entry point
  return text_offset + memBase;
}

static cpu_addr_t _loadBin(const char* fileName, Memory *mem, cpu_addr_t memBase) {
  INFO("Reading binary image from %s", fileName);

  FILE *f = fopen(fileName, "rb");
  if(!f) {
    ERROR("Unable to open binary image file %s", fileName);
    return 0;
  }

  INFO("Loading BIN to 0x%" PRI_CPU_PTR, memBase);

  const size_t bufSize = 512 * sizeof(cpu_word_t);
  uint8_t *buf = malloc(bufSize);
  if(!buf) {
    ERROR("Failed to allocate read buffer");
    return 0;
  }

  cpu_addr_t ptr = 0;
  for(int n = 0; !feof(f); ++n) {
    const size_t read = fread(buf, 1, bufSize, f);

    mem_write(mem, ptr, buf, read);
    ptr += read;

    if((n % 256) == 0)
      INFO("Binary image: %" PRI_CPU_SIZE " bytes", ptr);
  }

  free(buf);

  fclose(f);

  return memBase;
}

static bool _parseArgs(int argc, char* argv[]) {
  static const struct option options[] = {
    { .name = "kernel", .has_arg = required_argument, .flag = 0, .val = 'k' },
    { .name = "bin",    .has_arg = required_argument, .flag = 0, .val = 'b' },
#ifdef CONFIG_DOS
    { .name = "swap",   .has_arg = required_argument, .flag = 0, .val = 's' },
#endif
    {0},
  };
  for(int opt; (opt = getopt_long(argc, argv, "k:b:s:", options, NULL)) != -1;) {
    switch(opt) {
      case 'k': {
        _opts.kernelFile = optarg;
        break;
      }

      case 'b': {
        _opts.binFile = optarg;
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

  if(_opts.kernelFile) {
    if(_opts.binFile) {
      ERROR("Either --kernel or --bin, not both");
      return false;
    }
  } else if(!_opts.binFile) {
    ERROR("Either --kernel or --bin is required");
    return false;
  }

#ifdef CONFIG_DOS
  if(!_opts.swapFile) {
    ERROR("--swap is required");
    return false;
  }
#endif

  return true;
}

static void _usage(const char* exe) {
  INFO("Usage: %s [OPTIONS]", exe);
  INFO("Options:");
  INFO("    -k|--kernel Image - kernel image");
  INFO("    -b|--bin image.bin - binary image. Loaded to 0x%" PRI_CPU_PTR, (cpu_addr_t)RAM_BASE);
#ifdef CONFIG_DOS
  INFO("    -s|--swap ram.swp - location of the swap file");
#endif
}

static void _vmDestroy() {
  rv_destroy(&_vm.cpu);
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

  if(!rv_init(&_vm.cpu, &_vm.bus)) {
    _vmDestroy();
    return 1;
  }

  static RV_Cpu *harts[] = { &_vm.cpu };

#ifdef CONFIG_DOS
  if(!mem_init(&_vm.ram, 32LL * 1024 * 1024, _opts.swapFile)) {
#else
  if(!mem_init(&_vm.ram, 32LL * 1024 * 1024)) {
#endif
    _vmDestroy();
    return 1;
  }

  if(!bus_register(&_vm.bus, RAM_BASE, mem_device(&_vm.ram))) {
    _vmDestroy();
    return 1;
  }

  if(!aclint_init(&_vm.aclint, harts) || !bus_register(&_vm.bus, ACLINT_BASE, aclint_device(&_vm.aclint))) {
    _vmDestroy();
    return 1;
  }

  if(!plic_init(&_vm.plic, harts) || !bus_register(&_vm.bus, PLIC_BASE, plic_device(&_vm.plic))) {
    _vmDestroy();
    return 1;
  }

  if(!ns16550a_init(&_vm.uart0, &_vm.plic) || !bus_register(&_vm.bus, UART_BASE, ns16550_device(&_vm.uart0))) {
    _vmDestroy();
    return 1;
  }

  cpu_addr_t entryPoint = 0;
  if(_opts.kernelFile) {
    const cpu_addr_t dtbAddress = _buildDeviceTree();
    if(!dtbAddress) {
      _vmDestroy();
      return 1;
    }

    entryPoint = _loadLinuxImage(_opts.kernelFile, &_vm.ram, RAM_BASE);

    rv_pokeRx(&_vm.cpu, CPU_REG_A0, 0);  // hart id
    rv_pokeRx(&_vm.cpu, CPU_REG_A1, dtbAddress);
  } else {
    entryPoint = _loadBin(_opts.binFile, &_vm.ram, RAM_BASE);
  }
  if(!entryPoint) {
    _vmDestroy();
    return 1;
  }

  INFO("Resetting the CPU...");
  rv_reset(&_vm.cpu, entryPoint);

  uint64_t runtime_ms = 0;
  clock_t ts = clock();

  ui_cps(0);
  ui_time(runtime_ms);
  ui_idle(false);

  for(long int cyclesAcc = 0;;) {
#ifdef MAX_CYCLES
    if(aclint_mtime(&_vm.aclint) >= MAX_CYCLES) {
      const clock_t now = clock();
      const clock_t dur = now - ts;
      runtime_ms += dur / (CLOCKS_PER_SEC / 1000);
      break;
    }
#endif

    const int cyclesPerStep = 100;
    for(int i = 0; i < cyclesPerStep; ++i)
      rv_run(&_vm.cpu);
    aclint_tick(&_vm.aclint, cyclesPerStep);
    cyclesAcc += cyclesPerStep;

    if(rv_isWFI(&_vm.cpu)) {
      const uint64_t aclint_usecs = (ACLINT_FREQENCY / 1000000);

      uint64_t mtimeRemains_us = aclint_mtimeRemains(&_vm.aclint, 0) / aclint_usecs;
      if(mtimeRemains_us > 100000)
        mtimeRemains_us = 100000;

      ui_idle(true);
      usleep(mtimeRemains_us);
      ui_idle(false);

      aclint_tick(&_vm.aclint, mtimeRemains_us * aclint_usecs);
    }

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
  DEBUG("CPU executed %" PRIu64 " cycles in %lld.%03lld sec PC: %" PRI_CPU_PTR, aclint_mtime(&_vm.aclint), runtime_ms / 1000ULL, runtime_ms % 1000ULL, rv_peekPC(&_vm.cpu));
#endif

  _vmDestroy();
  return 0;
}
