#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "transdb.h"

char data[] = "Teatr-Akcje";
char buff[10];

int main() {
	int fd, ret;
	fd = open("/dev/db", O_RDWR);
	if(fd < 0) {
		printf("FAIL: open1 errno: %d, %s\n", errno, strerror(errno));
	}
	assert(fd >= 0);
	ret = write(fd, data, 10);
	if(ret != 10) {
		printf("FAIL: write1: %d errno: %d, %s\n", ret, errno, strerror(errno));
	}
	assert(ret == 10);
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
	ret = read(fd, buff, 10);
	if(ret != 10) {
		printf("FAIL: read2: %d fd: %d errno: %d, %s\n", ret, fd, errno, strerror(errno));
	}
	assert(ret == 10);
	assert(strncmp(buff, data, 10) == 0);
	close(fd);
}
