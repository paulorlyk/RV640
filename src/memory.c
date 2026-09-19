//
// Created by palulukan on 7/24/26.
//

#include "memory.h"

#include "log.h"
#include "ui.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#ifdef CONFIG_DOS

#include <libi86/malloc.h>
#include <libi86/string.h>
#include <dos.h>
#include <sys/fcntl.h>

static inline _dosFileHandle _dosFOpen(const char *name) {
  int handle = -1;

  if(_dos_open(name, O_RDWR, &handle) == 0)
    return handle;

  if(_dos_creat(name, 0, &handle) == 0)
    return handle;

  return -1;
}

static inline void _dosFClose(_dosFileHandle file) {
  _dos_close(file);
}

static inline unsigned int _dosFRead(_dosFileHandle file, __libi86_fpc_t buffer, unsigned int size) {
  unsigned int read = 0;

  _dos_read(file, buffer, size, &read);

  return read;
}

static inline unsigned int _dosFWrite(_dosFileHandle file, const __libi86_fpcc_t buffer, unsigned int size) {
  unsigned int written = 0;

  _dos_write(file, buffer, size, &written);

  return written;
}

#define DOS_SEEK_SET 0
#define DOS_SEEK_CUR 1
#define DOS_SEEK_END 2

static inline int _dosFSeek(_dosFileHandle file, long int offset, int whence) {
  union REGS r;

  r.h.ah = 0x42;
  r.h.al = whence;
  r.x.bx = file;
  r.x.dx = (unsigned int)offset;
  r.x.cx = ((unsigned long int)offset) >> 16;

  int86(0x21, &r, &r);

  if(r.x.cflag)
    return -1;

  return 0;
}

static inline long int _dosFTell(_dosFileHandle file) {
  union REGS r;

  r.h.ah = 0x42;
  r.h.al = DOS_SEEK_CUR;
  r.x.bx = file;
  r.x.dx = 0;
  r.x.cx = 0;

  int86(0x21, &r, &r);

  if(r.x.cflag)
    return -1;

  return (long int)(((unsigned long int)r.x.dx << 16) | (unsigned long int)r.x.ax);
}

static inline void _swapLruHead(Memory* self, struct _memPage *page) {
  if(page->prev)
    page->prev->next = page->next;
  if(page->next)
    page->next->prev = page->prev;

  page->next = self->lruListHead;
  page->prev = NULL;

  self->lruListHead->prev = page;
  self->lruListHead = page;
}

static inline void _storePage(Memory* self, struct _memPage *page) {
#ifdef MEMORY_STATS
  ++self->pageWrites;
#endif

  if(_dosFSeek(self->swap, (long int)page->addr, DOS_SEEK_SET) != 0) {
    ERROR("Mem: Failed to seek swap");
    return;
  }

  if(_dosFWrite(self->swap, page->data, MEM_PAGE_SIZE) != MEM_PAGE_SIZE) {
    ERROR("Mem: Failed to write page to swap");
    return;
  }
}

static inline void _loadPage(Memory* self, unsigned long int pageAddr, struct _memPage *page) {
  if(_dosFSeek(self->swap, (long int)pageAddr, DOS_SEEK_SET) != 0) {
    ERROR("Mem: Failed to seek swap");
    return;
  }

  if(_dosFRead(self->swap, page->data, MEM_PAGE_SIZE) != MEM_PAGE_SIZE) {
    ERROR("Mem: Failed to read page from swap");
    return;
  }

  page->addr = pageAddr;
  page->dirty = false;
}

static inline struct _memPage* _lookupPage(Memory* self, unsigned long int addr) {
#ifdef MEMORY_STATS
  ++self->pageLookups;
#endif

  const unsigned long int pageAddr = addr & MEM_PAGE_ADDR_MASK;

  struct _memPage *page = self->lruListHead;
  for(;;) {
    if(page->addr == pageAddr) {
#ifdef MEMORY_STATS
      ++self->pageHits;
#endif

      if(page != self->lruListHead)
        _swapLruHead(self, page);

      return page;
    }

    if(!page->next)
      break;

    page = page->next;
  }

#ifdef MEMORY_STATS
  ++self->pageMisses;
#endif

  _swapLruHead(self, page);

  if(page->dirty) {
    ui_page_status(UI_PS_WR);
    _storePage(self, page);
  }

  ui_page_status(UI_PS_RD);
  _loadPage(self, pageAddr, page);

  ui_page_status(UI_PS_IDLE);

  return page;
}

#endif

