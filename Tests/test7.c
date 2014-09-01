#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "transdb.h"

int fd;
char data[] = 
"0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF";

void *pfoo(void *arg) {
	int i = *((int*)arg), ret;
	char buf[32];
	ret = pread(fd, buf, 32, i);
	if(ret != 32) 
		printf("FAIL: read in thread %d, error: %d, %s\n", i, errno, strerror(errno));
	assert(ret == 32);
	assert(strncmp(buf, data + i, 32) == 0);
	ret = 0;
	return (void *) ret;
}

int main() {
	int ret;
	fd = open("/dev/db", O_RDWR);
	if(fd < 0) {
		printf("FAIL: open1 errno: %d, %s\n", errno, strerror(errno));
	}
	assert(fd >= 0);
	ret = write(fd, data, 32 * 3);
	if(ret != 32 * 3) {
		printf("FAIL: write errno: %d, %s\n", errno, strerror(errno));
	}
	assert(ret == 32 * 3);
	ret = ioctl(fd, DB_COMMIT);
	if(ret != 0) {
		printf("FAIL: db_commit errno: %d, %s\n", errno, strerror(errno));
	}
	assert(ret == 0);
	close(fd);

	pthread_t threads[32 * 2];
	pthread_attr_t attr;
	int args[32 * 2];
	int rets[32 * 2];
	if((ret = pthread_attr_init(&attr)) != 0) 
		printf("FAIL: pthread_attr_init error: %d, %s\n", ret, strerror(ret));
	assert(ret == 0);

	fd = open("/dev/db", O_RDWR);
	if(fd < 0) {
		printf("FAIL: open1 errno: %d, %s\n", errno, strerror(errno));
	}
	assert(fd >= 0);

	int i;
	for(i = 0; i < 32 * 2; i++) {
		args[i] = i;
		if((ret = pthread_create(threads + i, &attr, pfoo, args + i)) != 0)
			printf("FAIL: pthread_create error: %d, %s\n", ret, strerror(ret));
		assert(ret == 0);
	}
	for(i = 0; i < 32 * 2; i++) {
		if((ret = pthread_join(threads[i], (void **)(rets + i))) != 0)
			printf("FAIL: pthread_join error: %d, %s\n", ret, strerror(ret));
		assert(ret == 0);
		if(rets[i] != 0)
			printf("FAIL: thread %d\n", i);
		assert(rets[i] == 0);
	}

	close(fd);
	if((ret = pthread_attr_destroy(&attr)) != 0) 
		printf("FAIL: pthread_attr_destroy error: %d, %s\n", ret, strerror(ret));
	assert(ret == 0);
}
