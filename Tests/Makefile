SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=)

all: $(OBJECTS)

%: %.c
	gcc -pthread -o $@ $<

clean:
	rm -f $(OBJECTS)
