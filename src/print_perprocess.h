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
#define BDBT_set_Recycle 0
#include <BDBT/BDBT.h>

#define BLL_set_prefix psbll
#define BLL_set_type_node uint32_t
#define BLL_set_IntegerNR 1
#define BLL_set_NodeDataType ProcessStats_t
#define BLL_set_Link 1
#define BLL_set_LinkSentinel 0
#define BLL_set_OnlyNextLink 1
#define BLL_set_Recycle 0
#include <BLL/BLL.h>

psbll_t psbll;
psbdbt_t psbdbt;

#define BME_set_Prefix slock
#define BME_set_LockValue 1
#define BME_set_Sleep 0
#if set_Verbose
  #define BME_set_CountLockFail 1
#endif
#include <BME/BME.h>

typedef struct{
  uint32_t pidbits;
  uint8_t is_producing;
  slock_t slocks[32];
  uint8_t refcount[32];
  pid_string_t pid[32][16];
  uint16_t totalref;

  #if set_Verbose
    uint64_t slocks_LockedForNothing;
  #endif

  slock_t sort_slock;

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
        if(buffer[i] == ':'){
          break;
        }
        i++;
      }
      if(i == read_size){
        break;
      }

      uintptr_t memtypestrsize = &buffer[i] - memtypestr;

      uint64_t *ptrmem = NULL;

      if(!str_n0ncmp("Rss", memtypestr, memtypestrsize)){ ptrmem = &ProcessStats.mem.rss; }
      else if(!str_n0ncmp("Pss", memtypestr, memtypestrsize)){ ptrmem = &ProcessStats.mem.pss; }
      else if(!str_n0ncmp("Private_Clean", memtypestr, memtypestrsize)){ ptrmem = &ProcessStats.mem.uss; }
      else if(!str_n0ncmp("Private_Dirty", memtypestr, memtypestrsize)){ ptrmem = &ProcessStats.mem.uss; }
      else if(!str_n0ncmp("Swap", memtypestr, memtypestrsize)){ ptrmem = &ProcessStats.mem.swap; }

      i++; /* passed colon after memtype */

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

  while(slock_Lock(&tg->sort_slock));

  psbll_NodeReference_t nr = psbll_NewNode(&psbll);
  psbll_GetNodeByReference(&psbll, nr)->data = ProcessStats;

  psbdbt_Query(&psbdbt, true, &sort_value, &ki, &psbdbt_nr);

  if(ki != sizeof(uint32_t) * 8){
    psbdbt_Add(&psbdbt, true, &sort_value, ki, *psbdbt_nr, nr);
    psbll_sicnl(&psbll, nr);
  }
  else{
    psbll_linkNextOfOrphan(&psbll, nr, *psbdbt_nr);
    *psbdbt_nr = nr;
  }

  slock_Unlock(&tg->sort_slock);
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

    while(slock_Lock(&tg->slocks[index]));

    if(tg->refcount[index] == 16){
      continue;
    }

    tg->pid[index][tg->refcount[index]++] = pid;

    __atomic_fetch_or(&tg->pidbits, (uint32_t)1 << index, __ATOMIC_SEQ_CST);

    slock_Unlock(&tg->slocks[index]);

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

  if(slock_Lock(&tg->slocks[pid_ctz])){
    goto gt_begin;
  }

  if(!tg->refcount[pid_ctz]){
    /* we didnt lock in time :< */
    #if set_Verbose
      __atomic_add_fetch(&tg->slocks_LockedForNothing, 1, __ATOMIC_SEQ_CST);
    #endif
    slock_Unlock(&tg->slocks[pid_ctz]);
    goto gt_begin;
  }

  pid_string_t pid = tg->pid[pid_ctz][--tg->refcount[pid_ctz]];

  if(!tg->refcount[pid_ctz]){
    __atomic_fetch_xor(&tg->pidbits, (uint32_t)1 << pid_ctz, __ATOMIC_SEQ_CST);
  }

  slock_Unlock(&tg->slocks[pid_ctz]);

  __atomic_sub_fetch(&tg->totalref, 1, __ATOMIC_SEQ_CST);

  perprocess_perthread_process(tg, pid);

  goto gt_begin;
}

FUNC void print_perprocess(){

  print_row_def(rule_row, {
    PRINT_ROW_PAD_(10, 1)
    PRINT_ROW_PAD_(14, 1)
    PRINT_ROW_PAD_(14, 1)
    PRINT_ROW_PAD_(14, 1)
    PRINT_ROW_PAD_(14, 1)
    PRINT_ROW_PAD(14, 1)
  }, {
    PRINT_ROW_CSTR_(0, "PID")
    PRINT_ROW_CSTR_(0, "Swap")
    PRINT_ROW_CSTR_(0, "USS")
    PRINT_ROW_CSTR_(0, "PSS")
    PRINT_ROW_CSTR_(0, "RSS")
    PRINT_ROW_CSTR(0, "COMMAND")
  });

  rule_row[5].length = 0;

  FS_dir_t dir;
  if(FS_dir_open("/proc", 0, &dir) != 0){
    _abort();
  }

  psbll_Open(&psbll);

  psbdbt_Open(&psbdbt);

  /* it gonna be 0 */
  psbdbt_NewNode(&psbdbt);

  {
    perprocess_threadglobal_t tg;
    /* easier than setting many elements 0 */
    __builtin_memset(&tg, 0, sizeof(tg));

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

    #if set_Verbose
      for(uint32_t i = 0; i < sizeof(tg.slocks) / sizeof(tg.slocks[0]); i++){
        puts_literal("[DEBUG] slocks ");
        utility_puts_number(i);
        puts_char_repeat(' ', 1);
        utility_puts_number(tg.slocks[i].LockFailCount);
        puts_char_repeat('\n', 1);
      }
      puts_literal("[DEBUG] slocks_LockedForNothing ");
      utility_puts_number(tg.slocks_LockedForNothing);
      puts_char_repeat('\n', 1);
      puts_literal("[DEBUG] sort_slock ");
      utility_puts_number(tg.sort_slock.LockFailCount);
      puts_char_repeat('\n', 1);
    #endif
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

        print_row_call(rule_row, {
          PRINT_ROW_STR_(0, ps->pid.data, ps->pid.length)
          PRINT_ROW_SIZENUMBER_(91, ps->mem.swap)
          PRINT_ROW_SIZENUMBER_(32, ps->mem.uss)
          PRINT_ROW_SIZENUMBER_(34, ps->mem.pss)
          PRINT_ROW_SIZENUMBER_(36, ps->mem.rss)
          PRINT_ROW_STR(0, ps->command, ps->command_size)
        });

        nr = n->NextNodeReference;
      }while(!psbll_inric(nr));
    }
  }

  psbdbt_Close(&psbdbt);

  psbll_Close(&psbll);
}
