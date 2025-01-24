#pragma pack(push, 1)
  typedef struct{
    uint64_t swap;
    uint64_t uss;
    uint64_t pss;
    uint64_t rss;
  }ProcessMemoryStats_t;

  typedef struct{
    uint8_t length;
    uint8_t data[10];
  }pid_string_t;

  typedef struct{
    /* zmem stores pid as u32. */
    /* better store as char[10] to avoid conversions */
    pid_string_t pid;

    /* zmem uses string, linux uses char[32] */
    uint8_t username_size;
    uint8_t username[32];

    /* zmem uses string but then truncates it to 50 */
    /* better use char[50] */
    uint8_t command_size;
    uint8_t command[50];

    ProcessMemoryStats_t mem;
  }ProcessStats_t;
#pragma pack(pop)

#define BDBT_set_prefix psbdbt
#define BDBT_set_type_node uint32_t
#define BDBT_set_KeySize sizeof(uint32_t) * 8
#include <BDBT/BDBT.h>

#define BLL_set_prefix psbll
#define BLL_set_type_node uint32_t
#define BLL_set_IntegerNR 1
#define BLL_set_NodeDataType ProcessStats_t
#define BLL_set_Link 1
#define BLL_set_LinkSentinel 0
#include <BLL/BLL.h>

typedef struct{
  uint32_t pidbits;
  uint8_t is_producing;
  uint8_t spinlock[32];
  uint8_t refcount[32];
  pid_string_t pid[32][16];
  uint16_t totalref;

  uint8_t sort_spinlock;
  psbll_t *psbll;
  psbdbt_t *psbdbt;

  uint32_t current_thread_count;
  uint32_t maximum_thread_count;

  FS_dir_traverse_t dirtra;

  void *gt_main_stack[5];

  uint32_t rand_num;
}perprocess_threadglobal_t;

