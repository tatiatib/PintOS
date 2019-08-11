#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "filesys/cache.h"
#include <stdio.h>

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define BLOCK_PER_SECTOR BLOCK_SECTOR_SIZE / sizeof(block_sector_t)
#define BLOCKS_DBL_INDIRECT BLOCK_PER_SECTOR * BLOCK_PER_SECTOR

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}


/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  
  ASSERT (inode != NULL);
  if (pos < inode->data.length){
    if ( (size_t)(pos / BLOCK_SECTOR_SIZE) < inode->data.used_direct){
      return inode->data.direct_blocks[pos/BLOCK_SECTOR_SIZE];
    }
    if ((size_t )(pos / BLOCK_SECTOR_SIZE) < (inode->data.used_direct + inode->data.used_indirect)){
      block_sector_t * cur = calloc(1, BLOCK_SECTOR_SIZE);
      block_read(fs_device, inode->data.indirect, cur);
      pos = pos - (BLOCK_SECTOR_SIZE * DIRECT_NUMB);
      pos = pos / BLOCK_SECTOR_SIZE;
      block_sector_t res = cur[pos];
      free(cur);
      return res;
    }         
    if ((size_t )(pos / BLOCK_SECTOR_SIZE) < (inode->data.used_direct + inode->data.used_indirect 
      + inode->data.used_double_indirect * BLOCK_SECTOR_SIZE )){
      block_sector_t * cur = calloc(1, BLOCK_SECTOR_SIZE);
      block_read(fs_device, inode->data.double_indirect, cur);
      pos = pos - (BLOCK_SECTOR_SIZE * DIRECT_NUMB) - (BLOCK_PER_SECTOR * BLOCK_SECTOR_SIZE);
      int cur_index = (int) (pos) / (int)(BLOCK_SECTOR_SIZE * BLOCK_PER_SECTOR);
      block_sector_t * cur_q = calloc(1, BLOCK_SECTOR_SIZE);
      block_read(fs_device, cur[cur_index], cur_q);
      pos = pos - (cur_index * BLOCK_SECTOR_SIZE * BLOCK_PER_SECTOR);
      pos = (int)(pos) / (int)(BLOCK_SECTOR_SIZE);
      block_sector_t res = cur_q[pos];
      free(cur_q);
      free(cur);
      return res;
    } 
    // return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  }
  
  return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

static int count_direct(size_t sectors);
static int count_indirect(size_t sectors);
static int count_dbl_indirect(size_t sectors);
void inode_free(struct inode_disk * inode);

bool alloc_direct(struct inode_disk * inode, size_t size, size_t start);
bool alloc_indirect(struct inode_disk * inode, size_t size, size_t start);
bool alloc_dbl_indirect(struct inode_disk * inode, size_t  size, size_t start_1, size_t start_2);
void extend_file(struct inode * inode, off_t offset, off_t size);
static char zeros[BLOCK_SECTOR_SIZE];
struct lock open_nodes_lock;

static int 
count_direct(size_t sectors)
{
  return sectors > DIRECT_NUMB ? DIRECT_NUMB : sectors;
}

static int 
count_indirect(size_t sectors)
{
  return sectors < (DIRECT_NUMB + BLOCK_PER_SECTOR) ? sectors - DIRECT_NUMB : BLOCK_PER_SECTOR;
}

static int
count_dbl_indirect(size_t sectors)
{
  return sectors  - (DIRECT_NUMB + BLOCK_PER_SECTOR);
} 


bool 
alloc_direct(struct inode_disk * inode, size_t size, size_t start)
{
  size_t i;
  for (i = start; i < size; i++){
     if (!free_map_allocate (1, &inode->direct_blocks[i]))
      return false;

  }
  return true;
}

bool 
alloc_indirect(struct inode_disk * inode, size_t size, size_t start)
{
  block_sector_t * blocks = calloc(1, BLOCK_SECTOR_SIZE);
  if (start == 0){
    ASSERT(free_map_allocate (1, &inode->indirect));
  }else
    block_read(fs_device, inode->indirect, blocks);
  
  size_t i;
  for (i = start; i < size; i++){
     if (!free_map_allocate (1, &blocks[i]))
      return false;
    else
      block_write (fs_device, blocks[i], zeros);  
  }
  block_write(fs_device, inode->indirect, blocks);
  free(blocks);
  return true; 

}
bool 
alloc_dbl_indirect(struct inode_disk * inode, size_t size, size_t start_1, size_t start_2)
{
  int layer_1 = (int )(size) / (int)(BLOCK_PER_SECTOR) + 1;
  block_sector_t * blocks = calloc(1, BLOCK_SECTOR_SIZE);
  if (start_1 == 0 && start_2 == 0){
    ASSERT(free_map_allocate (1, &inode->double_indirect));
  }else
    block_read(fs_device, inode->double_indirect, blocks);

  int i;
  for (i = start_1; i < layer_1; i++){
    block_sector_t * dbl_blocks = calloc(1, BLOCK_SECTOR_SIZE);
    if (start_2 == 0){
      ASSERT(free_map_allocate (1, &blocks[i]));
    }else
      block_read(fs_device, blocks[i], dbl_blocks);
        
    int j;
    int max = i < layer_1 - 1 ? BLOCK_PER_SECTOR : (size -  (layer_1 - 1) * BLOCK_PER_SECTOR);
    for (j = start_2; j < max; j ++){
      ASSERT(free_map_allocate(1, &dbl_blocks[j]))
      block_write (fs_device, dbl_blocks[j], zeros);  
    }
    block_write(fs_device, blocks[i], dbl_blocks);
    free(dbl_blocks);

  }
  
  block_write(fs_device, inode->double_indirect, blocks);
  free(blocks);
  return true;
}
/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
  lock_init(&open_nodes_lock);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, int type)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;
  int direct = 0, indirect = 0, double_indirect = 0;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);
  
  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->used_indirect = 0;
      disk_inode->used_double_indirect = 0;
      disk_inode->type = type;
      direct = count_direct(sectors);
      success = alloc_direct(disk_inode,  direct, 0);
      disk_inode->used_direct = direct;
      if ((sectors - direct > 0) && success){
        indirect = count_indirect(sectors);
        disk_inode->used_indirect = indirect;
        success = alloc_indirect(disk_inode, indirect, 0);
      }
     
      if ((sectors - (direct + indirect) > 0) && success){
        double_indirect = count_dbl_indirect(sectors);
        disk_inode->used_double_indirect = double_indirect;
        success = alloc_dbl_indirect(disk_inode, double_indirect, 0, 0);
      }
      if (success)
        block_write(fs_device, sector, disk_inode);
      // if (free_map_allocate (sectors, &disk_inode->start))
      //   {
      //     block_write (fs_device, sector, disk_inode);
      //     if (sectors > 0)
      //       {
      //         static char zeros[BLOCK_SECTOR_SIZE];
      //         size_t i;

      //         for (i = 0; i < sectors; i++)
      //           block_write (fs_device, disk_inode->start + i, zeros);
      //       }
      //     success = true;
      //   }
      if (!success) inode_free(disk_inode);
      free (disk_inode);
    }

  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;
  lock_acquire(&open_nodes_lock);
  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          lock_release(&open_nodes_lock);
          return inode;
        }
    }
  
  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL){
    lock_release(&open_nodes_lock);
    return NULL;
  }
  
  
  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  lock_release(&open_nodes_lock);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init(&inode->lock);
  block_read (fs_device, inode->sector, &inode->data);
  
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}


