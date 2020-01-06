## Lab 4 Lazy allocation

### Part 0: Print page table
This part is easy, just recursively visit the pagetable and print the information
```c
void
vmprint_printer(pte_t pte, int index, int level){
    printf(" ");
    for (int i = 0; i < level; ++i){
        printf(" ..");
    }
    printf("%d: pte %p pa %p\n", index, pte, PTE2PA(pte));
}

void
vmprint_helper(pagetable_t pagetable, int level){
    for (int i = 0; i < 512; i++){
        pte_t pte = pagetable[i];
        if ((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
            // this PTE points to a lower-level page table.
            vmprint_printer(pte, i, level);
            uint64 child = PTE2PA(pte);
            vmprint_helper((pagetable_t)child, level + 1);
        }else if (pte & PTE_V){
            // leaf
            vmprint_printer(pte, i, level);
            //vmprint_printer( (uint64)(pagetable+i), i, level);
        }
    }
}

void
vmprint(pagetable_t pagetable){
    printf("page table %p\n", pagetable);
    vmprint_helper(pagetable, 1);
}
```

### Part 1 Lazy allocation

In this part, we need to make ``echo hi`` work properly:
1. First, we need to remove the ``growproc`` call in ``sys_sbrk`` and only grow the process size numerically
   ```
    myproc()->sz = addr + (long)n;
   ```
2. When page faults happen, we need to allocate new pages in the corresponding address:
   ```c
    void lazyallocation(struct proc * p, uint64 va_fault){
        // allocate a new page at va_fault
        pagetable_t pagetable = p->pagetable;
        uint64 a = PGROUNDDOWN(va_fault); 
        char* mem = kalloc(); // allocate a new page of physical memory
        if(mem == 0){
            p->killed = 1;
            exit(-1);
        }

        memset(mem, 0, PGSIZE);
        if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
            kfree(mem);
            uvmdealloc(pagetable, a, a);
            return;
        }

    return;
    }
    ```
3. The above code will make ``uvmunmap`` panic when it tries to free virtual addresses that actually have no physical mappings, or virtual addresses that have not been installed in the pagetable
   ```c
    void
    uvmunmap(pagetable_t pagetable, uint64 va, uint64 size, int do_free)
    {
        uint64 a, last;
        pte_t *pte;
        uint64 pa;

        a = PGROUNDDOWN(va);
        last = PGROUNDDOWN(va + size - 1);
        for(;;){
            //printf("current a=%p to be freed\n", a);
            if((pte = walk(pagetable, a, 0)) == 0){
            // this pte has not been installed yet
            if(a == last)
                break;
            a += PGSIZE;
            pa += PGSIZE;
            continue;
            }
            if(PTE_FLAGS(*pte) == PTE_V)
                panic("uvmunmap: not a leaf");
            if((*pte & PTE_V) == 0){
            // this pte has not been installed yet
                if(a == last)
                    break;
                a += PGSIZE;
                pa += PGSIZE;
                continue;
            }
            if(do_free){
                pa = PTE2PA(*pte);
                kfree((void*)pa);
            }
            *pte = 0;
            if(a == last)
                break;
            a += PGSIZE;
            pa += PGSIZE;
        }
    }
   ```
These changes should be able to make ``echo hi`` work

### Part 2 User tests:

1. Handle negative sbrk() arguments:
    - when we have negative sbrk() arguments, we can simply use the system call ``growproc(n)`` to free these space for us.
        ```c
        uint64
        sys_sbrk(void)
        {
            long addr;
            int n;

            if(argint(0, &n) < 0)
                return -1;

            addr = myproc()->sz;
            if (n < 0){ // if n < 0, we need to use uvmdealloc to release those memory
                if(growproc(n) < 0)
                return -1;
            }
            myproc()->sz = addr + (long)n;
            return addr;
        }
        ```
2. Kill a process if it page-faults on a virtual memory address higher than any allocated with sbrk().
3. Handle faults on the invalid page below the stack.
   - These two requirement ask us to do a check for the virtual address before trying to allocate a page for them:
    ```c
    else if ( r_scause() == 13 || r_scause() == 15 ) {
        if (r_stval() > p->sz || r_stval() > MAXVA){ // overflow
            p->killed = 1;
            exit(-1);
        }
        if (r_stval() < p->ustack){ // invalid page under user stack
            p->killed = 1;
            exit(-1);
        }
        lazyallocation(p, r_stval());
        usertrapret(); 
        return;
    }
    ```
    - For the user stack over flow problem, we need add this attribute in ``struct proc`` in ``proc.h`` and maintain this attribute in ``fork()`` and ``exec()``

4. Handle out-of-memory correctly: if kalloc() fails in the page fault handler, kill the current process.
   - Already present in the ``lazyallocation`` function

