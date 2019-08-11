#ifndef FRAME_H
#define FRAME_H

#include <hash.h>
#include <list.h>
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/synch.h"

struct frame
{
	void * upage;
	void * kpage;
	struct hash_elem hash_elem;     /* Hash table element. */
	struct list_elem elem;
	struct thread * owner;
	bool pinned;
	bool writable;
};

struct lock alloc_lock;
void frame_init(void);
void * frame_palloc(enum palloc_flags);
bool frame_install(void *, void *, bool);
void frame_free(void *);
void frame_free_page(void *, int, struct thread *);
void frame_unpin(void *);
void eviction(void);
struct frame * find_frame(void);
bool frame_reclamation(void *, int);
struct frame * get_random_frame(struct list *);
#endif