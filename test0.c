#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

char buff[80];

int main(int argc, char **argv) {
	assert(argc > 2);
	int fd, ret;
	fd = open("/dev/db", O_RDWR);
	if(fd < 0) {
		printf("FAIL: open2 errno: %d, %s\n", errno, strerror(errno));
	}
	assert(fd >= 0);
	ret = pread(fd, buff, atoi(argv[2]), atoi(argv[1]));
	write(0, buff, atoi(argv[2]));
	printf("\n");
	close(fd);
}
