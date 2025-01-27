#define STDIN 0
#define STDOUT 1
#define STDERR 2

#include <WITCH/STR/STR.h>

uint8_t _utility_print_fdint;
uint16_t _utility_print_index;
uint8_t _utility_print_buffer[0x1000];

FUNC void flush_print(){
  IO_fd_t fd;
  IO_fd_set(&fd, _utility_print_fdint);
  IO_write(
    &fd,
    _utility_print_buffer,
    _utility_print_index
  );
  _utility_print_index = 0;
}
FUNC void utility_init_print(){
  _utility_print_fdint = STDERR;
  _utility_print_index = 0;
}

#define _exit(val) \
  flush_print(); \
  PR_exit(val); \
  __unreachable();

FUNC void utility_print_setfd(uint32_t fdint){
  if(_utility_print_fdint != fdint){
    flush_print();
    _utility_print_fdint = fdint;
  }
}

FUNC void puts(const void *ptr, uintptr_t size){
  for(uintptr_t i = 0; i < size; i++){
    _utility_print_buffer[_utility_print_index] = ((const uint8_t *)ptr)[i];
    if(++_utility_print_index == sizeof(_utility_print_buffer)){
      flush_print();
    }
  }
}
#define puts_literal(literal) \
  puts(literal, sizeof(literal) - 1)

FUNC void puts_char_repeat(uint8_t ch, uintptr_t size){
  while(size--){
    puts(&ch, 1);
  }
}

FUNC void utility_puts_number(uint64_t num){
  uint8_t buf[64];
  uint8_t *buf_ptr;
  uintptr_t size;

  buf_ptr = buf;
  STR_uto64(num, 10, &buf_ptr, &size);
  puts(buf_ptr, size);
}

