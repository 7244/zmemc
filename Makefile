OUTPUT = zmemc

AMD64_LINKER =
AMD64_COMPILER =
I386_LINKER = -m elf_i386
I386_COMPILER = -m32

ARCH = AMD64

LINKER = ld -n --gc-sections -z nosectionheader $($(ARCH)_LINKER)

COMPILER = clang
COMPILER += -masm=intel -std=c99
COMPILER += -nostdlib -fno-stack-protector -ffreestanding -fno-asynchronous-unwind-tables -fdata-sections -ffunction-sections -Qn
COMPILER += $($(ARCH)_COMPILER)
COMPILER += -c

debug: CFLAGS = -g
debug: _all
release: CFLAGS = -Ofast -Os
release: _all

_all: _compile _clean

_compile:
	$(COMPILER) src/_start_$(ARCH).S -o _start.S.o && \
	$(COMPILER) $(CFLAGS) src/main.c -o main.c.o && \
	$(LINKER) main.c.o _start.S.o -o $(OUTPUT) || true

_clean:
	rm -f main.c.o
	rm -f _start.S.o
