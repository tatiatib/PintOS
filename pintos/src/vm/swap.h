#ifndef SWAP_H
#define SWAP_H
#include <bitmap.h>
#include <stdio.h>
#include "devices/block.h"
#include "frame.h"


void swap_init(void);
size_t swap_write(void *);
void swap_read(void *, size_t);
void swap_remove(size_t);
#endif	