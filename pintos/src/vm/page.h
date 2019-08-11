#ifndef PAGE_H
#define PAGE_H
#include <hash.h>
#include "frame.h"
#include "threads/vaddr.h"

struct page
{
	uint8_t * upage;
	struct file_info * file_entry;
	struct hash_elem hash_elem;       /* Hash table element. */
	int swap_block_index;
};

struct file_info{
	struct file * file;
	off_t ofs;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool writable;
	int mmapid;
};
	

void page_init(struct hash *);
void * page_alloc(enum palloc_flags);
bool page_install(void *, void *, bool, struct hash *);
void page_free(void*);
void page_add_file(struct file *,  off_t, void *, uint32_t, uint32_t, bool, int,  struct hash *);
struct page * page_valid(void *, struct hash *);
bool page_load(struct page *);
void * page_zeros(struct hash *);
void page_thread_free(struct hash *	);
void page_unmap(struct hash *, int fd);
void page_unpin(void * kpage);
#endif