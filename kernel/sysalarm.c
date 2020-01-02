#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

extern char trampoline[], uservec[], userret[];

uint64
sys_sigalarm(void)
{
  int interval;
  uint64 handler_addr; 
  if(argint(0, &interval) < 0)
    return -1;
  if (argaddr(1, &handler_addr) < 0)
    return -1;

  struct proc * p = myproc();
  
  p->interval = interval;
  p->handler = handler_addr;

  return 0;  // not reached
}

uint64
sys_sigreturn(void)
{
  struct proc * p = myproc();
  *(p->tf) = *(p->tf_backup);
  p->returned = 1;
  return 0;
}