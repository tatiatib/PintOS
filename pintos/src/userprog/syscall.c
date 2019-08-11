#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/directory.h"
#include <string.h>
#include "threads/synch.h"
#include "process.h"
#include "exception.h"
#include "vm/page.h"

#define FUNC_NUMB 21
static void syscall_handler (struct intr_frame *);
void write_handler(void *args, uint32_t * eax);
void exit_handler (void *args, uint32_t * eax);
void halt_handler (void *args, uint32_t * eax);
void practice_handler(void *args, uint32_t * eax);
void create_handler (void *args, uint32_t * eax);
void open_handler (void *args, uint32_t * eax);
void close_handler (void *args, uint32_t * eax);
void exec_handler (void *args, uint32_t * eax);
void wait_handler (void *args, uint32_t * eax);
void read_handler (void *args, uint32_t * eax);
void filesize_handler (void *args, uint32_t * eax);
void seek_handler (void *args, uint32_t * eax);
void tell_handler (void *args, uint32_t * eax);
void remove_handler (void *args, uint32_t * eax);
void mmap_handler (void *args, uint32_t * eax);
void munmap_handler (void *args, uint32_t * eax);
void chdir_handler (void * args, uint32_t * eax);
void mkdir_handler (void * args, uint32_t * eax);
void readdir_handler (void * args, uint32_t * eax);
void isdir_handler (void * args, uint32_t * eax);
void isnumber_handler (void * args, uint32_t * eax);
typedef void (*syscall)(void *args, uint32_t * eax);


static syscall functions[FUNC_NUMB] = {halt_handler, exit_handler, exec_handler, wait_handler, create_handler, remove_handler,
    open_handler, filesize_handler, read_handler, write_handler, seek_handler, tell_handler, close_handler, practice_handler,
    mmap_handler, munmap_handler, chdir_handler, mkdir_handler, readdir_handler, isdir_handler, isnumber_handler};


void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


void
force_exit(uint32_t * eax)
{
  int error = -1;
  functions[SYS_EXIT](&(error), eax);
}

static bool
check_boundaries (void * args)
{
  if (is_user_vaddr(args) && pagedir_get_page (thread_current()->pagedir, args) != NULL)
    return true;
  else{
    struct page * page_faulted = page_valid(pg_round_down (args), &thread_current()->pages);
    if (page_faulted != NULL && page_load(page_faulted)){
      return true;
    }
    else{
      if (is_user_vaddr(args) && stack_page_valid(thread_current()->esp, args)){
        if (grow_stack(args)) return true;
      }
    } 
  }
  return false;
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  if (check_boundaries((void *) f->esp)){
    thread_current()->esp = f->esp;
    uint32_t* args = ((uint32_t*) f->esp);
    int sys_number = args[0];
    if (sys_number < FUNC_NUMB){
      args = (void*)(char *)args + sizeof(int);
      if (check_boundaries((void *)args))
        functions[sys_number](args, &f->eax);
      else
        force_exit(&f->eax);    
    }
  }else
    force_exit(&f->eax);
}

void
exec_handler (void *args, uint32_t * eax)
{
  char *cmdline = *(char **)args;
  if (cmdline == NULL || !check_boundaries(cmdline)){
    force_exit(eax);
    return;
  }
  tid_t pid = process_execute(cmdline);
  *eax = pid;
}

void
wait_handler (void *args, uint32_t * eax)
{
  int pid = *(int*)args;
  *eax = process_wait(pid);
}

void 
write_handler(void *args, uint32_t * eax)
{
  int fd = *(int*)args;
  void * buffer = *(void **)((char *) args + sizeof (int));
  if(!check_boundaries(buffer)){
    force_exit(eax);
    return;
  }
  size_t size = *(size_t *)((char *) args + sizeof (int) + sizeof (void *));
  if (fd == 1){
    putbuf(buffer, size);
  }else{
    if (fd >= 130){
      force_exit(eax);
      return;
    }
    struct file * cur_file = thread_current()->fds[fd]; 
    if (cur_file && !cur_file->deny_write){
      //lock_acquire(&filesys_lock);
      off_t byte_written = file_write(cur_file, buffer, size);
      //lock_release(&filesys_lock);
      *eax = byte_written;
    }else{
      *eax  = 0;
    }

  }
}

void 
exit_handler (void *args, uint32_t * eax)
{
  *eax = *(int*)args; 
  printf("%s: exit(%d)\n", &thread_current ()->name, *eax);
  thread_current()->me->return_value = *eax;
  thread_exit();
}

