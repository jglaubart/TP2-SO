
# Memory allocator selection (default: buddy)
ALLOCATOR ?= buddy

all:  bootloader kernel userland image

bootloader:
	cd Bootloader; make all

kernel:
	cd Kernel; make all ALLOCATOR=$(ALLOCATOR)

userland:
	cd Userland; make all

image: kernel bootloader userland
	cd Image; make all

clean:
	cd Bootloader; make clean
	cd Image; make clean
	cd Kernel; make clean
	cd Userland; make clean
	rm -f *.zip

.PHONY: bootloader image collections kernel userland all clean