void 
inode_free (struct inode_disk * inode)
{
  size_t i;
  block_sector_t * cur = NULL;
  for (i = 0; i < inode->used_direct; i++){
    free_map_release (inode->direct_blocks[i], 1);
  }
  if (inode->used_indirect != 0){
    cur = calloc(1, BLOCK_SECTOR_SIZE);
    block_read(fs_device, inode->indirect, cur);
    size_t  i;
    for (i = 0; i < inode->used_indirect; i++)
      free_map_release (cur[i], 1);
  }

  if (inode->used_double_indirect != 0){
    size_t layer_1 = inode->used_double_indirect / BLOCK_PER_SECTOR + 1;
    block_read(fs_device, inode->double_indirect, cur);
    size_t i;
    block_sector_t * layer = calloc(1, BLOCK_SECTOR_SIZE);
    for (i = 0; i < layer_1; i++){
      block_read(fs_device, cur[i],  layer);
      size_t j;
      size_t max = i < layer_1 - 1 ? BLOCK_PER_SECTOR : (inode->used_double_indirect -  (layer_1 - 1) * BLOCK_PER_SECTOR);
      for (j = 0;  j < max; j ++){
        free_map_release (layer[j], 1);
      }
    }
    free(layer);
  }

  free(cur);
}
/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;
  lock_acquire(&open_nodes_lock);
  block_write(fs_device, inode->sector, &inode->data);
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      // printf("%s\n", "remove");
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
      lock_release(&open_nodes_lock);
      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          free_map_release (inode->sector, 1);
          inode_free(&inode->data);
        }
      free (inode);
    }else{
      lock_release(&open_nodes_lock);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  if (inode_length(inode) > size + offset){
    cache_read_ahead(byte_to_sector(inode, offset + size));
  }

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
      //   {
          /* Read full sector directly into caller's buffer. */
          // block_read (fs_device, sector_idx, buffer + bytes_read);          
          // cache_read(sector_idx, (void*)(buffer + bytes_read), sector_ofs, BLOCK_SECTOR_SIZE );
      //   }
      // else
      //   {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          // if (bounce == NULL)
          //   {
          //     bounce = malloc (BLOCK_SECTOR_SIZE);
          //     if (bounce == NULL)
          //       break;
          //   }
    
          // block_read (fs_device, sector_idx, bounce);
      cache_read(sector_idx, (void*)(buffer + bytes_read), sector_ofs, chunk_size);
          // memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        // }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  return bytes_read;
}

