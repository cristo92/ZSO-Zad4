#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

char data[] = "&&&&bbbbccccdddd&&&&eeeeffffgggghhhhiiiijjjjkkkk&&&&llllmmmm";
char buff[80];

int main(int argc, char **argv) {
	assert(argc > 2);
	int fd, ret;
	fd = open("/dev/db", O_RDWR);
	if(fd < 0) {
		printf("FAIL: open2 errno: %d, %s\n", errno, strerror(errno));
	}
	assert(fd >= 0);
	ret = pread(fd, buff, strlen(argv[2]), atoi(argv[1]));
	if(ret != strlen(argv[2])) {
		printf("FAIL: read2: %d fd: %d errno: %d, %s\n", ret, fd, errno, strerror(errno));
	}
	assert(ret == strlen(argv[2]));
	assert(strncmp(argv[2], buff, strlen(argv[2])) == 0);
	close(fd);
}
