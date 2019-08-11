#include "vm/page.h"
#include <stdbool.h>
#include <stdint.h>
#include <debug.h>
#include <stdlib.h>	
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "filesys/file.h"

// struct hash pages;
struct lock hash_pages_lock;


/* Returns a hash value for page p. */
static unsigned
page_hash (const struct hash_elem * p_, void *aux UNUSED)
{
	const struct page *p = hash_entry (p_, struct page, hash_elem);
	return hash_bytes (&p->upage, sizeof p->upage);
}

/* Returns true if page a precedes page b. */
static bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
	const struct page *a = hash_entry (a_, struct page, hash_elem);
	const struct page *b = hash_entry (b_, struct page, hash_elem);
	return a->upage < b->upage;
}

void
page_init(struct hash * pages)
{
	hash_init (pages, page_hash, page_less, NULL);
}

void * 
page_alloc(enum palloc_flags flags)
{
  return frame_palloc(flags);
}

bool 
page_install(void *upage, void *kpage, bool writable, struct hash * pages)
{
  bool success = frame_install(upage, kpage, writable);
  if (success) {
    struct page * new_page = malloc(sizeof(struct page));
    new_page->upage = upage;
    new_page->file_entry = NULL;
    new_page->swap_block_index = -1;
    hash_insert(pages, &new_page->hash_elem);  
  }
  return success;
}

void 
page_free(void * kpage)
{
	frame_free(kpage);
}


void
page_add_file( struct file * file, off_t ofs, void *upage, uint32_t read_bytes, 
  uint32_t zero_bytes, bool writable, int mmapid,  struct hash * pages)
{
    struct page * page_for_file  = malloc(sizeof(struct page));
    struct file_info * file_entry = malloc(sizeof(struct file_info));
    file_entry->file = file;
    file_entry->ofs = ofs;
    file_entry->read_bytes = read_bytes;
    file_entry->zero_bytes = zero_bytes;
    file_entry->writable = writable;
    file_entry->mmapid = mmapid;
    page_for_file->file_entry = file_entry;
    page_for_file->upage = upage;
    page_for_file->swap_block_index = -1;
    hash_insert(pages, &page_for_file->hash_elem);
    
}

struct page *
page_valid(void * upage, struct hash * pages)
{
	struct page p;
	struct hash_elem *e;
	p.upage = upage;
	e = hash_find(pages, &p.hash_elem);
	return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}


bool 
page_load(struct page * faulted_page)
{
  if (faulted_page->swap_block_index != -1){
      bool reclaimed = frame_reclamation(faulted_page->upage, faulted_page->swap_block_index);
      faulted_page->swap_block_index = -1;
      return reclaimed;
  }

	struct file_info * file_entry = faulted_page->file_entry;
	size_t read_bytes = file_entry->read_bytes;
	size_t zero_bytes = file_entry->zero_bytes;

  lock_acquire(&alloc_lock);
  uint8_t *kpage = frame_palloc(PAL_USER);
  if (kpage == NULL){
    lock_release(&alloc_lock);
    return false;
  }
  // Load this page...
  if (zero_bytes != PGSIZE){
    int read = file_read_at(file_entry->file, kpage, read_bytes, file_entry->ofs);
    if (read != (int)read_bytes) {
      frame_free(kpage);
      lock_release(&alloc_lock);
      return false;
    }
  }

  memset(kpage + read_bytes, 0, zero_bytes);
  /* Add the page to the process's address space. */
  if (!frame_install(faulted_page->upage, kpage, file_entry->writable)) {
    frame_free(kpage);
    lock_release(&alloc_lock);
    return false;
  }

  frame_unpin(kpage);
  lock_release(&alloc_lock);
  if (faulted_page->swap_block_index != -1)
    faulted_page->swap_block_index = -1;
 
  return true;
}


static void
hash_elem_free(struct hash_elem *e, void *aux UNUSED)
{
  struct page * cur = hash_entry (e, struct page, hash_elem);
  if (cur->file_entry != NULL){
    if (cur->file_entry->mmapid != -1 && pagedir_is_dirty(thread_current()->pagedir, cur->upage)){
      file_write_at(cur->file_entry->file, cur->upage, cur->file_entry->read_bytes, cur->file_entry->ofs);
    }
    free(cur->file_entry);
  }

  frame_free_page(cur->upage, cur->swap_block_index, thread_current());
  free(cur);
}

void
page_thread_free(struct hash * pages)
{
  hash_destroy(pages, &hash_elem_free);
}

void 
page_unmap (struct hash * pages, int fd)
{
  int size = 0;
  struct page * to_delete[hash_size(pages)];
  
  struct hash_iterator i;
  hash_first (&i, pages);
  while (hash_next (&i)) {
    struct page * page = hash_entry (hash_cur (&i), struct page, hash_elem);
    if (page->file_entry != NULL && page->file_entry->mmapid == fd){
      if (pagedir_is_dirty(thread_current()->pagedir, page->upage)){
        file_write_at(page->file_entry->file, page->upage, page->file_entry->read_bytes, page->file_entry->ofs);
      }
      to_delete[size] = page;
      size += 1;
    } 
    
  }
  
  
  int j;
  for (j = 0; j < size; j ++){
    free(to_delete[j]->file_entry);
    frame_free_page(to_delete[j]->upage, to_delete[j]->swap_block_index, thread_current());
    hash_delete(pages, &to_delete[j]->hash_elem);
    free(to_delete[j]);
  }


}

void
page_unpin(void * kpage)
{
  frame_unpin(kpage);
}