#ifdef CONFIG_DOS
bool mem_init(Memory* self, cpu_size_t size, const char* swapFile)
#else
bool mem_init(Memory* self, cpu_size_t size)
#endif
{
  INFO("Mem: size = %" PRI_CPU_SIZE, size);

#ifdef CONFIG_DOS
  INFO("Mem: Allocating %d x %lu byte pages", MEM_PAGES, MEM_PAGE_SIZE);

  for(int i = 0; i < MEM_PAGES; ++i) {
    self->pages[i].data = _fmalloc(MEM_PAGE_SIZE);
    if(__libi86_FP_EQ_NULL(self->pages[i].data)) {
      ERROR("Failed to allocate memory page");
      mem_destroy(self);
      return false;
    }

    _fmemset(self->pages[i].data, 0, MEM_PAGE_SIZE);
  }

  INFO("Mem: Initializing swap file %s", swapFile);

  self->swap = _dosFOpen(swapFile);
  if(self->swap < 0) {
    ERROR("Failed to create swap file");
    mem_destroy(self);
    return false;
  }

  _dosFSeek(self->swap, 0, DOS_SEEK_END);
  long int swapPos = _dosFTell(self->swap);

  for(int n = 0; swapPos < size; swapPos += MEM_PAGE_SIZE) {
    char buf[MEM_PAGE_SIZE] = {0};

    if(_dosFWrite(self->swap, buf, MEM_PAGE_SIZE) != MEM_PAGE_SIZE) {
      ERROR("Failed to write swap file");
      mem_destroy(self);
      return false;
    }

    if((n++ % 256) == 0)
      INFO("Mem: Swap size: %ld bytes", swapPos);
  }

  _dosFClose(self->swap);
  self->swap = _dosFOpen(swapFile);
  if(self->swap < 0) {
    ERROR("Failed reopen swap file");
    mem_destroy(self);
    return false;
  }

  INFO("Mem: Swap initialized: %ld  bytes", swapPos);

  for(int i = 0; i < MEM_PAGES; ++i) {
    ui_page_status(UI_PS_RD);
    _loadPage(self, MEM_PAGE_SIZE * i, &self->pages[i]);
    ui_page_status(UI_PS_IDLE);

    if(i < MEM_PAGES - 1)
      self->pages[i].next = &self->pages[i + 1];
    if(i > 0)
      self->pages[i].prev = &self->pages[i - 1];
  }
  self->lruListHead = &self->pages[0];
#else
  self->data = calloc(size, 1);
  if(!self->data) {
    ERROR("Failed to allocate memory data");
    return false;
  }
#endif

  self->dev.size = size;
  self->dev.opaque = self;
  self->dev.cbRead = (dev_cb_read)mem_read;
  self->dev.cbWrite = (dev_cb_write)mem_write;

  return true;
}

void mem_destroy(Memory *self) {
#ifdef CONFIG_DOS
  for(int i = 0; i < MEM_PAGES; ++i) {
    if(self->swap)
      _storePage(self, self->pages + i);

    _ffree(self->pages[i].data);
  }

  if(self->swap >= 0)
    _dosFClose(self->swap);

#ifdef MEMORY_STATS
  DEBUG("Mem stats:\n\tpageLookups:\t%" PRIu32 "\n\tpageHits:\t%" PRIu32 "\n\tpageMisses:\t%" PRIu32 "\n\tpageWrites:\t%" PRIu32,
    self->pageLookups, self->pageHits, self->pageMisses, self->pageWrites);
#endif
#else
  free(self->data);
#endif
}

void mem_read(Memory *self, cpu_addr_t addr, void* buf, size_t size) {
#ifdef CONFIG_DOS
  unsigned long int localAddr = (unsigned long int)addr;

  for(size_t done = 0; done < size;) {
    const struct _memPage *page = _lookupPage(self, localAddr);

    const size_t pageOffest = (size_t)localAddr & MEM_PAGE_OFFSET_MASK;

    const size_t chunkSize = min_size_t(MEM_PAGE_SIZE - pageOffest, size - done);

    _fmemcpy((char *)buf + done, page->data + pageOffest, chunkSize);

    done += chunkSize;
    localAddr += chunkSize;
  }
#else
  assert(addr < self->dev.size);
  assert(size <= self->dev.size - addr);

  memcpy(buf, self->data + addr, size);
#endif
}

void mem_write(Memory *self, cpu_addr_t addr, const void* buf, size_t size) {
#ifdef CONFIG_DOS
  unsigned long int localAddr = (unsigned long int)addr;

  for(size_t done = 0; done < size;) {
    struct _memPage *page = _lookupPage(self, localAddr);

    const size_t pageOffest = (size_t)localAddr & MEM_PAGE_OFFSET_MASK;

    const size_t chunkSize = min_size_t(MEM_PAGE_SIZE - pageOffest, size - done);

    _fmemcpy(page->data + pageOffest, (const char *)buf + done, chunkSize);

    page->dirty = true;

    done += chunkSize;
    localAddr += chunkSize;
  }
#else
  assert(addr < self->dev.size);
  assert(size <= self->dev.size - addr);

  memcpy(self->data + addr, buf, size);
#endif
}

int mem_cmp(Memory *self, cpu_addr_t addr, const void *buf, size_t size) {
#ifdef CONFIG_DOS
  int res = 0;

  unsigned long int localAddr = (unsigned long int)addr;

  for(size_t done = 0; done < size;) {
    struct _memPage *page = _lookupPage(self, localAddr);

    const size_t pageOffest = (size_t)localAddr & MEM_PAGE_OFFSET_MASK;

    size_t chunkSize = min_size_t(MEM_PAGE_SIZE - pageOffest, size - done);

    res = _fmemcmp(page->data + pageOffest, (const char *)buf + done, chunkSize);
    if(res)
      break;

    done += chunkSize;
    localAddr += chunkSize;
  }

  return res;
#else
  assert(addr < self->dev.size);
  assert(size <= self->dev.size - addr);

  return memcmp(self->data + addr, buf, size);
#endif
}

bool mem_dumpImage(Memory *self, const char *imgFile) {
  FILE *f = fopen(imgFile, "wb");
  if(!f)
    return false;

  for(cpu_size_t s = 0; s < self->dev.size;) {
    char buf[4096];
    const cpu_size_t chunkSize = min_cpu_size_t(sizeof(buf), self->dev.size - s);

    mem_read(self, s, buf, chunkSize);

    fwrite(buf, 1, chunkSize, f);

    s += chunkSize;
  }

  fclose(f);

  return true;
}
