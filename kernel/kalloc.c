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

struct
{
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct
{
  struct spinlock lock;
  char *ref_base;
  char *alloc_base;
} ref_counter;

void kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void *)PHYSTOP);
}

int get_pages(void *pa_start, void *pa_end)
{
  char *p;
  int pages = 0;
  p = (char *)PGROUNDUP((uint64)pa_start);

  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    pages++;
  int total_ref = pages / PGSIZE + 1;
  return total_ref;
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  printf("total ref is %d\n", get_pages(pa_start, pa_end));
  int mem = get_pages(pa_start, pa_end) * PGSIZE;
  ref_counter.ref_base = (char *)PGROUNDUP((uint64)pa_start);
  ref_counter.alloc_base = (char *)(ref_counter.ref_base + mem);
  memset(ref_counter.ref_base, 1, mem);

  p = ref_counter.alloc_base;
  // printf("cmp base and ref %p & %p\n", (char *)PGROUNDUP((uint64)pa_start), p);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

int ref_index(void *pa)
{
  return ((char *)pa - ref_counter.alloc_base) / PGSIZE;
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&ref_counter.lock);
  char ref = --ref_counter.ref_base[ref_index(pa)];
  if (ref > 0)
  {
    release(&ref_counter.lock);
    return;
  }
  release(&ref_counter.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

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
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r)
  {
    memset((char *)r, 5, PGSIZE); // fill with junk
    acquire(&ref_counter.lock);
    ++ref_counter.ref_base[ref_index(r)];
    release(&ref_counter.lock);
  }
  return (void *)r;
}

//increase reference
void ref_inc(void *pa)
{
  acquire(&ref_counter.lock);
  ++ref_counter.ref_base[ref_index(pa)];
  release(&ref_counter.lock);
}
