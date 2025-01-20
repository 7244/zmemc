#define STDIN 0
#define STDOUT 1
#define STDERR 2

#include <WITCH/STR/STR.h>

#pragma pack(push, 1)
struct{
  uint8_t fdint;
  uint16_t index;
  uint8_t buffer[0x1000];
}_utility_print;
#pragma pack(pop)
void flush_print(){
  if(_utility_print.fdint != STDIN){
    IO_fd_t fd;
    IO_fd_set(&fd, _utility_print.fdint);
    IO_write(
      &fd,
      _utility_print.buffer,
      _utility_print.index
    );
    _utility_print.index = 0;
  }
}
void utility_init_print(){
  _utility_print.fdint = STDIN;
  _utility_print.index = 0;
}

#define _exit(val) \
  flush_print(); \
  PR_exit(val); \
  __unreachable();

void _utility_print_fd_update(uint32_t fdint){
  if(_utility_print.fdint != fdint){
    flush_print();
    _utility_print.fdint = fdint;
  }
}

void puts(uint32_t fdint, const void *ptr, uintptr_t size){
  _utility_print_fd_update(fdint);
  for(uintptr_t i = 0; i < size; i++){
    _utility_print.buffer[_utility_print.index] = ((const uint8_t *)ptr)[i];
    if(++_utility_print.index == sizeof(_utility_print.buffer)){
      flush_print();
    }
  }
}
#define puts_literal(fdint, literal) \
  puts(fdint, literal, sizeof(literal) - 1)

void puts_char_repeat(uint32_t fdint, uint8_t ch, uintptr_t size){
  while(size--){
    puts(fdint, &ch, 1);
  }
}

void utility_puts_number(uint32_t fdint, uint64_t num){
  uint8_t buf[64];
  uint8_t *buf_ptr;
  uintptr_t size;

  buf_ptr = buf;
  STR_uto64(num, 10, &buf_ptr, &size);
  puts(fdint, buf_ptr, size);
}

void _abort(const char *filename, uintptr_t filename_length, uintptr_t line){
  puts_literal(STDERR, "err at ");
  puts(STDERR, filename, filename_length);
  puts_char_repeat(STDERR, ':', 1);
  utility_puts_number(STDERR, line);
  puts_char_repeat(STDERR, '\n', 1);
  _exit(1);
}
#define _abort() \
  _abort(__FILE__, sizeof(__FILE__) - 1, __LINE__); \
  __unreachable();

#include <linux/mman.h>

#define __generic_malloc __generic_malloc
void *__generic_malloc(uintptr_t size){
  size += sizeof(uintptr_t);

  uintptr_t ret = syscall6(
    __NR_mmap,
    (uintptr_t)NULL,
    size,
    PROT_READ | PROT_WRITE,
    MAP_ANONYMOUS | MAP_PRIVATE,
    -1,
    0
  );

  if(ret > (uintptr_t)-0x1000){
    _abort();
  }

  *(uintptr_t *)ret = size;

  return (void *)((uintptr_t *)ret + 1);
}
#define __generic_realloc __generic_realloc
void *__generic_realloc(void *ptr, uintptr_t size){
  if(ptr == NULL){
    return __generic_malloc(size);
  }

  size += sizeof(uintptr_t);

  ptr = (void *)((uintptr_t *)ptr - 1);

  uintptr_t ret = syscall5(
    __NR_mremap,
    (uintptr_t)ptr,
    *(uintptr_t *)ptr,
    size,
    MREMAP_MAYMOVE,
    (uintptr_t)NULL
  );

  if(ret > (uintptr_t)-0x1000){
    _abort();
  }

  *(uintptr_t *)ret = size;

  return (void *)((uintptr_t *)ret + 1);
}
#define __generic_free __generic_free
void __generic_free(void *ptr){
  if(ptr == NULL){
    return;
  }

  ptr = (void *)((uintptr_t *)ptr - 1);

  uintptr_t err = syscall2(
    __NR_munmap,
    (uintptr_t)ptr,
    *(uintptr_t *)ptr
  );

  if(err != 0){
    _abort();
  }
}

bool str_n0ncmp(const void *str0, const void *str1, uintptr_t str1size){
  uintptr_t str0size = MEM_cstreu(str0);

  if(str0size != str1size){
    return 1;
  }

  return MEM_cmp(str0, str1, str0size);
}

void print_padstring(uintptr_t pad_size, const void *str, uintptr_t size){
  puts_char_repeat(STDOUT, ' ', pad_size - size);
  puts(STDOUT, str, size);
}
#define print_padstring_literal(pad_size, str) \
  print_padstring(pad_size, str, sizeof(str) - 1)

uint64_t utility_log10(uint64_t num){
  uint64_t ret = 1;
  while(num /= 10){
    ret++;
  }
  return ret;
}

void utility_print_zeropad_num(uintptr_t pad_length, uint64_t num){
  puts_char_repeat(STDOUT, '0', pad_length - utility_log10(num));
  utility_puts_number(STDOUT, num);
}

void utility_print_major_minor(uint64_t major, uintptr_t minor_length, uint64_t minor){
  utility_puts_number(STDOUT, major);
  puts_char_repeat(STDOUT, '.', 1);
  utility_print_zeropad_num(minor_length, minor);
}

void print_float(const void *color, uintptr_t color_size, uint64_t num){
  puts_char_repeat(STDOUT, ' ', 1);
  puts(STDOUT, color, color_size);
  uint64_t num_major = num / 1000;
  uint64_t num_minor = num % 1000;
  puts_char_repeat(STDOUT, ' ', 13 - (utility_log10(num_major) + 1 + 3));
  utility_print_major_minor(num_major, 3, num_minor);
  puts_literal(STDOUT, "\e[m");
}
#define print_float(color, num) \
  print_float("\e[" #color "m", sizeof("\e[" #color "m") - 1, num)

void print_sizenumber(const void *color, uintptr_t color_size, uintptr_t pad_length, uint64_t num){
  const char *sizetypestr_array[] = {
    "kB",
    "MB",
    "GB",
    "TB"
  };

  uint64_t size = num * 100;
  uint8_t DivideCount = 0;
  while(size > 1024 * 100){
    size /= 1024;
    DivideCount++;
  }

  puts_char_repeat(STDOUT, ' ', 1);
  puts(STDOUT, color, color_size);
  uint64_t num_major = size / 100;
  uint64_t num_minor = size % 100;
  puts_char_repeat(STDOUT, ' ', (pad_length - 3) - (utility_log10(num_major) + 1 + 2));
  utility_print_major_minor(num_major, 2, num_minor);
  puts_char_repeat(STDOUT, ' ', 1);
  puts(STDOUT, sizetypestr_array[DivideCount], 2);
  puts_literal(STDOUT, "\e[m");
}
#define print_sizenumber(color, pad_length, num) \
  print_sizenumber("\e[" #color "m", sizeof("\e[" #color "m") - 1, pad_length, num)

void print_cmd(uintptr_t size, const uint8_t *cmd){
  puts_char_repeat(STDOUT, ' ', 1);
  puts(STDOUT, cmd, size);
}
