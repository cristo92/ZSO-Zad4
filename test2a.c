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

int main(int argc, char **argv) {
	assert(argc > 2);
	int fd, n, ret;
	fd = open("/dev/db", O_WRONLY);
	if(fd < 0) {
		printf("FAIL: open1 errno: %d, %s\n", errno, strerror(errno));
	}
	assert(fd >= 0);
	n = strlen(argv[1]);
	ret = write(fd, argv[1], n);
	if(ret != n) {
		printf("FAIL: write1: %d errno: %d, %s\n", ret, errno, strerror(errno));
	}
	assert(ret == n);
	if(argv[2][0] == '1') ret = ioctl(fd, DB_COMMIT);
	else ret = ioctl(fd, DB_ROLLBACK);
	if(ret != 0) printf("FAIL\n");
	else printf("ACCEPT\n");
	close(fd);
}