FUNC void perprocess_perthread_process(perprocess_threadglobal_t *tg, pid_string_t pid){

  /* / proc / pid / smaps_rollup \0 */
  /* / proc / pid / cmdline \0 */
  uint8_t full_path[1 + 4 + 1 + 10 + 1 + 12 + 1] = "/proc/";
  uint8_t *procslash = &full_path[1 + 4 + 1];

  ProcessStats_t ProcessStats;
  __builtin_memset(&ProcessStats.mem, 0, sizeof(ProcessStats.mem));

  ProcessStats.pid = pid;

  __builtin_memcpy(procslash, pid.data, pid.length);

  procslash[pid.length] = '/';

  uint8_t *pidslash = &procslash[pid.length + 1];

  {
    __builtin_memcpy(pidslash, "smaps_rollup", 12);
    pidslash[12] = 0;

    IO_fd_t fd;
    if(IO_open(full_path, O_RDONLY, &fd) != 0){
      return;
    }

    uint8_t buffer[0x1000];
    IO_size_t read_size = (IO_size_t)IO_read(&fd, buffer, sizeof(buffer));

    IO_close(&fd);

    if((IO_ssize_t)read_size < 0){
      return;
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
      return;
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

  uint32_t sort_value = ProcessStats.mem.swap;

  psbdbt_KeySize_t ki = 0;
  psbdbt_NodeReference_t _psbdbt_nr = 0; /* root */
  psbdbt_NodeReference_t *psbdbt_nr = &_psbdbt_nr;

  while(__atomic_exchange_n(&tg->sort_spinlock, 1, __ATOMIC_SEQ_CST));

  psbll_NodeReference_t nr = psbll_NewNode(tg->psbll);
  psbll_GetNodeByReference(tg->psbll, nr)->data = ProcessStats;

  psbdbt_Query(tg->psbdbt, true, &sort_value, &ki, &psbdbt_nr);

  if(ki != sizeof(uint32_t) * 8){
    psbdbt_Add(tg->psbdbt, true, &sort_value, ki, *psbdbt_nr, nr);
    psbll_sicpl(tg->psbll, nr);
  }
  else{
    psbll_linkNextOfOrphan(tg->psbll, *psbdbt_nr, nr);
    *psbdbt_nr = nr;
  }

  __atomic_exchange_n(&tg->sort_spinlock, 0, __ATOMIC_SEQ_CST);
}

FUNC void perprocess_perthread_entry(perprocess_threadglobal_t *);

FUNC void perprocess_perthread_produce(perprocess_threadglobal_t *tg){
  gt_begin:;

  const char *cstr;
  if(FS_dir_traverse(&tg->dirtra, &cstr)){
    if(__atomic_load_n(&tg->pidbits, __ATOMIC_SEQ_CST)){
      return;
    }
    goto gt_end;
  }

  uintptr_t i = 0;
  while(cstr[i] != 0){
    if(STR_ischar_digit(cstr[i]) == 0){
      break;
    }
    i++;
  }
  if(cstr[i] != 0){
    /* not fully digit file name */
    goto gt_begin;
  }

  pid_string_t pid;
  pid.length = i;
  __builtin_memcpy(pid.data, cstr, i);

  while(1){
    uint8_t index = tg->rand_num++ % (sizeof(uint32_t) * 8);

    while(__atomic_exchange_n(&tg->spinlock[index], 1, __ATOMIC_SEQ_CST));

    if(tg->refcount[index] == 16){
      continue;
    }

    tg->pid[index][tg->refcount[index]++] = pid;

    __atomic_fetch_or(&tg->pidbits, (uint32_t)1 << index, __ATOMIC_SEQ_CST);

    __atomic_exchange_n(&tg->spinlock[index], 0, __ATOMIC_SEQ_CST);

    break;
  }

  uint32_t totalref = __atomic_add_fetch(&tg->totalref, 1, __ATOMIC_SEQ_CST);

  while(
    totalref > tg->current_thread_count &&
    tg->current_thread_count != tg->maximum_thread_count
  ){
    utility_newthread(perprocess_perthread_entry, tg);
    tg->current_thread_count++;

    /* opening thread takes long, lets update value */
    totalref = __atomic_load_n(&tg->totalref, __ATOMIC_SEQ_CST);
  }

  if(totalref < tg->current_thread_count * 8){
    if(totalref < 0x100){
      goto gt_begin;
    }
  }

  return;

  gt_end:;

  if(!__atomic_sub_fetch(&tg->current_thread_count, 1, __ATOMIC_SEQ_CST)){
    __builtin_longjmp(tg->gt_main_stack, 1);
  }

  __atomic_exchange_n(&tg->is_producing, 0, __ATOMIC_SEQ_CST);

  syscall1(__NR_exit, 0);
  __unreachable();
}

FUNC void perprocess_perthread_entry(perprocess_threadglobal_t *tg){
  gt_begin:;

  uint32_t pidbits = __atomic_load_n(&tg->pidbits, __ATOMIC_SEQ_CST);

  if(!pidbits){
    if(__atomic_exchange_n(&tg->is_producing, 1, __ATOMIC_SEQ_CST)){
      goto gt_begin;
    }

    perprocess_perthread_produce(tg);

    __atomic_exchange_n(&tg->is_producing, 0, __ATOMIC_SEQ_CST);

    goto gt_begin;
  }

  uint32_t pid_ctz = __builtin_ctz(pidbits);

  if(__atomic_exchange_n(&tg->spinlock[pid_ctz], 1, __ATOMIC_SEQ_CST)){
    goto gt_begin;
  }

  if(!(__atomic_load_n(&tg->pidbits, __ATOMIC_SEQ_CST) & (uint32_t)1 << pid_ctz)){
    /* we didnt lock in time :< */
    __atomic_exchange_n(&tg->spinlock[pid_ctz], 0, __ATOMIC_SEQ_CST);
    goto gt_begin;
  }

  pid_string_t pid = tg->pid[pid_ctz][--tg->refcount[pid_ctz]];

  if(!tg->refcount[pid_ctz]){
    __atomic_fetch_xor(&tg->pidbits, (uint32_t)1 << pid_ctz, __ATOMIC_SEQ_CST);
  }

  __atomic_exchange_n(&tg->spinlock[pid_ctz], 0, __ATOMIC_SEQ_CST);

  __atomic_sub_fetch(&tg->totalref, 1, __ATOMIC_SEQ_CST);

  perprocess_perthread_process(tg, pid);

  goto gt_begin;
}

FUNC void print_perprocess(){

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

  /* it gonna be 0 */
  psbdbt_NewNode(&psbdbt);

  {
    perprocess_threadglobal_t tg;
    /* easier than setting many elements 0 */
    __builtin_memset(&tg, 0, sizeof(tg));

    tg.psbll = &psbll;
    tg.psbdbt = &psbdbt;

    tg.current_thread_count = 1;
    tg.maximum_thread_count = WITCH_num_online_cpus();

    uint8_t dirbuffer[0x1000];
    if(FS_dir_traverse_open(&dir, dirbuffer, sizeof(dirbuffer), &tg.dirtra) != 0){
      _abort();
    }

    if(!__builtin_setjmp(tg.gt_main_stack)){
      perprocess_perthread_entry(&tg);
    }

    FS_dir_traverse_close(&tg.dirtra);
  }

  FS_dir_close(&dir);

  {
    uint32_t traversed_key;

    psbdbt_Traverse_t tra;
    psbdbt_TraverseInit(&tra, psbdbt_BitOrderLow, 0);
    while(psbdbt_Traverse(
      &psbdbt,
      &tra,
      true,
      psbdbt_BitOrderLow,
      &traversed_key
    )){
      psbll_NodeReference_t nr = tra.Output;

      do{
        psbll_Node_t *n = psbll_GetNodeByReference(&psbll, nr);

        ProcessStats_t *ps = &n->data;

        print_padstring(10, ps->pid.data, ps->pid.length);
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
