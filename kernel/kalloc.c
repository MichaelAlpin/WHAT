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

struct run
{
  struct run *next;
};

// The freelist lists for the CPUs
struct
{
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void kinit()
{
  // Initialize all the locks
  for (int i = 0; i < NCPU; i++)
  {
    initlock(&kmem[i].lock, "kmem");
  }

  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  push_off();
  int i = cpuid();

  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  // Update the running CPU's free list
  acquire(&kmem[i].lock);
  r->next = kmem[i].freelist;
  kmem[i].freelist = r;
  release(&kmem[i].lock);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  push_off();
  int i = cpuid();

  acquire(&kmem[i].lock);

  struct run *r = kmem[i].freelist;
  if (r == 0)
  {
    // Long route: the free list is empty, meaning we have to still some pages from a following CPU
    for (int j = 1; j < NCPU; j++)
    {
      if (kmem[(i + j) % NCPU].freelist > 0)
      {
        acquire(&kmem[(i + j) % NCPU].lock);
        struct run *traveler = kmem[(i + j) % NCPU].freelist;
        struct run *half_traveler = kmem[(i + j) % NCPU].freelist;

        // Reach the middle of the free list of the other CPU, indexed i+j
        while (traveler > 0)
        {
          traveler = traveler->next;
          if (traveler > 0)
            traveler = traveler->next;

          half_traveler = half_traveler->next;
        }

        if (half_traveler == 0)
        {
          // Move the list from index i+j to index i when the list has only one item
          kmem[i].freelist = kmem[(i + j) % NCPU].freelist;
          kmem[(i + j) % NCPU].freelist = 0;
        }
        else
        {
          // Move half of the list from index i+j to index i
          kmem[i].freelist = half_traveler;
          for (struct run *scanner = kmem[(i + j) % NCPU].freelist; scanner != 0; scanner = scanner->next)
          {
            if (scanner->next == half_traveler)
            {
              scanner->next = 0;
              break;
            }
          }
        }

        release(&kmem[(i + j) % NCPU].lock);
        break;
      }
    }

    r = kmem[i].freelist;
  }

  if (r > 0)
  {
    // When page is found, remove it from the CPU's free list and fill it with junk
    kmem[i].freelist = r->next;
    memset((char *)r, 5, PGSIZE);
  }

  release(&kmem[i].lock);

  pop_off();
  return (void *)r;
}