void
extend_file(struct inode * inode, off_t offset, off_t size)
{
  size_t cur_alloced = inode->data.used_direct * BLOCK_SECTOR_SIZE + inode->data.used_indirect * BLOCK_SECTOR_SIZE 
    + inode->data.used_double_indirect * BLOCK_SECTOR_SIZE;

  if ((size_t)((inode->data.length - offset + size) + inode->data.length) < cur_alloced){
    inode->data.length += (inode->data.length - offset + size);
    return;
  }
  if (offset > inode->data.length){
    inode->data.length += (offset - inode->data.length + size);
  }else
    inode->data.length += (inode->data.length - offset + size);

  size_t sectors_need_all = bytes_to_sectors(inode->data.length);
  size_t sectors_need = sectors_need_all - (inode->data.used_direct + inode->data.used_indirect + inode->data.used_double_indirect);
  size_t direct = 0;
  size_t indirect = 0;
  size_t dbl_indirect = 0;
  if (inode->data.used_direct < DIRECT_NUMB){
    if (inode->data.used_direct + sectors_need > DIRECT_NUMB){
      direct = DIRECT_NUMB - inode->data.used_direct;
      indirect = sectors_need - direct;
    }else
      direct = sectors_need;
    alloc_direct(&inode->data, direct + inode->data.used_direct, inode->data.used_direct);
    inode->data.used_direct += direct;
  }else
    indirect = sectors_need;

  if (indirect > 0 && inode->data.used_indirect < BLOCK_PER_SECTOR){
    if (inode->data.used_indirect + indirect > BLOCK_PER_SECTOR){
      indirect = BLOCK_PER_SECTOR - inode->data.used_indirect;
      dbl_indirect = sectors_need - direct - indirect;
    }
    alloc_indirect(&inode->data, indirect + inode->data.used_indirect, inode->data.used_indirect);
    inode->data.used_indirect += indirect;
  }else{
    if (direct == 0)
      dbl_indirect = sectors_need;
  }

  if(dbl_indirect > 0 && inode->data.used_double_indirect < BLOCKS_DBL_INDIRECT){
    int ind_layer_1 =(int )(inode->data.used_double_indirect) /  (int )(BLOCK_PER_SECTOR);
    int ind_layer_2 = inode->data.used_double_indirect -  ind_layer_1 * BLOCK_PER_SECTOR;
    alloc_dbl_indirect(&inode->data, dbl_indirect + inode->data.used_double_indirect, ind_layer_1, ind_layer_2);
    inode->data.used_double_indirect += dbl_indirect;
  }
  
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  if (inode->deny_write_cnt)
    return 0;

  if (offset + size > inode_length(inode)){
    extend_file(inode, offset, size);
    block_write(fs_device, inode->sector, &inode->data);
  }

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        // {
          /* Write full sector directly to disk. */
          // block_write (fs_device, sector_idx, buffer + bytes_written);
          // cache_write(sector_idx, (void *)(buffer + bytes_written), sector_ofs,  chunk_size);
        // }
      // else
        // {
           // We need a bounce buffer. 
          // if (bounce == NULL)
          //   {
          //     bounce = malloc (BLOCK_SECTOR_SIZE);
          //     if (bounce == NULL)
          //       break;
          //   }  
          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          // if (sector_ofs > 0 || chunk_size < sector_left){
            // block_read (fs_device, sector_idx, bounce);
            // bounce = cache_read(sector_idx)->data;
            // memcpy(bounce, cache_read(sector_idx)->data, BLOCK_SECTOR_SIZE);
          // }else
            // memset (bounce, 0, BLOCK_SECTOR_SIZE);
          // memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          // block_write (fs_device, sector_idx, bounce);
      cache_write(sector_idx, (void *)(buffer + bytes_written), sector_ofs, chunk_size);
        // }
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}


