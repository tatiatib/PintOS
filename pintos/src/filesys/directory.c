#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>


/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  return inode_create (sector, entry_cnt * sizeof (struct dir_entry), 0);
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode)
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL;
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir)
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir)
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir)
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp)
{
  struct dir_entry e;
  size_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e){
    
    if (e.in_use && !strcmp (name, e.name))
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
    }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode)
{
  struct dir_entry e;
  ASSERT (dir != NULL);
  ASSERT (name != NULL);
  
  lock_acquire(&dir->inode->lock);
  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;
  lock_release(&dir->inode->lock);
  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);


  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;
  
  lock_acquire(&dir->inode->lock);
  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL)){
    goto done;
  }

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;

  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
 done:
  lock_release(&dir->inode->lock);
  return success;
}

static bool
dir_empty (struct dir *dir)
{
  char file[NAME_MAX + 1];
  return !dir_readdir (dir, file);
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name)
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);
  
  lock_acquire(&dir->inode->lock);
  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs)){
    goto done;
  }

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL || (inode->data.type == 0 && !dir_empty(dir_open(inode))))
    goto done;
  

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  lock_release(&dir->inode->lock);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;
  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e)
    {
      dir->pos += sizeof e;
      if (e.in_use && strcmp(e.name, "..") && strcmp(e.name, "."))
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        }
    }
  return false;
}

static int
get_last_token(char * name)
{
  char * name_parse = malloc(strlen(name) + 1);
  strlcpy(name_parse, name, strlen(name) + 1);
  char * saveptr = name_parse;
  char * token = strtok_r(name_parse, "/", &saveptr);
  int token_numb = 0;
  while (1)
  {
    token = strtok_r(NULL, "/", &saveptr);
    if (token == NULL) break;
    token_numb += 1;
  }
  free(name_parse);
  return token_numb;
}

struct dir *
dir_parent(char * name, char ** entry, bool find)
{
  if(thread_current()->cwd->inode->removed) {
    return NULL;
  }

  char * name_parse = malloc(strlen(name) + 1);
  strlcpy(name_parse, name, strlen(name) + 1);
  char * saveptr = name_parse;
  char * token = strtok_r(name_parse, "/", &saveptr);
  if (strlen(token) == strlen(name)){
    *entry = malloc(strlen(token) + 1);
    strlcpy(*entry, token, strlen(token) + 1);
    free(name_parse);
    return dir_reopen(thread_current()->cwd);
  }
  int token_numb = get_last_token(name);
  struct inode * parent_node;
  struct dir * parent;
  if ((char)name[0] == '/'){
    parent = dir_open_root();
    if (token_numb == 0 && (dir_lookup(parent, token, &parent_node) || !find)){
      *entry = malloc(strlen(token) + 1);
      strlcpy(*entry, token, strlen(token) + 1);
      free(name_parse);
      return parent;
    }
  }else
    parent = dir_reopen(thread_current()->cwd);

  
  if(!(dir_lookup(parent, token, &parent_node))){
    free(name_parse);
    return NULL;
  }
  dir_close(parent);
  struct inode * child_node;
  for (token = strtok_r(NULL, "/", &saveptr); token != NULL; token = strtok_r(NULL, "/", &saveptr)){
    parent = dir_open (parent_node);  
    dir_lookup(parent, token, &child_node);
    token_numb -= 1;
    if (token_numb == 0 && ((!find && child_node == NULL) || (find && child_node != NULL)))
      break;
    
    if (child_node == NULL) {
      free(name_parse);
      return NULL;
    }else{
      dir_close(parent);
      parent_node = child_node;
    }
  }
  *entry = malloc(strlen(token) + 1);
  strlcpy(*entry, token, strlen(token) + 1); 
  free(name_parse);
  return parent;
}

struct dir *
dir_get_by_name(char * name)
{
  if (strcmp(name, "/") == 0) return dir_open_root();
  char * new_entry = NULL;
  struct dir * dir = dir_parent((char*)name, &new_entry, false); 
  struct inode * child_node;
  if(!dir_lookup(dir, new_entry, &child_node)){
    dir_close(dir);
    return NULL;
  }
  dir_close(dir);
  return dir_open(child_node);
}