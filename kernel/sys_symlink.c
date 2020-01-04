#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"

uint64
sys_symlink(void)
{
  printf("Hello, world\n");
  return 0;
}