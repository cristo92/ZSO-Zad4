#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "transdb.h"

char data[] = 
	"1111AAAA1111AAAA1111BBBB1111BBBB2222CCCC2222CCCC2222DDDD2222DDDD3333EEEE3333EEEE3333FFFF3333FFFF";
char buff[32 * 3];

int main() {
	int fd, ret;
	fd = open("/dev/db", O_RDWR);
	if(fd < 0) {
		printf("FAIL: open1 errno: %d, %s\n", errno, strerror(errno));
	}
	assert(fd >= 0);
	ret = pwrite(fd, data + 32 -4, 40, 32 - 4);
	if(ret != 40) {
		printf("FAIL: write1: %d errno: %d, %s\n", ret, errno, strerror(errno));
	}
	assert(ret == 40);
	ret = ioctl(fd, DB_COMMIT);
	if(ret != 0) {
		printf("FAIL: db_commit errno: %d, %s\n", errno, strerror(errno));
	}
	assert(ret == 0);
	close(fd);

	fd = open("/dev/db", O_RDWR);
	if(fd < 0) {
		printf("FAIL: open2 errno: %d, %s\n", errno, strerror(errno));
	}
	assert(fd >= 0);
	ret = pread(fd, buff, 40, 32 - 4);
	if(ret != 40) {
		printf("FAIL: read2: %d fd: %d errno: %d, %s\n", ret, fd, errno, strerror(errno));
	}
	assert(ret == 40);
	assert(strncmp(buff, data + 32 - 4, 40) == 0);
	close(fd);
}
