#ifndef TRANSDB_H
#define TRANSDB_H

#include <asm/ioctl.h>

#define DB_COMMIT 	_IO('C', 0xD0)
#define DB_ROLLBACK 	_IO('C', 0xD1)

#endif
