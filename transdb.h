#ifndef TRANSDB_H
#define TRANSDB_H

#include <asm/ioctl.h>

#define DB_COMMIT 	_IO('C', 0xD0)
#define DB_ROLLBACK 	_IO('C', 0xD1)

#define ACCEPT 		1
#define ROLLBACK 		2

/* BLOCK_SIZE cannot be bigger than 128 */
#define DB_BLOCK_SIZE 	32
#define DB_BLOCK_SHIFT 	5

#endif
