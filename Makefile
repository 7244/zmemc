OUTPUT = zmemc

AMD64_LINKER =
AMD64_COMPILER =
I386_LINKER = -m elf_i386
I386_COMPILER = -m32

ARCH = AMD64

LINKER = ld -no-pie -n --gc-sections $($(ARCH)_LINKER)

COMPILER = cc
COMPILER += -std=c99
COMPILER += -fno-pic
COMPILER += -nostdlib -fomit-frame-pointer -fno-stack-protector -ffreestanding -fno-asynchronous-unwind-tables -fdata-sections -ffunction-sections -Qn
COMPILER += $($(ARCH)_COMPILER)
COMPILER += -c

debug: CFLAGS = -g
debug: _all
release: CFLAGS = -O3 -ffast-math -Os
release: LINKER += -z nosectionheader
release: _all

_all: _compile _clean

_compile:
	$(COMPILER) $(CFLAGS) src/main.c -o main.c.o && \
	$(LINKER) main.c.o -o $(OUTPUT) || true

_clean:
	rm -f main.c.o
