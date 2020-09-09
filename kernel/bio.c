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

#define NBUCKETS 13

struct
{
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  struct buf hashbucket[NBUCKETS];
} bcache;

void binit(void)
{
  struct buf *b;
  int i;

  for (i = 0; i < NBUCKETS; i++)
  {
    initlock(&bcache.lock[i], "bcache");
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }

  i = 0;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    b->next = bcache.hashbucket[i].next;
    b->prev = &bcache.hashbucket[i];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[i].next->prev = b;
    bcache.hashbucket[i].next = b;
  }
}

int bhash(int blockno)
{
  return blockno % NBUCKETS;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bhashnum = bhash(blockno);
  acquire(&bcache.lock[bhashnum]);

  // Is the block already cached?
  for (b = bcache.hashbucket[bhashnum].next; b != &bcache.hashbucket[bhashnum]; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      release(&bcache.lock[bhashnum]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  int next_hash_num = (bhashnum + 1) % NBUCKETS;
  while (next_hash_num != bhashnum)
  {
    acquire(&bcache.lock[next_hash_num]);
    for (b = bcache.hashbucket[next_hash_num].prev; b != &bcache.hashbucket[next_hash_num]; b = b->prev)
    {
      if (b->refcnt == 0)
      {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.lock[next_hash_num]);

        b->next = bcache.hashbucket[bhashnum].next;
        b->prev = &bcache.hashbucket[bhashnum];
        bcache.hashbucket[bhashnum].next->prev = b;
        bcache.hashbucket[bhashnum].next = b;
        release(&bcache.lock[bhashnum]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[next_hash_num]);
    next_hash_num = (next_hash_num + 1) % NBUCKETS;
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b->dev, b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
// Move to the head of the MRU list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int bhashnum = bhash(b->blockno);
  acquire(&bcache.lock[bhashnum]);
  b->refcnt--;
  if (b->refcnt == 0)
  {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[bhashnum].next;
    b->prev = &bcache.hashbucket[bhashnum];
    bcache.hashbucket[bhashnum].next->prev = b;
    bcache.hashbucket[bhashnum].next = b;
  }

  release(&bcache.lock[bhashnum]);
}

void bpin(struct buf *b)
{
  int bhashnum = bhash(b->blockno);
  acquire(&bcache.lock[bhashnum]);
  b->refcnt++;
  release(&bcache.lock[bhashnum]);
}

void bunpin(struct buf *b)
{
  int bhashnum = bhash(b->blockno);
  acquire(&bcache.lock[bhashnum]);
  b->refcnt--;
  release(&bcache.lock[bhashnum]);
}
