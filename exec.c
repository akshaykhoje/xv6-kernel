#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];    //Akshay: ustack is used to build the args to be pushed on user-stack
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  struct proc *curproc = myproc();

  begin_op();

  if((ip = namei(path)) == 0){      //Akshay: namei is used to get the inode of the executable file
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }
  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf)) //Akshay: readi reads the ELF header
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)     //Akshay: setupkvm() creates a new page directory and maps kernel pages
    goto bad;

  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){   //Akshay: read ELF program headers from ELF file
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)    //Akshay: map the code/data into pagedir-pagetables-pages
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)  //Akshay: copy data from ELF file into the pages allocated
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {   //Akshay: for each entry in argv[] ...
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)  //Akshay: ... copy it on user-stack. 
      goto bad;
    ustack[3+argc] = sp;    //Akshay: remember it's location on user stack in ustack.
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)  //Akshay: copy ustack to user stack.
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)     //Akshay: copy name of new process in proc->name.
    if(*s == '/')
      last = s+1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));

  // Commit to the user image.
  oldpgdir = curproc->pgdir;      
  curproc->pgdir = pgdir;    //Akshay: change to new page directory.
  curproc->sz = sz;         //Akshay: change new size.
  curproc->tf->eip = elf.entry;  // main  //Akshay: used to jump to user code when we return from exec(). 
  curproc->tf->esp = sp;    //Akshay: set user stack pointer to "sp" (bottom of stack of arguments)
  switchuvm(curproc);   //Akshay: update TSS, change CR3 to newpagedir
  freevm(oldpgdir);
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

/*Akshay: we know that exec() does not return!
 * However, this exec() function returns to sys_exec().
 * NOTE : We are still in kernel code, running on kernel stack. p->kstack has the tf setup.
 * There is context struct on stack.
 * sys_exec() returns to trapret(), the tf will be popped; with "iret", jump into new program!
 * New program is NOT old program which could have accessed return value of sys_exec()
 */