5. Handle the case in which a process passes a valid address from sbrk() to a system call such as read or write, but the memory for that address has not yet been allocated.
    - NO solution yet


## Lab 6: user-level threads and alarm

### A: Uthread: switching between threads

1. We add a field ``context`` in ``struct thread`` to save register values:
   ```c
    // Saved registers for kernel context switches.
    struct context {
        uint64 ra;
        uint64 sp;

        // callee-saved
        uint64 s0;
        uint64 s1;
        uint64 s2;
        uint64 s3;
        uint64 s4;
        uint64 s5;
        uint64 s6;
        uint64 s7;
        uint64 s8;
        uint64 s9;
        uint64 s10;
        uint64 s11;
    };

    struct thread {
    char       stack[STACK_SIZE]; /* the thread's stack */
    struct context context;
    int        state;             /* FREE, RUNNING, RUNNABLE */
    };
   ```

2. ``thread_switch.S`` just simply stores old values and load new values:
   ```c
    thread_switch:
	/* YOUR CODE HERE */
	sd ra, 0(a0)
	sd sp, 8(a0)
	sd s0, 16(a0)
	sd s1, 24(a0)
	sd s2, 32(a0)
	sd s3, 40(a0)
	sd s4, 48(a0)
	sd s5, 56(a0)
	sd s6, 64(a0)
	sd s7, 72(a0)
	sd s8, 80(a0)
	sd s9, 88(a0)
	sd s10, 96(a0)
	sd s11, 104(a0)

	ld ra, 0(a1)
	ld sp, 8(a1)
	ld s0, 16(a1)
	ld s1, 24(a1)
	ld s2, 32(a1)
	ld s3, 40(a1)
	ld s4, 48(a1)
	ld s5, 56(a1)
	ld s6, 64(a1)
	ld s7, 72(a1)
	ld s8, 80(a1)
	ld s9, 88(a1)
	ld s10, 96(a1)
	ld s11, 104(a1)
	
	ret    /* return to ra */
   ```

3. when creating a new thread, remember to set the stack address as well:
    ```c
    void 
    thread_create(void (*func)())
    {
    struct thread *t;

    for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
        if (t->state == FREE) break;
    }
    t->state = RUNNABLE;
    // YOUR CODE HERE
    t->context.ra = (uint64)func;
    t->context.sp = (uint64)((t->stack) + STACK_SIZE);
    }
    ```

### B: Alarm

1. - Q: In this exercise you'll add a feature to xv6 that periodically alerts a process as it uses CPU time. This might be useful for compute-bound processes that want to limit how much CPU time they chew up, or for processes that want to compute but also want to take some periodic action. More generally, you'll be implementing a primitive form of user-level interrupt/fault handlers; you could use something similar to handle page faults in the application, for example. Your solution is correct if it passes alarmtest and usertests.
   - A: 
     - Need add a field in ``struct proc`` to keep track of passed timer ticks. After some specified time, call the handler function in ``usertrap``
     - keep a backup of register values before switching to the handler function

2.  ``struct proc``
    ```c
    // Per-process state
    struct proc {
        struct spinlock lock;

        // p->lock must be held when using these:
        enum procstate state;        // Process state
        struct proc *parent;         // Parent process
        void *chan;                  // If non-zero, sleeping on chan
        int killed;                  // If non-zero, have been killed
        int xstate;                  // Exit status to be returned to parent's wait
        int pid;                     // Process ID

        // these are private to the process, so p->lock need not be held.
        uint64 kstack;               // Virtual address of kernel stack
        uint64 sz;                   // Size of process memory (bytes)
        pagetable_t pagetable;       // Page table
        struct trapframe *tf;        // data page for trampoline.S
        struct trapframe *tf_backup; // data page for trampoline.S backup for sigreturn
        struct context context;      // swtch() here to run process
        struct file *ofile[NOFILE];  // Open files
        struct inode *cwd;           // Current directory
        
        // sigalarm changes here
        int interval;                // time interval btw two alarms
        int ticks;                   // passed ticks since last alarm
        int returned;                // whether we have returned from sigalarm handler function
        uint64 handler;              // address for the sigalarm handler function

        char name[16];               // Process name (debugging)
    };
    ```

3. ``sys_sigalarm``:
   ```c
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
   ```

4. ``sys_sigreturn``:
   ```c
    uint64
    sys_sigreturn(void)
    {
        struct proc * p = myproc();
        *(p->tf) = *(p->tf_backup);
        p->returned = 1;
        return 0;
    }
   ```

5. changes in trap.c:
   ```c
    // give up the CPU if this is a timer interrupt.
    if(which_dev == 2){
        p->ticks += 1;
        if ( (p->ticks >= p->interval-1) && ( p->interval > 0 ) && (p->returned == 1) ){
            p->ticks = 0;
            p->returned = 0; // to be set to 1 in sigreturn()
            // switch to handler
            // save trampoline for register restoring in future
            *(p->tf_backup) = *(p->tf);
            p->tf->epc = p->handler;
        }
        yield();
    }
   ```

