//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

struct file_slab {
  struct file objects[SLAB_SIZE];
  struct file_slab* next;
  int num_free;
};

struct file_slab* file_slab_containing(struct file* f) {
  uintptr_t addr = (uintptr_t) f;
  uintptr_t slab_addr = addr & ~(PAGE_SIZE - 1);
  return ((struct file_slab*) slab_addr);
}


void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");

  // Allocate slabs and add them to the list
  struct file_slab* slab_list = NULL;
  for (int i = 0; i < NUM_SLABS; i++) {
    struct file_slab* slab = kalloc();
    if (!slab) {
      // Handle allocation failure
    }
    slab->num_free = SLAB_SIZE;
    slab->next = slab_list;
    slab_list = slab;
  }
  ftable.slabs = slab_list;
}

/*
void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}
*/

struct file *
get_file_from_file_cache()
{
  acquire(&ftable.lock);

  // Search for a slab with available objects
  struct file_slab* slab = ftable.slabs;
  while (slab != NULL && slab->num_free == 0) {
    slab = slab->next;
  }

  // If we found a slab, get an object from it
  struct file* result = NULL;
  if (slab != NULL) {
    result = &slab->objects[SLAB_SIZE - slab->num_free];
    slab->num_free--;
  }

  release(&ftable.lock);
  return result;
}

void
return_file_to_file_cache(struct file* f)
{
  acquire(&ftable.lock);

  // Find the slab containing this object
  struct file_slab* slab = file_slab_containing(f);

  // Return the object to the slab
  slab->num_free++;
  if (slab->num_free == SLAB_SIZE) {
    // If the slab is now completely empty, remove it from the list and free it
    if (ftable.slabs == slab) {
      ftable.slabs = slab->next;
    } else {
      struct file_slab* prev_slab = ftable.slabs;
      while (prev_slab->next != slab) {
        prev_slab = prev_slab->next;
      }
      prev_slab->next = slab->next;
    }
    kfree(slab);
  }

  release(&ftable.lock);
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
    f = get_file_from_file_cache();
  /*
  for(f = ftable.file; f < ftable.file + NFILE; f++){ // Akshay: don't select from this array, select from the cache
    if(f->ref == 0){  // Akshay: (f->ref = 0) => not in use
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  */
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

void fileclose(struct file *f) {
    acquire(&ftable.lock);
    if (f->ref < 1)
        panic("fileclose");
    if (--f->ref > 0) {
        release(&ftable.lock);
        return;
    }
    f->type = FD_NONE;
    return_file_to_file_cache(f);
    release(&ftable.lock);
}

/*
// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if(ff.type == FD_INODE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}
*/

// Get metadata about file f.
int
filestat(struct file *f, struct stat *st)
{
  if(f->type == FD_INODE){
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

// Read from file f.
int
fileread(struct file *f, char *addr, int n)
{
  int r;

  if(f->readable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return piperead(f->pipe, addr, n);
  if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }
  panic("fileread");
}

//PAGEBREAK!
// Write to file f.
int
filewrite(struct file *f, char *addr, int n)
{
  int r;

  if(f->writable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return pipewrite(f->pipe, addr, n);
  if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * 512;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    return i == n ? n : -1;
  }
  panic("filewrite");
}

