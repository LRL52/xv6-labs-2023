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

#define PAGE_IDX(pa) (((uint64)pa - (uint64)end) >> PGSHIFT)
static int _refcount[RAM_SIZE / PGSIZE];
static struct spinlock _refcount_lock[RAM_SIZE / PGSIZE];
extern volatile int started;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  int i;

  initlock(&kmem.lock, "kmem");
  for (i = 0; i < RAM_SIZE / PGSIZE; ++i) {
    initlock(&_refcount_lock[i], "_refcount");
    _refcount[i] = 0;
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Get the reference count of the page 
int
get_refcount(void *pa) {
  // if (started) printf("get_refcount: %p %d %d\n", pa, PAGE_IDX(pa), _refcount[PAGE_IDX(pa)]);
  return _refcount[PAGE_IDX(pa)];
}

// Set the reference count of the page
void
set_refcount(void *pa, int count) {
  int idx;

  idx = PAGE_IDX(pa);
  acquire(&_refcount_lock[idx]);
  _refcount[idx] = count;
  release(&_refcount_lock[idx]);
  // if (started) printf("set_refcount: %p %d %d\n", pa, PAGE_IDX(pa), _refcount[PAGE_IDX(pa)]);
}

// Increment the reference count
int
page_ref_inc(void *pa) {
  int idx, ret;

  idx = PAGE_IDX(pa);
  acquire(&_refcount_lock[idx]);
  ret = ++_refcount[idx];
  release(&_refcount_lock[idx]);
  // if (started) printf("page_ref_inc: %p %d %d\n", pa, PAGE_IDX(pa), _refcount[PAGE_IDX(pa)]);
  return ret;
}

// Decrement the reference count
int
page_ref_dec(void *pa) {
  int idx, ret;

  idx = PAGE_IDX(pa);
  acquire(&_refcount_lock[idx]);
  ret = --_refcount[idx];
  release(&_refcount_lock[idx]);
  // if (started) printf("page_ref_dec: %p %d %d\n", pa, PAGE_IDX(pa), _refcount[PAGE_IDX(pa)]);
  return ret;
}

// Increment the reference count of 
// the page. This is the same as page_ref_inc.
void
get_page(void *pa) {
  page_ref_inc(pa);
}

// Decrement the reference count of
// the page. If the reference count
// falls to 0, free the page.
void
put_page(void *pa) {
  if (get_refcount(pa) == 1) {
    kfree(pa);
  } else {
    page_ref_dec(pa);
  }
}


// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  if (page_ref_dec(pa) > 0) {
    return;
    // panic("kfree: refcount > 0");
  }

  // if (started)
    // printf("kfree: %p\n", pa);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    set_refcount((void*)r, 1);
    // if (started) printf("kalloc: %p\n", r);
  }
  return (void*)r;
}

// collect the amount of free memory in bytes
void
calc_freemem(void)
{
  struct run *r;
  uint64 count = 0;

  acquire(&kmem.lock);
  r = kmem.freelist;
  while(r) {
    ++count;
    r = r->next;
  }
  release(&kmem.lock);

  printf("Free pages: %d\n", count);
}