## Lab6: Lock

### Part A: Memory allocator
1. you will have to redesign the memory allocator to avoid a single lock and list. The basic idea is to maintain a free list per CPU, each list with its own lock. Allocations and frees on different CPUs can run in parallel, because each CPU will operate on a different list.
   - current cpu id can be obtained via ``mycpu()``
   - kmem becomes an array ``kmem[NCPU]``
2. the one CPU must "steal" part of the other CPU's free list. 
    - search for other CPU's free list
      - If found one, lock that cpu's kmem, steal that lock from its free list
      - add the lock's address into current cpu's free list

## Lab 7: file system

### Part A: Large file
1.  NDIRECT = 11, NINDIRECT = (BSIZE / sizeof(uint)), NINDIRECT_DOUBLE = (BSIZE / sizeof(uint)) * (BSIZE / sizeof(uint)), MAXFILE = NDIRECT + NINDIRECT + NINDIRECT_DOUBLE
2. 
   ```c
   bmap(struct inode *ip, uint bn)
    if (bn < NDIRECT){
        bn belongs to direct blocks;
        do some processing
    }
    bn -= NDIRECT;
    if (bn < NINDIRECT){
        bn belongs to singely indirect blocks;
        do some processing;
    }
    bn -= NINDIRECT
    if (bn < NINDIRECT_DOUBLE){
        addr_level_0 = ip->addrs[NDIRECT+1]) (if not exist, allocate one block here)
        int index_level_1 = bn // 256
        uint addr_level_1 = addr_level_0[index_level_1] (if not exist, allocate one block here) (remember to write log_op for this block)
        
            int index_level_2 = bn % 256
            uint addr_level_2 = addr_level_1[index_level_2] (if not exist, allocate one block here)
            (remember to write log_op for this block)
            release buffer
        release buffer
    }
    panic("out of range")
   ```

### Part B: Symbolic link
1. Implement the symlink(target, path) system call to create a new symbolic link at path that refers to target.
   ```c
    uint64
    sys_symlink(void)
    {
        char target[MAXPATH], path[MAXPATH];
        struct inode * ip;
        int len;

        if ( argstr(0, target, MAXPATH) < 0 || argstr(1, path, MAXPATH) < 0 ){
            return -1;
        }

        begin_op(ROOTDEV);
        ip = create(path, T_SYMLINK, 0, 0);
        if (ip == 0){
            printf("creating inode fails\n");
            end_op(ROOTDEV);
            return -1;
        }

        // write target path to ip inode
        len = strlen(target);
        if(writei(ip, 0, (uint64)target, 0, sizeof(char) * (uint64)len) != sizeof(char) * (uint64)len){
            printf("write target fails\n");
            iunlockput(ip);
            end_op(ROOTDEV);
            return -1;
        }

        iunlockput(ip);
        end_op(ROOTDEV);
        return 0;
    }
   ```

2. recursively retrive symbolic links:
   ```c
    char* retrieve_target(char* path, int level){
        struct inode *ip;
        //printf("finding path: %s\n", path);

        if ( (ip = namei(path)) == 0){
            // no file exists
            //printf("non-exist file\n");
            return (char*)0;
        }
        ilock(ip);
        // whether the target path is another symbolic link
        if (ip->type == T_SYMLINK){
            level += 1;
            if (level > 10){
            //printf("cyclic symlinks\n");
            iunlockput(ip);
            return (char*)0; // approximate cyclic symbolic link issue
            }
            // extract target path
            char target[MAXPATH];
            int num;
            if ( ( num = readi(ip, 0, (uint64)target, 0, sizeof(char)*(uint)MAXPATH)) == 0 ){
            //printf("read fails: %d < %d\n", num, (int)(sizeof(char)*(uint)MAXPATH));
            iunlockput(ip);
            return 0; // read fails
            }
            //printf("source path: %s, target path: %s\n", path, target);
            iunlockput(ip);
            return retrieve_target(target, level);
        }else{
            //printf("final target: %s\n", path);
            iunlockput(ip);
            return path;
        }

        iunlockput(ip);
        //printf("wired issue\n");
        return (char*)0;
    }
   ```

3. For concurrency:
   - The icache.lock spin-lock protects the allocation of icache entries. Since ip->ref indicates whether an entry is free, and ip->dev and ip->inum indicate which i-node an entry holds, one must hold icache.lock while using any of those fields.
   - An ip->lock sleep-lock protects all ip-> fields other than ref, dev, and inum.  One must hold ip->lock in order to read or write that inode's ip->valid, ip->size, ip->type, &c.