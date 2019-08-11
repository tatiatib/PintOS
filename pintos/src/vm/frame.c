#include "vm/frame.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <debug.h>
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "threads/interrupt.h"
#include <stdlib.h>	
#include <stddef.h>
#include <string.h>
#include <random.h>
#include "swap.h"
struct hash frames;
#define MIN_REMOVE 8
/* Returns a hash value for page p. */
static unsigned
frame_hash (const struct hash_elem *f_, void *aux UNUSED)
{
	const struct frame *f = hash_entry (f_, struct frame, hash_elem);
	return hash_bytes (&f->kpage, sizeof (f->kpage));
}

/* Returns true if page a precedes page b. */
static bool
frame_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
	const struct frame *a = hash_entry (a_, struct frame, hash_elem);
	const struct frame *b = hash_entry (b_, struct frame, hash_elem);
	return a->kpage < b->kpage;
}

void 
frame_init()
{
	hash_init (&frames, frame_hash, frame_less, NULL);
	lock_init(&alloc_lock);
}

struct frame *
get_random_frame (struct list * frames)
{
  int index = random_ulong () % list_size(frames); 
  struct list_elem *e = list_begin (frames);
  while (index > 0){
    e = list_next (e);
    index -= 1;
  }

  return list_entry (e, struct frame, elem);
}

struct frame *
find_frame()
{
  struct list not_dirty_frames;
  struct list dirty_frames;
  struct list not_accessed_frames;
  list_init(&not_dirty_frames);
  list_init(&dirty_frames);
  list_init(&not_accessed_frames);
  struct hash_iterator i;
  hash_first (&i, &frames);
  while (hash_next (&i)){
    struct frame *f  = hash_entry (hash_cur (&i), struct frame, hash_elem);
    if (!f->pinned){
      bool accessed = pagedir_is_accessed(f->owner->pagedir, f->upage);
      bool dirty = pagedir_is_dirty(f->owner->pagedir, f->upage);
      if (!accessed && (!dirty || !f->writable)){
        list_push_back(&not_accessed_frames, &f->elem);
      }else{
        if(accessed && (!dirty || !f->writable)){
          list_push_back(&not_dirty_frames, &f->elem);
        }else{
          list_push_back(&dirty_frames, &f->elem);
        }
      }
    }
    pagedir_set_accessed(f->owner->pagedir, f->upage, false);
  }


  if (list_size(&not_accessed_frames) > 10){
    return get_random_frame(&not_accessed_frames);
  }else{
    if(list_size(&not_dirty_frames) > 10)
      return get_random_frame(&not_dirty_frames);
    else
      return get_random_frame(&dirty_frames);
    
  }
  return NULL;
}



void 
eviction()
{
  struct frame * to_evict = find_frame();
  if (to_evict == NULL){
    PANIC("Frame table is empty");
  }
  struct page * page = page_valid(to_evict->upage, &to_evict->owner->pages);
  ASSERT(page->upage == to_evict->upage);
  ASSERT(page != NULL);
  if (page->file_entry != NULL){
    if (page->file_entry->mmapid != -1){
      file_write_at(page->file_entry->file, page->upage, page->file_entry->read_bytes, page->file_entry->ofs);
    } else{
      if (to_evict->writable || pagedir_is_dirty(to_evict->owner->pagedir, to_evict->upage)){
        size_t index = swap_write(to_evict->kpage);
        page->swap_block_index = index; 
      }
    }
  }else{  
    if (pagedir_is_dirty(to_evict->owner->pagedir, to_evict->upage)){
      size_t index = swap_write(to_evict->kpage);
      page->swap_block_index = index;    

    }
    
  }
  pagedir_clear_page (to_evict->owner->pagedir, to_evict->upage);
  palloc_free_page(to_evict->kpage);
  hash_delete(&frames, &to_evict->hash_elem);
  free(to_evict);
} 


void *
frame_palloc(enum palloc_flags flags)
{

	void * page = palloc_get_page(flags);
	if (page == NULL){
    eviction();
    page = palloc_get_page(flags);
    if (page == NULL)
      PANIC("come on");
	}

	return page;
}   


bool 
frame_install(void *upage, void *kpage, bool writable)
{
	struct thread * t = thread_current();
	struct frame  * page_frame = malloc(sizeof(struct frame));
	page_frame->upage = upage;
	page_frame->kpage = kpage;
	page_frame->owner = t;
  page_frame->pinned = true;
  page_frame->writable = writable;
	hash_insert(&frames, &page_frame->hash_elem);
	return (pagedir_get_page(t->pagedir, upage) == NULL
          && pagedir_set_page(t->pagedir, upage, kpage, writable));
}


void 
frame_free(void * kpage)
{
	struct frame f;
	struct hash_elem *e;
	f.kpage = kpage;
	e = hash_delete (&frames, &f.hash_elem);
	struct frame * frame = hash_entry (e, struct frame, hash_elem);
	palloc_free_page(kpage);
	free(frame);
}


void 
frame_free_page (void * upage, int index, struct thread * current)
{
  struct hash_iterator i;
  struct frame *f = NULL;
  hash_first (&i, &frames);
  while (hash_next (&i)){
    f = hash_entry (hash_cur (&i), struct frame, hash_elem);
    if (f->upage == upage && f->owner == current){
      break;
    }
  }

  if (f != NULL){
    if (index != -1)
      swap_remove(index);
    hash_delete(&frames, &f->hash_elem);
    free(f);
  }
  
}

void 
frame_unpin (void * kpage)
{
  struct frame f;
  struct hash_elem *e;
  f.kpage = kpage;
  e = hash_find (&frames, &f.hash_elem);
  struct frame * frame = hash_entry (e, struct frame, hash_elem);
  frame->pinned = false;

}

bool
frame_reclamation(void * upage, int index)
{
  lock_acquire(&alloc_lock);
  uint8_t *kpage = frame_palloc(PAL_USER);
  if (kpage == NULL){
    lock_release(&alloc_lock);
    return false;
  }
 
  swap_read(kpage, index);

  /* Add the page to the process's address space. */
  if (!frame_install(upage, kpage, true)) {
    frame_free(kpage);
    lock_release(&alloc_lock);
    return false;
  }
  frame_unpin(kpage);
  pagedir_set_accessed (thread_current()->pagedir, upage, false);
  pagedir_set_dirty (thread_current()->pagedir, upage, false);
  lock_release(&alloc_lock);
  return true;

}