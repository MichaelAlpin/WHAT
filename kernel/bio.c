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

#define NBUCK 13 // The number of buckets in the hash table

struct
{
  struct buf buf[NBUF]; // Array of the buffers

  struct buf *table[NBUCK];            // Hash table of the buffers
  struct spinlock bucket_locks[NBUCK]; // Locks per bucket
} bcache;

int f(uint x)
{
  return x % NBUCK;
}

void binit(void)
{
  struct buf *b;

  // Initialize the locks
  for (int i = 0; i < NBUCK; i++)
  {
    initlock(&bcache.bucket_locks[i], "bcache");
  }

  // Initialize the buffer hash table & buffer locks
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    initsleeplock(&b->lock, "buffer");
    int index = f(b->blockno);

    // Add the buffer to the table
    acquire(&bcache.bucket_locks[index]);
    struct buf *previous_head = bcache.table[index];
    bcache.table[index] = b;
    b->prev = 0;
    b->next = previous_head;
    if (previous_head != 0)
    {
      previous_head->prev = b;
    }
    release(&bcache.bucket_locks[index]);
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  int index = f(blockno);

  struct buf *b;

  // Is the block already cached?
  acquire(&bcache.bucket_locks[index]);
  for (b = bcache.table[index]; b != 0; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      release(&bcache.bucket_locks[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached
  release(&bcache.bucket_locks[index]);
  for (int i = 0; 1; i++)
  {
    b = &bcache.buf[i % NBUF];
    if (b->refcnt == 0)
    {
      uint previous_index = f(b->blockno);
      acquire(&bcache.bucket_locks[index]);
      if (previous_index != index)
      {
        acquire(&bcache.bucket_locks[previous_index]);
      }

      if (b->refcnt == 0)
      {
        // Make sure that the block isn't in the cache
        struct buf *traveler;
        for (traveler = bcache.table[index]; traveler != 0; traveler = traveler->next)
        {
          if (traveler->dev == dev && traveler->blockno == blockno)
          {
            traveler->refcnt++;
            release(&bcache.bucket_locks[index]);
            if (previous_index != index)
            {
              release(&bcache.bucket_locks[previous_index]);
            }
            acquiresleep(&traveler->lock);
            return traveler;
          }
        }

        // Update the allocated buffer metadata & location
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        if (previous_index != index)
        {
          // Move the block to the right place in the table
          // First, remove from the original bucket
          if (b->prev != 0)
          {
            b->prev->next = b->next;
          }
          else
          {
            bcache.table[previous_index] = b->next;
          }
          if (b->next != 0)
          {
            b->next->prev = b->prev;
          }

          // Then, append to the new bucket
          struct buf *previous_head = bcache.table[index];
          bcache.table[index] = b;
          b->prev = 0;
          b->next = previous_head;
          if (previous_head != 0)
          {
            previous_head->prev = b;
          }
        }

        release(&bcache.bucket_locks[index]);
        if (previous_index != index)
        {
          release(&bcache.bucket_locks[previous_index]);
        }

        acquiresleep(&b->lock);
        return b;
      }

      release(&bcache.bucket_locks[index]);
      if (previous_index != index)
      {
        release(&bcache.bucket_locks[previous_index]);
      }
    }
  }
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  acquire(&bcache.bucket_locks[f(b->blockno)]);
  b->refcnt--;
  release(&bcache.bucket_locks[f(b->blockno)]);
}

void bpin(struct buf *b)
{
  acquire(&bcache.bucket_locks[f(b->blockno)]);
  b->refcnt++;
  release(&bcache.bucket_locks[f(b->blockno)]);
}

void bunpin(struct buf *b)
{
  acquire(&bcache.bucket_locks[f(b->blockno)]);
  b->refcnt--;
  release(&bcache.bucket_locks[f(b->blockno)]);
}
