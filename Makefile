KDIR ?= /lib/modules/`uname -r`/build

default: test
	$(MAKE) -C $(KDIR) M=$$PWD

install:
	$(MAKE) -C $(KDIR) M=$$PWD modules_install

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

test: test1 test2a test2b

test1: test1.c
	gcc -o test1 test1.c

test2a: test2a.c
	gcc -o test2a test2a.c

test2b: test2b.c
	gcc -o test2b test2b.c
