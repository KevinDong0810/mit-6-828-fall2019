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
} kmem[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; ++i)
    initlock(&(kmem[i].lock), "kmem");
  freerange(end, (void*)PHYSTOP);
  printf("init successfully\n");
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

int get_cpu(void){
  push_off(); // disable interruption
  int id = cpuid();
  pop_off();
  return id; 
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  //printf("entering kfree\n");
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  //printf("waiting for cpu id\n");
  int id = get_cpu();
  //printf("waiting for locks\n");
  acquire(&(kmem[id].lock));
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&(kmem[id].lock));
  //printf("kfree successfully\n");
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  //printf("entering kalloc\n");
  struct run *r;
  int id = get_cpu();
  acquire(&(kmem[id].lock));
  r = kmem[id].freelist;
  if(r)
    kmem[id].freelist = r->next;
  else{
    // search for other cpu's free list
    for (int i = 0; i < NCPU; ++i){
      if (i == id)
        continue;
      acquire(&(kmem[i].lock));
      if (kmem[i].freelist){
        r = kmem[i].freelist;
        kmem[i].freelist = r->next;
        release(&(kmem[i].lock));
        break;
      }
      release(&(kmem[i].lock));
    }
  }
  release(&kmem[id].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  
  //printf("exiting kalloc\n");
  return (void*)r;
}
