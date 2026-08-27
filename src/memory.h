//
// Created by palulukan on 7/24/26.
//

#ifndef MEMORY_H_CF2C5830AD314A74AB2FF51D5723BE8C
#define MEMORY_H_CF2C5830AD314A74AB2FF51D5723BE8C

#include "dev/device.h"

#include <stdbool.h>
#include <stddef.h>

// #define MEMORY_STATS

#ifdef CONFIG_DOS

#include <stdio.h>

#define MEM_PAGES 64

#define MEM_PAGE_ADDR_SHIFT 12U

#define MEM_PAGE_SIZE (1UL << MEM_PAGE_ADDR_SHIFT)

#define MEM_PAGE_OFFSET_MASK (MEM_PAGE_SIZE - 1UL)
#define MEM_PAGE_ADDR_MASK (~MEM_PAGE_OFFSET_MASK)

#define MEM_SWAP_IO_BUF_SIZE 4096

#define MEM_LOOKUP_CACHED_PAGES   4

#endif

typedef struct {
#ifdef CONFIG_DOS
  struct _memPage {
    unsigned long int addr;   // No need to keep long pointers because of file system limitations
    bool dirty;
    char __far* data;
  } pages[MEM_PAGES];
  int evictCtr; // Yes, this is somehow faster than a proper LRU...

#if MEM_LOOKUP_CACHED_PAGES > 0
  struct _memPage *cachedPages[MEM_LOOKUP_CACHED_PAGES];
  int cachedPagesEvictCtr;
#endif

  FILE *swap;
  uint8_t *localPage;

#ifdef MEMORY_STATS
  uint32_t pageLookups;
  uint32_t cacheHits;
  uint32_t cacheMisses;
  uint32_t pageHits;
  uint32_t pageMisses;
  uint32_t pageWrites;
#endif
#else
  char *data;
#endif

  Device dev;
} Memory;

#ifdef CONFIG_DOS
bool mem_init(Memory* self, cpu_size_t size, const char* swapFile);
#else
bool mem_init(Memory* self, cpu_size_t size);
#endif

void mem_destroy(Memory *self);

#define mem_size(self) ((self)->dev.size)
#define mem_device(self) (&(self)->dev)

void mem_read(Memory *self, cpu_addr_t addr, void* buf, size_t size);
void mem_write(Memory *self, cpu_addr_t addr, const void* buf, size_t size);
int mem_cmp(Memory *self, cpu_addr_t addr, const void* buf, size_t size);

#endif //MEMORY_H_CF2C5830AD314A74AB2FF51D5723BE8C
