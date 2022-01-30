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

int steal();

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

/*struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;*/

struct kmem_t {
  struct spinlock lock;
  struct run *freelist;
  int size;
} kmems[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmems[i].lock, "kmem");
    kmems[i].size = 0;
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

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  struct kmem_t* kmem;

  push_off();
  kmem = &kmems[cpuid()];

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem->lock);
  r->next = kmem->freelist;
  kmem->freelist = r;
  kmem->size++;
  release(&kmem->lock);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  struct kmem_t* kmem;

  push_off();
  kmem = &kmems[cpuid()];

  acquire(&kmem->lock);
  if (kmem->size == 0)
    steal();
  r = kmem->freelist;
  if (r) {
    kmem->freelist = r->next;
    kmem->size--;
  }
  release(&kmem->lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  pop_off();
  return (void*)r;
}

int richest()
{
  int maxsz, maxid;
  maxsz = 0;
  maxid = -1;

  for (int i = 0; i < NCPU; i++)
    if (i != cpuid())
      acquire(&kmems[i].lock);

  for (int i = 0; i < NCPU; i++) {
    if (kmems[i].size > maxsz) {
      maxsz = kmems[i].size;
      maxid = i;
    }
  }

  for (int i = 0; i < NCPU; i++)
    if (i != cpuid())
      release(&kmems[i].lock);

  return maxid;
}

int stealfrom(int victim) 
{
  struct kmem_t *ikmem, *vkmem;
  struct run *stealrun, *newrun, *temp;
  int stealsz, newsz;

  if (cpuid() == victim)
    panic("stealfrom self");

  ikmem = &kmems[cpuid()];
  vkmem = &kmems[victim];

  acquire(&vkmem->lock);
  if (vkmem->size == 0) {
    release(&vkmem->lock);
    return 0;
  }

  // find the middle node of linked list
  newrun = stealrun = vkmem->freelist;
  stealsz = (vkmem->size + 1) / 2;
  newsz = vkmem->size - stealsz;
  for (int i = 0; i < stealsz - 1; i++) {
    newrun = newrun->next;
  }
  temp = newrun;
  newrun = newrun->next;
  temp->next = 0;
  temp = 0;

  // do steal
  vkmem->freelist = newrun;
  vkmem->size = newsz;
  if (ikmem->size > 0)
    panic("stealfrom dstcpu rests");
  ikmem->freelist = stealrun;
  ikmem->size = stealsz;

  release(&vkmem->lock);

  return stealsz;
}

int steal() 
{
  int richer;
  if ((richer = richest()) == -1)
    return 0;
  return stealfrom(richer);
}
