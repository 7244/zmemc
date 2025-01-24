#define STDIN 0
#define STDOUT 1
#define STDERR 2

#include <WITCH/STR/STR.h>

uint8_t _utility_print_fdint;
uint16_t _utility_print_index;
uint8_t _utility_print_buffer[0x1000];

FUNC void flush_print(){
  if(_utility_print_fdint != STDIN){
    IO_fd_t fd;
    IO_fd_set(&fd, _utility_print_fdint);
    IO_write(
      &fd,
      _utility_print_buffer,
      _utility_print_index
    );
    _utility_print_index = 0;
  }
}
FUNC void utility_init_print(){
  _utility_print_fdint = STDIN;
  _utility_print_index = 0;
}

#define _exit(val) \
  flush_print(); \
  PR_exit(val); \
  __unreachable();

FUNC void _utility_print_fd_update(uint32_t fdint){
  if(_utility_print_fdint != fdint){
    flush_print();
    _utility_print_fdint = fdint;
  }
}

FUNC void puts(uint32_t fdint, const void *ptr, uintptr_t size){
  _utility_print_fd_update(fdint);
  for(uintptr_t i = 0; i < size; i++){
    _utility_print_buffer[_utility_print_index] = ((const uint8_t *)ptr)[i];
    if(++_utility_print_index == sizeof(_utility_print_buffer)){
      flush_print();
    }
  }
}
#define puts_literal(fdint, literal) \
  puts(fdint, literal, sizeof(literal) - 1)

FUNC void puts_char_repeat(uint32_t fdint, uint8_t ch, uintptr_t size){
  while(size--){
    puts(fdint, &ch, 1);
  }
}

FUNC void utility_puts_number(uint32_t fdint, uint64_t num){
  uint8_t buf[64];
  uint8_t *buf_ptr;
  uintptr_t size;

  buf_ptr = buf;
  STR_uto64(num, 10, &buf_ptr, &size);
  puts(fdint, buf_ptr, size);
}

FUNC void _abort(const char *filename, uintptr_t filename_length, uintptr_t line){
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

#if defined(__x86_64__)
  __attribute((naked))
  static sintptr_t _utility_newthread_entry(void *stack){
    __asm__ __volatile__(
        "pop %%rdi\n"
        "pop %%rax\n"
        "jmp *%%rax\n"
        : : :
    );
    #if defined(__compiler_gcc)
      __unreachable();
    #endif
  }
#endif

__attribute((naked))
static sintptr_t _utility_newthread(void *stack){
  __asm__ __volatile__(
    #if defined(__i386__)
      "mov 4(%%esp), %%ecx\n"
      "mov $0x50f00, %%ebx\n"
      "mov $120, %%eax\n"
      "int $0x80\n"
      "test %%eax, %%eax\n"
      "jz gt\n"
      "ret\n"
      "gt:\n"
      "jmp *(%%esp)\n"
      : : : "eax", "ecx", "ebx", "edx", "memory"
    #elif defined(__x86_64__)
      "mov %%rdi, %%rsi\n"
      "mov $0x50f00, %%edi\n"
      "mov $56, %%eax\n"
      "syscall\n"
      "ret\n"
      : : : "rax", "rcx", "rsi", "r11", "memory"
    #else
      #error ?
    #endif
  );
  #if defined(__compiler_gcc)
    __unreachable();
  #endif
}

FUNC void *_utility_newstack(uintptr_t size){
  #if defined(__i386__)
    void *p = __generic_mmap(size);
    return (void *)((uintptr_t)p + size - sizeof(void *) * 2);
  #elif defined(__x86_64__)
    size -= sizeof(void *);
    void *p = __generic_mmap(size);
    return (void *)((uintptr_t)p + size - sizeof(void *) * 3);
  #else
    #error ?
  #endif
}

FUNC void utility_newthread(void *func, void *param){
  void *stack = _utility_newstack(0x8000);
  #if defined(__i386__)
    ((void **)stack)[0] = func;
    ((void **)stack)[1] = param;
  #elif defined(__x86_64__)
    ((void **)stack)[0] = _utility_newthread_entry;
    ((void **)stack)[1] = param;
    ((void **)stack)[2] = func;
  #else
    #error ?
  #endif
  if(_utility_newthread(stack) < 0){
    _abort();
  }
}

FUNC bool str_n0ncmp(const void *str0, const void *str1, uintptr_t str1size){
  uintptr_t str0size = MEM_cstreu(str0);

  if(str0size != str1size){
    return 1;
  }

  return MEM_cmp(str0, str1, str0size);
}

FUNC void print_padstring(uintptr_t pad_size, const void *str, uintptr_t size){
  puts_char_repeat(STDOUT, ' ', pad_size - size);
  puts(STDOUT, str, size);
}
#define print_padstring_literal(pad_size, str) \
  print_padstring(pad_size, str, sizeof(str) - 1)

FUNC uint64_t utility_log10(uint64_t num){
  uint64_t ret = 1;
  while(num /= 10){
    ret++;
  }
  return ret;
}

FUNC void utility_print_zeropad_num(uintptr_t pad_length, uint64_t num){
  puts_char_repeat(STDOUT, '0', pad_length - utility_log10(num));
  utility_puts_number(STDOUT, num);
}

FUNC void utility_print_major_minor(uint64_t major, uintptr_t minor_length, uint64_t minor){
  utility_puts_number(STDOUT, major);
  puts_char_repeat(STDOUT, '.', 1);
  utility_print_zeropad_num(minor_length, minor);
}

FUNC void print_float(const void *color, uintptr_t color_size, uint64_t num){
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

FUNC void print_sizenumber(const void *color, uintptr_t color_size, uintptr_t pad_length, uint64_t num){
  const char *sizetypestr_array =
    " kB"
    " MB"
    " GB"
    " TB"
  ;

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
  puts(STDOUT, &sizetypestr_array[DivideCount * 3], 3);
  puts_literal(STDOUT, "\e[m");
}
#define print_sizenumber(color, pad_length, num) \
  print_sizenumber("\e[" #color "m", sizeof("\e[" #color "m") - 1, pad_length, num)

FUNC void print_cmd(uintptr_t size, const uint8_t *cmd){
  puts_char_repeat(STDOUT, ' ', 1);
  puts(STDOUT, cmd, size);
}
