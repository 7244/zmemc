#define set_program_name "zmemc"
#define set_program_version "0.2.2"


#define FUNC static

#define __platform_nostdlib
#define WITCH_PRE_is_not_allowed
#define __WITCH_IO_allow_sigpipe
#define __WITCH_FS_no_cooked

#include <WITCH/WITCH.h>
#define __generic_alloc_abort
#define __generic_alloc_confident
#include <WITCH/generic_alloc.h>
#include <WITCH/PR/PR.h>
#include <WITCH/IO/IO.h>
#include <WITCH/FS/FS.h>

#include "utility.h"

enum{
  param_perprocess_e,
  param_summary_e,
  param_version_e,
  param_help_e
};

FUNC void print_small_help(){
  puts_literal(STDERR,
    "\e[4mUsage:\e[m " set_program_name " [OPTIONS]\n"
    "\n"
    "For more information, try '--help'.\n"
  );
}

FUNC void print_help(){
  puts_literal(STDOUT,
    set_program_name " is a Linux memory monitoring program that displays detailed information about virtual memory.\n"
    "\n"
    "\e[4mUsage:\e[m " set_program_name " [OPTIONS]\n"
    "\n"
    "Options:\n"
    "  -p, --per-process  Display per-process memory usage or not (default: false)\n"
    "  -s, --summary      Display a summary of memory usage or not (default: true)\n"
    "  -h, --help         Print help\n"
    "  -V, --version      Print version\n"
  );
}

#include "print_summary.h"
#include "print_perprocess.h"

__attribute__((noreturn))
void main(uintptr_t argc, const uint8_t **argv){

  #include <WITCH/PlatformOpen.h>

  utility_init_print();

  uint8_t param_enum_bits = 0;

  for(uintptr_t iarg = 1; iarg < argc; iarg++){
    const uint8_t *arg = argv[iarg];

    if(arg[0] == '-' && arg[1] != '-'){
      if(arg[1] == 0 || arg[2] != 0){
        puts_literal(STDERR,
          "\e[31merror:\e[m - parameters supposed to be one letter.\n"
          "\n"
        );
        print_small_help();
        _exit(1);
      }

      if(arg[1] == 'p'){ param_enum_bits |= 1 << param_perprocess_e; }
      else if(arg[1] == 's'){ param_enum_bits |= 1 << param_summary_e; }
      else if(arg[1] == 'V'){ param_enum_bits |= 1 << param_version_e; }
      else if(arg[1] == 'h'){ param_enum_bits |= 1 << param_help_e; }
      else{
        puts_literal(STDERR, "\e[31merror:\e[m unexpected argument '\e[33m-");
        puts_char_repeat(STDERR, arg[1], 1);
        puts_literal(STDERR, "\e[m' found\n\n");
        print_small_help();
        _exit(1);
      }
    }
    else if(arg[0] == '-' && arg[1] == '-'){
      const uint8_t *pstr = &arg[2];

      if(!STR_n0cmp("per-process", pstr)){ param_enum_bits |= 1 << param_perprocess_e; }
      else if(!STR_n0cmp("summary", pstr)){ param_enum_bits |= 1 << param_summary_e; }
      else if(!STR_n0cmp("version", pstr)){ param_enum_bits |= 1 << param_version_e; }
      else if(!STR_n0cmp("help", pstr)){ param_enum_bits |= 1 << param_help_e; }
      else{
        puts_literal(STDERR, "\e[31merror:\e[m unexpected argument '\e[33m-");
        puts(STDERR, pstr, MEM_cstreu(pstr));
        puts_literal(STDERR, "\e[m' found\n\n");
        print_small_help();
        _exit(1);
      }
    }
    else{
      puts_literal(STDERR, "all parameters need to start with - or --\n");
      _exit(1);
    }
  }

  if(__builtin_popcount(param_enum_bits) > 1){
    puts_literal(STDERR, "too many parameters.\n");
    _exit(1);
  }

  if(!param_enum_bits){
    param_enum_bits |= 1 << param_summary_e;
  }

  uint8_t bit = __builtin_ctz(param_enum_bits);

  if(bit == param_help_e){
    print_help();
  }
  if(bit == param_version_e){
    puts_literal(STDOUT, set_program_name " " set_program_version "\n");
  }
  if(bit == param_summary_e){
    print_summary();
  }
  if(bit == param_perprocess_e){
    print_summary();
    puts_char_repeat(STDOUT, '\n', 2);
    flush_print();
    print_perprocess();
  }

  _exit(0);
}