void 
halt_handler (void *args UNUSED, uint32_t * eax UNUSED)
{
  shutdown_power_off ();
}

void 
practice_handler(void *args, uint32_t * eax)
{
   int x = *(int*)args;
   *eax = x + 1;
}


void 
create_handler (void *args, uint32_t * eax)
{
  char *name = *(char **)args;
  if (name == NULL || !check_boundaries(name)){
    force_exit(eax);
    return;
  }
  
  if (!check_boundaries ((void *)((char*)args + sizeof(char*)))){
    force_exit(eax);
    return;
  }
  unsigned initial_size = *(int*)((char*)args + sizeof(char*));
  //lock_acquire(&filesys_lock);
  bool success = filesys_create(name, initial_size, 1);
  //lock_release(&filesys_lock);
  *eax = success;
}

void
open_handler (void *args, uint32_t * eax)
{
  char *name = *(char **)args;
  if (name == NULL || !check_boundaries(name)){
    force_exit(eax);
    return;
  }
  if (strlen(name) == 0){
    *eax = -1;
    return;
  }
  //lock_acquire(&filesys_lock);
  struct file * file = filesys_open(name);
  //lock_release(&filesys_lock);

  if (file == NULL){
    *eax = -1;
    return;
  }
  size_t last_fd = thread_current()->last_fd;
  if (last_fd == 130){
    *eax = -1;
    return;
  }
  int i;
  for (i = 2; i < 130; ++i)
  {
    if (thread_current()->fds[i] == NULL){
      thread_current()->fds[i] = file;
      *eax = i;
      return;
    }
  }
}

void 
close_handler (void *args, uint32_t *eax)
{
  int fd = *(int*)args;
  if (fd <= 1 || fd >= 130){
    force_exit(eax);
    return;
  }
  struct file * cur_file = thread_current()->fds[fd];
  if (cur_file != NULL){ 
    //lock_acquire(&filesys_lock);
    if (cur_file->inode->data.type)
      file_close(cur_file);
    else
      dir_close((struct dir *)cur_file);
    //lock_release(&filesys_lock);
    thread_current()->fds[fd] = NULL;
  }else{
    force_exit(eax);
  }

}

void 
read_handler (void *args, uint32_t *eax)
{
  int fd = *(int*)args;
  char * buffer = *(char **)((char *) args + sizeof (int));
  if(!check_boundaries(buffer)){
    
    force_exit(eax);
    return;
  }
  size_t size = *(size_t *)((char *) args + sizeof (int) + sizeof (void *));
  if (fd == 0){
     size_t i;
     for (i = 0; i < size; i++){
        uint8_t c = input_getc();
        buffer[i] = (char)c;
     }
  }
  if (fd == 1 || fd >= 130){
    force_exit(eax);
    return;
  }

  struct file * cur_file = thread_current()->fds[fd];
  if (cur_file == NULL){
    force_exit(eax);
    return;
  }
  //lock_acquire(&filesys_lock);
  off_t bytes_read = file_read(cur_file, buffer, size);
  //lock_release(&filesys_lock);
  *eax = bytes_read;
}

void
filesize_handler (void *args, uint32_t *eax)
{
  int fd = *(int*)args;
  struct file * cur_file = thread_current()->fds[fd];
  //lock_acquire(&filesys_lock);
  off_t size = file_length(cur_file);
  //lock_release(&filesys_lock);
  *eax = size;
}


void 
seek_handler (void *args, uint32_t *eax)
{
  int fd = *(int*)args;
  unsigned int position = *(unsigned int*) (args + sizeof (int));
  
  if (fd == 1 || fd >= 130){
    force_exit(eax);
    return;
  }

  struct file * cur_file = thread_current()->fds[fd];
  if (cur_file == NULL){
    force_exit(eax);
    return;
  }

  //lock_acquire(&filesys_lock);
  file_seek(cur_file, position);
  //lock_release(&filesys_lock);
} 

void 
tell_handler (void *args, uint32_t *eax)
{
  int fd = *(int*)args;
  
  if (fd == 1 || fd >= 130){
    force_exit(eax);
    *eax= -1;
    return;
  }

  
  struct file * cur_file = thread_current()->fds[fd];
  if (cur_file == NULL){
    force_exit(eax);
    *eax= -1;
    return;
  }

  //lock_acquire(&filesys_lock);
  int answer = file_tell(cur_file);
  //lock_release(&filesys_lock);
  *eax= answer;
}