FUNC void _abort(const char *filename, uintptr_t filename_length, uintptr_t line){
  utility_print_setfd(STDERR);

  puts_literal("err at ");
  puts(filename, filename_length);
  puts_char_repeat(':', 1);
  utility_puts_number(line);
  puts_char_repeat('\n', 1);

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

FUNC uint64_t utility_log10(uint64_t num){
  uint64_t ret = 1;
  while(num /= 10){
    ret++;
  }
  return ret;
}

FUNC void utility_print_zeropad_num(uintptr_t pad_length, uint64_t num){
  puts_char_repeat('0', pad_length - utility_log10(num));
  utility_puts_number(num);
}

FUNC void utility_print_major_minor(uint64_t major, uintptr_t minor_length, uint64_t minor){
  utility_puts_number(major);
  puts_char_repeat('.', 1);
  utility_print_zeropad_num(minor_length, minor);
}

FUNC void print_sizenumber(uint64_t num_major, uint64_t num_minor, uint8_t DivideCount){
  const char *sizetypestr_array =
    " kB"
    " MB"
    " GB"
    " TB"
  ;

  utility_print_major_minor(num_major, 2, num_minor);
  puts(&sizetypestr_array[DivideCount * 3], 3);
}

#pragma pack(push, 1)
  typedef struct{
    uint8_t length;
    bool side;
  }print_row_pad_t;
  typedef struct{
    uint8_t type;
    uint8_t color;
    union{
      struct{
        const void *str;
        uintptr_t length;
      };
      uint64_t num;
    };
  }print_row_data_t;
#pragma pack(pop)

FUNC void _print_row(
  uintptr_t size,
  const print_row_pad_t * const row_pads,
  uintptr_t data_size,
  const print_row_data_t * const row_datas
){
  for(uintptr_t i = 0; i < data_size; i++){
    puts_literal("\e[");
    utility_puts_number(row_datas[i].color);
    puts_char_repeat('m', 1);

    uintptr_t pad_size = row_pads[i % size].length;

    uint64_t num_major;
    uint64_t num_minor;
    uint8_t DivideCount;

    uintptr_t text_size;
    if(row_datas[i].type == 0){
      text_size = MEM_cstreu(row_datas[i].str);
    }
    else if(row_datas[i].type == 1){
      uint64_t num = row_datas[i].num * 100;
      DivideCount = 0;
      while(num > 1024 * 100){
        num /= 1024;
        DivideCount++;
      }

      num_major = num / 100;
      num_minor = num % 100;

      text_size = utility_log10(num_major) + 1 + 2 + 3;
    }
    else if(row_datas[i].type == 2){
      num_major = row_datas[i].num / 1000;
      num_minor = row_datas[i].num % 1000;

      text_size = utility_log10(num_major) + 1 + 3;
    }
    else if(row_datas[i].type == 3){
      text_size = row_datas[i].length;
    }
    else{
      __unreachable();
    }

    if(pad_size < text_size){
      pad_size = text_size;
    }

    if(row_pads[i % size].side == 1){
      puts_char_repeat(' ', pad_size - text_size);
    }

    if(row_datas[i].type == 0){
      puts(row_datas[i].str, text_size);
    }
    else if(row_datas[i].type == 1){
      print_sizenumber(num_major, num_minor, DivideCount);
    }
    else if(row_datas[i].type == 2){
      utility_print_major_minor(num_major, 3, num_minor);
    }
    else if(row_datas[i].type == 3){
      puts(row_datas[i].str, text_size);
    }
    else{
      __unreachable();
    }

    if(row_pads[i % size].side == 0){
      puts_char_repeat(' ', pad_size - text_size);
    }

    puts_literal("\e[m");

    puts_char_repeat(i % size + 1 != size ? ' ' : '\n', 1);
  }
}
#define print_row(pads, datas) { \
  print_row_pad_t _pads[] = pads; \
  print_row_data_t _datas[] = datas; \
  uintptr_t _print_row_size0 = sizeof(_pads) / sizeof(_pads[0]); \
  uintptr_t _print_row_size1 = sizeof(_datas) / sizeof(_datas[0]); \
  _print_row(_print_row_size0, _pads, _print_row_size1, _datas); \
}
#define print_row_def(name, pads, datas) \
  print_row_pad_t name[] = pads; \
  { \
    print_row_data_t _datas[] = datas; \
    uintptr_t _print_row_size0 = sizeof(name) / sizeof(name[0]); \
    uintptr_t _print_row_size1 = sizeof(_datas) / sizeof(_datas[0]); \
    _print_row(_print_row_size0, name, _print_row_size1, _datas); \
  }
#define print_row_call(name, datas) \
  { \
    print_row_data_t _datas[] = datas; \
    uintptr_t _print_row_size0 = sizeof(name) / sizeof(name[0]); \
    uintptr_t _print_row_size1 = sizeof(_datas) / sizeof(_datas[0]); \
    _print_row(_print_row_size0, name, _print_row_size1, _datas); \
  }

#define PRINT_ROW_PAD(length, side) \
  {length __ca__ side}
#define PRINT_ROW_PAD_(...) \
  PRINT_ROW_PAD(__VA_ARGS__) __ca__

#define PRINT_ROW_CSTR(p_color, p_cstr) \
  {.type = 0 __ca__ .color = p_color __ca__ .str = p_cstr}
#define PRINT_ROW_CSTR_(...) \
  PRINT_ROW_CSTR(__VA_ARGS__) __ca__
#define PRINT_ROW_SIZENUMBER(p_color, val) \
  {.type = 1 __ca__ .color = p_color __ca__ .num = val}
#define PRINT_ROW_SIZENUMBER_(...) \
  PRINT_ROW_SIZENUMBER(__VA_ARGS__) __ca__
#define PRINT_ROW_FLOAT(p_color, val) \
  {.type = 2 __ca__ .color = p_color __ca__ .num = val}
#define PRINT_ROW_FLOAT_(...) \
  PRINT_ROW_FLOAT(__VA_ARGS__) __ca__
#define PRINT_ROW_STR(p_color, p_str, p_length) \
  {.type = 3 __ca__ .color = p_color __ca__ .str = p_str __ca__ .length = p_length}
#define PRINT_ROW_STR_(...) \
  PRINT_ROW_STR(__VA_ARGS__) __ca__
