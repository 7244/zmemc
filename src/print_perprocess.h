typedef struct{
  uint64_t swap;
  uint64_t uss;
  uint64_t pss;
  uint64_t rss;
}ProcessMemoryStats_t;

typedef struct{
  /* zmem stores pid as u32. */
  /* better store as char[10] to avoid conversions */
  uint8_t pid_size;
  uint8_t pid[10];

  /* zmem uses string, linux uses char[32] */
  uint8_t username_size;
  uint8_t username[32];

  /* zmem uses string but then truncates it to 50 */
  /* better use char[50] */
  uint8_t command_size;
  uint8_t command[50];

  ProcessMemoryStats_t mem;
}ProcessStats_t;

#define BDBT_set_prefix psbdbt
#define BDBT_set_type_node uint32_t
#define BDBT_set_MaxKeySize 32
#include <BDBT/BDBT.h>

#define BLL_set_prefix psbll
#define BLL_set_type_node uint32_t
#define BLL_set_IntegerNR 1
#define BLL_set_NodeDataType ProcessStats_t
#define BLL_set_Link 1
#define BLL_set_LinkSentinel 0
#include <BLL/BLL.h>

void print_perprocess(){

  print_padstring_literal(10, "PID");
  puts_char_repeat(STDOUT, ' ', 1);
  print_padstring_literal(14, "Swap");
  puts_char_repeat(STDOUT, ' ', 1);
  print_padstring_literal(14, "USS");
  puts_char_repeat(STDOUT, ' ', 1);
  print_padstring_literal(14, "PSS");
  puts_char_repeat(STDOUT, ' ', 1);
  print_padstring_literal(14, "RSS");
  puts_char_repeat(STDOUT, ' ', 1);
  print_padstring_literal(14, "COMMAND");
  puts_char_repeat(STDOUT, '\n', 1);

  FS_dir_t dir;
  if(FS_dir_open("/proc", 0, &dir) != 0){
    _abort();
  }

  psbll_t psbll;
  psbll_Open(&psbll);

  psbdbt_t psbdbt;
  psbdbt_Open(&psbdbt);

  psbdbt_NodeReference_t psbdbt_root = psbdbt_NewNode(&psbdbt);

  {
    uint8_t dirbuffer[0x1000];
    FS_dir_traverse_t dirtra;
    if(FS_dir_traverse_open(&dir, dirbuffer, sizeof(dirbuffer), &dirtra) != 0){
      _abort();
    }

    /* / proc / pid / smaps_rollup \0 */
    /* / proc / pid / cmdline \0 */
    uint8_t full_path[1 + 4 + 1 + 10 + 1 + 12 + 1] = "/proc/";
    uint8_t *procslash = &full_path[1 + 4 + 1];

    const char *cstr;
    while(!FS_dir_traverse(&dirtra, &cstr)){
      uintptr_t i = 0;
      while(cstr[i] != 0){
        if(STR_ischar_digit(cstr[i]) == 0){
          break;
        }
        i++;
      }
      if(cstr[i] != 0){
        /* not fully digit file name */
        continue;
      }

      ProcessStats_t ProcessStats;
      __builtin_memset(&ProcessStats.mem, 0, sizeof(ProcessStats.mem));

      {
        __builtin_memcpy(ProcessStats.pid, cstr, i);
        ProcessStats.pid_size = i;
      }

      __builtin_memcpy(procslash, cstr, i);
      procslash[i++] = '/';

      uint8_t *pidslash = &procslash[i];

      {
        __builtin_memcpy(pidslash, "smaps_rollup", 12);
        pidslash[12] = 0;

        IO_fd_t fd;
        if(IO_open(full_path, O_RDONLY, &fd) != 0){
          continue;
        }

        uint8_t buffer[0x1000];
        IO_size_t read_size = (IO_size_t)IO_read(&fd, buffer, sizeof(buffer));

        IO_close(&fd);

        if((IO_ssize_t)read_size < 0){
          continue;
        }
        if(read_size == sizeof(buffer)){
          /* we wanted single call :< */
          _abort();
        }

        IO_size_t i = 0;

        /* we pass first line */
        while(i < read_size){
          if(buffer[i++] == '\n'){
            break;
          }
        }

        while(1){
          const uint8_t *memtypestr = &buffer[i];
          while(i < read_size){
            if(buffer[i] == ' '){
              break;
            }
            i++;
          }
          if(i == read_size){
            break;
          }

          uintptr_t memtypestrsize = &buffer[i] - memtypestr;

          uint64_t *ptrmem = NULL;

          if(!str_n0ncmp("Rss:", memtypestr, memtypestrsize)){ ptrmem = &ProcessStats.mem.rss; }
          else if(!str_n0ncmp("Pss:", memtypestr, memtypestrsize)){ ptrmem = &ProcessStats.mem.pss; }
          else if(!str_n0ncmp("Private_Clean:", memtypestr, memtypestrsize)){ ptrmem = &ProcessStats.mem.uss; }
          else if(!str_n0ncmp("Private_Dirty:", memtypestr, memtypestrsize)){ ptrmem = &ProcessStats.mem.uss; }
          else if(!str_n0ncmp("Swap:", memtypestr, memtypestrsize)){ ptrmem = &ProcessStats.mem.swap; }

          i++; /* passed one space after memtype */

          /* this line is useless for us, lets skip line */
          if(ptrmem == NULL){
            while(i < read_size){
              if(buffer[i] == '\n'){
                break;
              }
              i++;
            }
            if(i == read_size){
              break;
            }
            i++;
            continue;
          }

          while(i < read_size){
            if(buffer[i] != ' '){
              break;
            }
            i++;
          }
          if(i == read_size){
            _abort();
          }

          const uint8_t *numstr = &buffer[i];
          while(i < read_size){
            if(!STR_ischar_digit(buffer[i])){
              break;
            }
            i++;
          }
          if(i == read_size){
            _abort();
          }

          uintptr_t numstrsize = &buffer[i] - numstr;

          *ptrmem += STR_psu64(numstr, numstrsize);

          /* this part expect size type. yes. */
          const uint8_t *endingstr = &buffer[i];
          while(i < read_size){
            if(buffer[i++] == '\n'){
              break;
            }
          }
          uintptr_t endingstrsize = &buffer[i] - endingstr;
          if(str_n0ncmp(" kB\n", endingstr, endingstrsize)){
            _abort();
          }
        }
      }

      { /* get_cmd */
        __builtin_memcpy(pidslash, "cmdline", 7);
        pidslash[7] = 0;

        IO_fd_t fd;
        if(IO_open(full_path, O_RDONLY, &fd) != 0){
          continue;
        }

        uint8_t buffer[0x1000];
        IO_ssize_t read_size = IO_read(&fd, ProcessStats.command, sizeof(ProcessStats.command));

        IO_close(&fd);

        ProcessStats.command_size = read_size;

        if(read_size < 0){
          ProcessStats.command_size = 0;
        }

        for(uintptr_t i = 0; i < ProcessStats.command_size; i++){
          if(ProcessStats.command[i] == 0){
            ProcessStats.command[i] = ' ';
          }
        }
      }

      psbll_NodeReference_t nr = psbll_NewNode(&psbll);
      psbll_GetNodeByReference(&psbll, nr)->data = ProcessStats;

      uint32_t sort_value = ProcessStats.mem.swap;

      psbdbt_KeySize_t ki = 0;
      psbdbt_NodeReference_t *psbdbt_nr = &psbdbt_root;

      psbdbt_Query(&psbdbt, true, sizeof(uint32_t) * 8, &sort_value, &ki, &psbdbt_nr);

      if(ki != sizeof(uint32_t) * 8){
        psbdbt_Add(&psbdbt, true, sizeof(uint32_t) * 8, &sort_value, ki, *psbdbt_nr, nr);
        psbll_sicpl(&psbll, nr);
      }
      else{
        psbll_linkNextOfOrphan(&psbll, *psbdbt_nr, nr);
        *psbdbt_nr = nr;
      }
    }

    FS_dir_traverse_close(&dirtra);
  }

  FS_dir_close(&dir);

  {
    uint32_t traversed_key;

    psbdbt_Traverse_t tra;
    psbdbt_TraverseInit(&tra, psbdbt_BitOrderLow, psbdbt_root);
    while(psbdbt_Traverse(
      &psbdbt,
      &tra,
      true,
      psbdbt_BitOrderLow,
      sizeof(uint32_t) * 8,
      &traversed_key
    )){
      psbll_NodeReference_t nr = tra.Output;

      do{
        psbll_Node_t *n = psbll_GetNodeByReference(&psbll, nr);

        ProcessStats_t *ps = &n->data;

        print_padstring(10, ps->pid, ps->pid_size);
        print_sizenumber(91, 14, ps->mem.swap);
        print_sizenumber(32, 14, ps->mem.uss);
        print_sizenumber(34, 14, ps->mem.pss);
        print_sizenumber(36, 14, ps->mem.rss);
        print_cmd(ps->command_size, ps->command);
        puts_char_repeat(STDOUT, '\n', 1);

        nr = n->PrevNodeReference;
      }while(!psbll_inric(nr));
    }
  }

  psbdbt_Close(&psbdbt);

  psbll_Close(&psbll);
}
