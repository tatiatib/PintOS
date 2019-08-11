#include "swap.h"
#include "threads/synch.h"

#define BLOCK (PGSIZE / BLOCK_SECTOR_SIZE)
struct bitmap * bitmap;
//Make the swap block global
static struct block * global_swap_block;
//Get the block device when we initialize our swap code
void 
swap_init()
{
	global_swap_block = block_get_role(BLOCK_SWAP);
	if (global_swap_block == NULL){
		return;
	}
	bitmap = bitmap_create(block_size(global_swap_block) / BLOCK);
}


size_t 
swap_write(void * buffer)
{
	size_t index = bitmap_scan_and_flip(bitmap, 0, 1, false);
	int i;
	for(i = 0;  i < BLOCK; i++){
		block_write(global_swap_block, (block_sector_t) index * BLOCK + i, (char*)buffer + ( i * BLOCK_SECTOR_SIZE ));
	}
	return index;
}

void 
swap_read(void * buffer, size_t index)
{
	int i;
	for(i = 0;  i < BLOCK; i++){
		block_read(global_swap_block, (block_sector_t) index * BLOCK + i, (char*)buffer + ( i * BLOCK_SECTOR_SIZE ));
	}
	bitmap_set(bitmap, (size_t)index, false);
}

void 
swap_remove(size_t index)
{
	bitmap_set(bitmap, (size_t)index, false);	
}