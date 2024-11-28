// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  int nfree; // number of free pages
} kmem[NCPU];

static char kmem_lock_name[NCPU][10];
static int initialized = 0;

void
kinit()
{
  int i;

  for (i = 0; i < NCPU; ++i) {
    snprintf(kmem_lock_name[i], sizeof(kmem_lock_name[i]), "kmem%d", i);
    initlock(&kmem[i].lock, kmem_lock_name[i]);
  }
  freerange(end, (void*)PHYSTOP);
  initialized = 1;
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int minv = 0x7fffffff, t = 0, i;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  if (!initialized) {
    for (i = 0; i < NCPU; ++i) {
      if (kmem[i].nfree < minv) {
        minv = kmem[i].nfree;
        t = i;
      }
    }
  } else {
    t = cpuid();
  }

  acquire(&kmem[t].lock);
  r->next = kmem[t].freelist;
  kmem[t].freelist = r;
  ++kmem[t].nfree;
  release(&kmem[t].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r = 0;
  int cpu, i;

  cpu = cpuid();
  if (kmem[cpu].nfree > 0) {
    acquire(&kmem[cpu].lock);
    if (kmem[cpu].nfree > 0) {
      r = kmem[cpu].freelist;
      kmem[cpu].freelist = r->next;
      --kmem[cpu].nfree;
    }
    release(&kmem[cpu].lock);
  }

  if (!r) {
    // This is a trick that uses the congruence property of number theory, 
    // to reduce lock contention when multiple CPUs are trying to allocate 
    // memory simultaneously.
    // It requires that gcd(3, NCPU) = 1.
    for (i = (cpu + 3) % NCPU; i != cpu; i = (i + 3) % NCPU) {
      if (kmem[i].nfree == 0)
        continue;
      acquire(&kmem[i].lock);
      if (kmem[i].nfree > 0) {
        r = kmem[i].freelist;
        kmem[i].freelist = r->next;
        --kmem[i].nfree;
        release(&kmem[i].lock);
        break;
      }
      release(&kmem[i].lock);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
