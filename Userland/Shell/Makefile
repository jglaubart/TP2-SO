include ../Makefile.inc

MODULE=shell.bin
SOURCES=$(wildcard [^_]*.c) \
        $(wildcard Commands/*.c) \
        $(wildcard tests/*.c)

GCCFLAGS += -Itests -I. -ICommands -I../include

all: $(MODULE)

$(MODULE): $(SOURCES)
	$(GCC) $(GCCFLAGS) -T shellModule.ld _loader.c -L../ $(SOURCES) -l:libc.a -l:libsys.a -o ../$(MODULE)

clean:
	rm -rf *.o

.PHONY: all clean print
