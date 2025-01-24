typedef struct{
  uint64_t total;
  uint64_t free;
  uint64_t available;
  uint64_t used;
  uint64_t shared;
  uint64_t buffers;
  uint64_t cached;
  uint64_t swap_total;
  uint64_t swap_free;
  uint64_t zswap;
  uint64_t zswap_compressed;
  uint64_t swap_cached;
  uint64_t compression_ratio;
  uint64_t swap_used;
  uint64_t swap_available;
  uint64_t totalvmem;
  uint64_t freevmem;
  uint64_t usedvmem;
  uint64_t availablevmem;
  uint64_t swap_on_disk;
}MemoryStats_t;

FUNC void print_summary(){
  IO_fd_t fd_meminfo;
  if(IO_open("/proc/meminfo", O_RDONLY, &fd_meminfo) != 0){
    _abort();
  }

  uint8_t data[0x2000];
  IO_ssize_t read_size = IO_read(&fd_meminfo, data, sizeof(data));
  if(read_size < 0){
    _abort();
  }
  if(read_size == sizeof(data)){
    /* we wanted single call read :< */
    _abort();
  }

  IO_close(&fd_meminfo);

  MemoryStats_t MemoryStats;
  __builtin_memset(&MemoryStats, 0, sizeof(MemoryStats));

  for(IO_ssize_t i = 0;;){
    const uint8_t *memtypestr = &data[i];
    while(i < read_size){
      if(data[i] == ' '){
        break;
      }
      i++;
    }
    if(i == read_size){
      break;
    }

    uintptr_t memtypestrsize = &data[i] - memtypestr;

    uint64_t *ptrmem = NULL;

    if(!str_n0ncmp("MemTotal:", memtypestr, memtypestrsize)){ ptrmem = &MemoryStats.total; }
    else if(!str_n0ncmp("MemFree:", memtypestr, memtypestrsize)){ ptrmem = &MemoryStats.free; }
    else if(!str_n0ncmp("MemAvailable:", memtypestr, memtypestrsize)){ ptrmem = &MemoryStats.available; }
    else if(!str_n0ncmp("MemUsed:", memtypestr, memtypestrsize)){ ptrmem = &MemoryStats.used; }
    else if(!str_n0ncmp("Shmem:", memtypestr, memtypestrsize)){ ptrmem = &MemoryStats.shared; }
    else if(!str_n0ncmp("Buffers:", memtypestr, memtypestrsize)){ ptrmem = &MemoryStats.buffers; }
    else if(!str_n0ncmp("Cached:", memtypestr, memtypestrsize)){ ptrmem = &MemoryStats.cached; }
    else if(!str_n0ncmp("SwapTotal:", memtypestr, memtypestrsize)){ ptrmem = &MemoryStats.swap_total; }
    else if(!str_n0ncmp("SwapFree:", memtypestr, memtypestrsize)){ ptrmem = &MemoryStats.swap_free; }
    else if(!str_n0ncmp("Zswap:", memtypestr, memtypestrsize)){ ptrmem = &MemoryStats.zswap_compressed; }
    else if(!str_n0ncmp("Zswapped:", memtypestr, memtypestrsize)){ ptrmem = &MemoryStats.zswap; }
    else if(!str_n0ncmp("SwapCached:", memtypestr, memtypestrsize)){ ptrmem = &MemoryStats.swap_cached; }
    /* everything here requires kB or size type. next code also expects that */
    /* if you gonna add HugePages_* or something that doesnt contain size type in future, */
    /* you also need to change next code. */

    i++; /* passed one space after memtype */

    /* this line is useless for us, lets skip line */
    if(ptrmem == NULL){
      while(i < read_size){
        if(data[i] == '\n'){
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
      if(data[i] != ' '){
        break;
      }
      i++;
    }
    if(i == read_size){
      _abort();
    }

    const uint8_t *numstr = &data[i];
    while(i < read_size){
      if(!STR_ischar_digit(data[i])){
        break;
      }
      i++;
    }
    if(i == read_size){
      _abort();
    }

    uintptr_t numstrsize = &data[i] - numstr;

    *ptrmem = STR_psu64(numstr, numstrsize);

    /* this part expect size type. yes. */
    const uint8_t *endingstr = &data[i];
    while(i < read_size){
      if(data[i++] == '\n'){
        break;
      }
    }
    uintptr_t endingstrsize = &data[i] - endingstr;
    if(str_n0ncmp(" kB\n", endingstr, endingstrsize)){
      _abort();
    }
  }

  MemoryStats.used = MemoryStats.total - MemoryStats.free - MemoryStats.buffers - MemoryStats.cached;
  MemoryStats.swap_used = MemoryStats.swap_total - MemoryStats.swap_free;
  MemoryStats.swap_available = MemoryStats.swap_total - MemoryStats.swap_used;
  MemoryStats.compression_ratio = MemoryStats.zswap * 1000 / MemoryStats.zswap_compressed;
  MemoryStats.totalvmem = MemoryStats.total + MemoryStats.swap_total;
  MemoryStats.freevmem = MemoryStats.free + MemoryStats.swap_free;
  MemoryStats.usedvmem = MemoryStats.used + MemoryStats.swap_used;
  MemoryStats.availablevmem = MemoryStats.available + MemoryStats.swap_available;
  MemoryStats.swap_on_disk = MemoryStats.swap_used - MemoryStats.zswap;

  print_padstring_literal(8, "");
  puts_char_repeat(STDOUT, ' ', 1);
  print_padstring_literal(13, "total");
  puts_char_repeat(STDOUT, ' ', 1);
  print_padstring_literal(13, "used");
  puts_char_repeat(STDOUT, ' ', 1);
  print_padstring_literal(13, "free");
  puts_char_repeat(STDOUT, ' ', 1);
  print_padstring_literal(13, "shared");
  puts_char_repeat(STDOUT, ' ', 1);
  print_padstring_literal(13, "buff/cache");
  puts_char_repeat(STDOUT, ' ', 1);
  print_padstring_literal(13, "available");
  puts_char_repeat(STDOUT, '\n', 1);

  puts_literal(STDOUT, "\e[94mMem:    \e[m");
  print_sizenumber(32, 13, MemoryStats.total);
  print_sizenumber(91, 13, MemoryStats.used);
  print_sizenumber(34, 13, MemoryStats.free);
  print_sizenumber(33, 13, MemoryStats.shared + MemoryStats.buffers);
  print_sizenumber(35, 13, MemoryStats.cached);
  print_sizenumber(34, 13, MemoryStats.available);
  puts_literal(STDOUT, "\n");

  puts_literal(STDOUT, "\e[95mSwap:   \e[m");
  print_sizenumber(32, 13, MemoryStats.swap_total);
  print_sizenumber(91, 13, MemoryStats.swap_used);
  print_sizenumber(34, 13, MemoryStats.swap_free);
  print_sizenumber(33, 13, 0);
  print_sizenumber(35, 13, MemoryStats.swap_cached);
  print_sizenumber(34, 13, MemoryStats.swap_available);
  puts_literal(STDOUT, "\n");

  puts_literal(STDOUT, "\e[94mTotal:  \e[m");
  print_sizenumber(32, 13, MemoryStats.totalvmem);
  print_sizenumber(91, 13, MemoryStats.usedvmem);
  print_sizenumber(34, 13, MemoryStats.freevmem);
  print_sizenumber(33, 13, MemoryStats.shared + MemoryStats.buffers);
  print_sizenumber(35, 13, MemoryStats.cached);
  print_sizenumber(34, 13, MemoryStats.availablevmem);
  puts_literal(STDOUT, "\n");


  print_padstring_literal(8, "");
  puts_char_repeat(STDOUT, ' ', 1);
  print_padstring_literal(13, "Zswap");
  puts_char_repeat(STDOUT, ' ', 1);
  print_padstring_literal(13, "Compressed");
  puts_char_repeat(STDOUT, ' ', 1);
  print_padstring_literal(13, "Ratio");
  puts_char_repeat(STDOUT, ' ', 1);
  print_padstring_literal(13, "On Disk");
  puts_char_repeat(STDOUT, '\n', 1);

  puts_literal(STDOUT, "\e[95mZswap:  \e[m");
  print_sizenumber(32, 13, MemoryStats.zswap);
  print_sizenumber(91, 13, MemoryStats.zswap_compressed);
  print_float(34, MemoryStats.compression_ratio);
  print_sizenumber(33, 13, MemoryStats.swap_on_disk);
  puts_char_repeat(STDOUT, '\n', 1);
}