void 
remove_handler (void *args, uint32_t *eax)
{
  char * cur_file = *(char **) args;
  if (cur_file == NULL || !check_boundaries(cur_file)){
    force_exit(eax);
    return;
  }

  //lock_acquire(&filesys_lock);
  bool success = filesys_remove(cur_file);
  //lock_release(&filesys_lock);
  *eax = success;
}


void 
mmap_handler (void * args, uint32_t *eax)
{
  int fd = *(int*)args;
  void * addr = *(void **)((char *) args + sizeof (int));
  if (fd == 0 || fd == 1 || (unsigned int)addr == 0 
    || pagedir_get_page (thread_current()->pagedir, addr) || pg_round_down(addr) != addr){
    *eax = -1;
    return;
  }  

  struct page * used = page_valid(pg_round_down (addr), &thread_current()->pages);
  if (used != NULL){
    *eax = -1;
    return;
  }
  struct file * cur_file = thread_current()->fds[fd];
  if (cur_file == NULL){
    *eax= -1;
    force_exit(eax);
    return;
  }

  //lock_acquire(&filesys_lock);
  struct file * file = file_reopen(cur_file);
  if (file == NULL){
    //lock_release(&filesys_lock);
    force_exit(eax);
  }
  int length = file_length(cur_file);
  //lock_release(&filesys_lock);
  if (length == 0){
    force_exit(eax);
  }
  int ofs = 0;
  while (length > 0){
    size_t page_read_bytes = length < PGSIZE ? length : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;
    page_add_file (file, ofs, addr, page_read_bytes, page_zero_bytes, true, fd,  &thread_current()->pages);
    addr += PGSIZE;
    ofs += PGSIZE;
    length -= page_read_bytes;
    struct page * used = page_valid(pg_round_down (addr), &thread_current()->pages);
    if (used != NULL){
      *eax = -1;
      return;
    }
  }
  *eax = fd;
}

void 
munmap_handler (void * args, uint32_t *eax)
{
  int fd = *(int*) args;
  page_unmap(&thread_current()->pages, fd);
  *eax = fd;  
}

void 
chdir_handler (void * args, uint32_t *eax)
{
  char * dir_name = *(char **) args;
  if (dir_name == NULL || !check_boundaries(dir_name)){
    force_exit(eax);
    return;
  }
  //lock_acquire(&filesys_lock);
  struct dir * dir = dir_get_by_name(dir_name);
  if (dir != NULL){
    dir_close(thread_current()->cwd);
    thread_current()->cwd = dir;
    *eax = true;
  }else
    *eax = false;
  //lock_release(&filesys_lock);
}

void 
mkdir_handler (void * args, uint32_t *eax)
{
  char * dir = *(char **) args;
  if (dir == NULL || !check_boundaries(dir)){
    force_exit(eax);
    return;
  }
  //lock_acquire(&filesys_lock);
  *eax = filesys_create(dir, 0, 0);
  //lock_release(&filesys_lock);
}

void 
readdir_handler (void * args, uint32_t *eax)
{
  int fd = *(int*)args;  
  if (fd == 0 || fd == 1 || fd >= 130){
    force_exit(eax);
    *eax= -1;
    return;
  }
  char * name = *(char **)((char *) args + sizeof (int));
  if (name == NULL || !check_boundaries(name)){
    force_exit(eax);
    return;
  }
  struct file * dir = thread_current()->fds[fd];
  if (dir->inode->data.type == 0){
    //lock_acquire(&filesys_lock);
    *eax = dir_readdir((struct dir *)dir, name);
    //lock_release(&filesys_lock);
  }
  else
    *eax = 0;
}

void 
isdir_handler (void * args, uint32_t *eax)
{
  int fd = *(int*)args;  
  if (fd == 0 || fd == 1 || fd >= 130){
    force_exit(eax);
    *eax= -1;
    return;
  }
  //lock_acquire(&filesys_lock);
  int type = thread_current()->fds[fd]->inode->data.type;
  //lock_release(&filesys_lock);
  *eax = !type;
}

void 
isnumber_handler (void * args, uint32_t *eax)
{ 
  int fd = *(int*)args;  
  if (fd == 0 || fd == 1 || fd >= 130){
    force_exit(eax);
    *eax= -1;
    return;
  }
  //lock_acquire(&filesys_lock);
  *eax = thread_current()->fds[fd]->inode->sector;
  //lock_release(&filesys_lock);
}