// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock[NBUF]; // block the later one when two threads that 
                              // get the same blockno are concrrently searching
                              // for a uncached block.
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

static char bcache_lock_name[NBUF][10];

#define MULTIPLIER 2654435769U  // Knuth's multiplicative method

static unsigned int hash(unsigned int k) {
    return (unsigned int)(((uint64)k * MULTIPLIER) >> 28) % NBUF;
}

void
binit(void)
{
  int i;

  for (i = 0; i < NBUF; ++i) {
    snprintf(bcache_lock_name[i], sizeof(bcache_lock_name[i]), "bcache%d", i);
    initlock(&bcache.buf[i].spinlock, bcache_lock_name[i]);
    initlock(&bcache.lock[i], bcache_lock_name[i]);
    initsleeplock(&bcache.buf[i].lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int idx, i, j;

  idx = hash(blockno);

  // Is the block already cached?
  for (i = idx, j = 0; j < NBUF; i = (i + 1) % NBUF, ++j) {
    b = &bcache.buf[i];
    acquire(&b->spinlock);
    if (b->dev == dev && b->blockno == blockno) {
      ++b->refcnt;
      release(&b->spinlock);
      acquiresleep(&b->lock);
      return b;
    }
    release(&b->spinlock);
  }

  // Not cached.
  // Search for an unused buffer.
  
  // Consider the case when two threads are searching for the same uncached block,
  // the later one should wait for the former one to finish the allocation. Otherwise,
  // the later one may encounter an empty buffer ealier than the former one.
  // So we block the later one until the former one finishes allocation.
  acquire(&bcache.lock[idx]);

  for (i = idx, j = 0; j < NBUF; i = (i + 1) % NBUF, ++j) {
    b = &bcache.buf[i];
    acquire(&b->spinlock);

    // The former thread may have already allocated the buffer.
    if (b->dev == dev && b->blockno == blockno) {
      ++b->refcnt;
      release(&b->spinlock);
      release(&bcache.lock[idx]);
      acquiresleep(&b->lock);
      return b;
    }
    
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&b->spinlock);
      release(&bcache.lock[idx]);
      acquiresleep(&b->lock);
      return b;
    }
    release(&b->spinlock);
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&b->spinlock);
  --b->refcnt;
  release(&b->spinlock);
}

void
bpin(struct buf *b) {
  acquire(&b->spinlock);
  ++b->refcnt;
  release(&b->spinlock);
}

void
bunpin(struct buf *b) {
  acquire(&b->spinlock);
  --b->refcnt;
  release(&b->spinlock);
}


