include Makefile.inc

# Memory allocator selection (default: buddy)
ALLOCATOR ?= buddy

KERNEL_BIN=kernel.bin
KERNEL_ELF=kernel.elf
SOURCES=$(wildcard *.c ./drivers/*.c ./idt/*.c ./ds/*.c)
SOURCES_ASM=$(wildcard asm/*.asm)

# Select memory allocator source file based on ALLOCATOR variable
ifeq ($(ALLOCATOR),bitmap)
    MEMORY_SRC=./memory/bitmap.c
else
    MEMORY_SRC=./memory/buddy.c
endif

SOURCES += $(MEMORY_SRC)

HOT_OBJECTS=./drivers/video.o fonts.o # Compiled with -O3
OBJECTS=$(SOURCES:.c=.o)
OBJECTS_ASM=$(SOURCES_ASM:.asm=.o)
LOADERSRC=loader.asm

LOADEROBJECT=$(LOADERSRC:.asm=.o)
STATICLIBS=

all: $(KERNEL_BIN)

$(KERNEL_ELF): $(LOADEROBJECT) $(OBJECTS) $(STATICLIBS) $(OBJECTS_ASM)
	$(LD) $(LDFLAGS) -T kernel.ld -o $(KERNEL_ELF) $(LOADEROBJECT) $(OBJECTS) $(OBJECTS_ASM) $(STATICLIBS)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $(KERNEL_ELF) $(KERNEL_BIN)

$(HOT_OBJECTS) : %.o: %.c
	$(GCC) -O3 $(GCCFLAGS) -I./include -c $< -o $@

$(filter-out $(HOT_OBJECTS),$(OBJECTS)) : %.o: %.c
	$(GCC) $(GCCFLAGS) -I./include -I./font_assets -c $< -o $@

%.o : %.asm
	$(ASM) $(ASMFLAGS) $< -o $@

# font.o:
# 	objcopy -O elf64-x86-64 -B i386 -I binary ./font_assets/Solarize.12x29.psf font.o

$(LOADEROBJECT):
	$(ASM) $(ASMFLAGS) $(LOADERSRC) -o $(LOADEROBJECT)

clean:
	rm -rf */*.o *.o *.bin memory/*.o $(KERNEL_ELF)

.PHONY: all clean
