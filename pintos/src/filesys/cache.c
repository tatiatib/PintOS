#include "cache.h"
#include <stdbool.h>
#include <stdint.h>
#include <debug.h>
#include <stdlib.h>	
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <bitmap.h>
#include <random.h>
#include "threads/malloc.h"
#include "filesys/inode.h"
#include "threads/thread.h"


static struct bitmap * bitmap;
static struct cache_block cache[CACHE_SIZE];
struct lock cache_lock;
int  cache_add(block_sector_t sector_idx);
void cache_eviction(void);
static void read_next_sector(void *);
static struct list read_ahead_sectors;
struct semaphore next_sector;
struct lock list_lock;
int init = 0;

struct list_entry
{
	int sector_idx;
	struct list_elem list_elem;
};

#define RAND_NUMB 10
void
cache_init()
{
	int i;
	for(i = 0; i < CACHE_SIZE; i++){
		lock_init(&cache[i].sector_lock);
		cache[i].dirty = false;
		cache[i].sector_idx = -1;
	}
	init = 1;
	bitmap = bitmap_create(CACHE_SIZE );
	lock_init(&cache_lock);
	lock_init(&list_lock);
	list_init(&read_ahead_sectors);
	sema_init(&next_sector, 0);
	thread_create("read_ahead", PRI_DEFAULT, read_next_sector, NULL);
}

void
cache_read(block_sector_t sector_idx, void * buffer, int offset, size_t size)
{
	lock_acquire(&cache_lock);
	int index = cache_find(sector_idx);	
	
	if (index == -1){
		index = cache_add(sector_idx);
	}
	lock_release(&cache_lock);
	ASSERT(offset + size <= BLOCK_SECTOR_SIZE);
	lock_acquire(&cache[index].sector_lock);
	memcpy(buffer, (uint8_t *)cache[index].data + offset, size);
	lock_release(&cache[index].sector_lock);
}

int
cache_find(block_sector_t index)
{
	int i;
	for( i = 0; i < CACHE_SIZE; i++){
		if(cache[i].sector_idx == index)
			return i;
	}
	return -1;	
}


int
cache_add(block_sector_t sector_idx)
{
	size_t i = bitmap_scan_and_flip(bitmap, 0, 1, false);
	if (i == BITMAP_ERROR) {
		cache_eviction();
		i = bitmap_scan_and_flip(bitmap, 0, 1, false); 
	}
	cache[i].data = malloc(BLOCK_SECTOR_SIZE);

	block_read (fs_device, sector_idx, cache[i].data);
	cache[i].sector_idx = sector_idx;

	return i;
}


void 
cache_write(block_sector_t sector_idx, void * buffer, int offset, size_t chunk_size)
{
	lock_acquire(&cache_lock);
	int index = cache_find(sector_idx);	
	
	if (index == -1){
		index = cache_add(sector_idx);
	}
	lock_release(&cache_lock);

	lock_acquire(&cache[index].sector_lock);
	cache[index].dirty = true;
	memcpy((uint8_t *)cache[index].data + offset, buffer, chunk_size);
	lock_release(&cache[index].sector_lock);
}

void 
cache_eviction()
{
	int i;
	int index;
	for (i = 0; i < RAND_NUMB; i++){
		index = random_ulong()%CACHE_SIZE;
		if (!cache[index].dirty) break;
	}
	lock_acquire(&cache[index].sector_lock);
	if (cache[index].dirty)
		block_write(fs_device, cache[index].sector_idx, cache[index].data);

	cache[index].sector_idx = -1;
	cache[index].dirty = false;
	free(cache[index].data);
	lock_release(&cache[index].sector_lock);
	bitmap_set(bitmap, index, false);
}

void
cache_back_to_disk()
{
	if (!init) return;
	lock_acquire(&cache_lock);
	int i;
	for (i = 0; i < CACHE_SIZE; i++){
		if (cache[i].dirty)
			block_write(fs_device, cache[i].sector_idx, cache[i].data);
		free(cache[i].data);
		cache[i].sector_idx = -1;
	}
	lock_release(&cache_lock);
}

void
cache_read_ahead(block_sector_t sector_idx)
{
	struct list_entry  *sector = malloc(sizeof(struct list_entry));
	sector->sector_idx = sector_idx;
	lock_acquire(&list_lock);
	list_push_back(&read_ahead_sectors, &sector->list_elem);
	lock_release(&list_lock);
	sema_up(&next_sector);

}

static void
read_next_sector(void * aux UNUSED){
	while (1){
		sema_down(&next_sector);
		lock_acquire(&list_lock);
		struct list_entry * cur = list_entry(list_pop_front(&read_ahead_sectors), struct list_entry, list_elem);
		lock_release(&list_lock);
		lock_acquire(&cache_lock);
		int index = cache_find(cur->sector_idx);	
		if (index == -1){
			size_t i = bitmap_scan_and_flip(bitmap, 0, 1, false);
			if (i != BITMAP_ERROR) {
				cache[i].data = malloc(BLOCK_SECTOR_SIZE);

				block_read (fs_device, cur->sector_idx, cache[i].data);
				cache[i].sector_idx = cur->sector_idx;
			}
		}
		lock_release(&cache_lock);
		free(cur);

	}
}
