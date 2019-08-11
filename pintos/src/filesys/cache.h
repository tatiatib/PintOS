#ifndef CACHE_H
#define CACHE_H
#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#define CACHE_SIZE 64

struct cache_block
{
	block_sector_t sector_idx;
	void * data;
	struct lock sector_lock;
	bool dirty;
};	

void cache_init(void);
void cache_read(block_sector_t, void *, int, size_t);
int cache_find(block_sector_t);
void cache_write(block_sector_t, void *, int, size_t );
void cache_back_to_disk(void);
void cache_read_ahead(block_sector_t);

#endif
