.intel_syntax noprefix
.text
  .globl _start

  _start:
    mov rdi, [rsp + 0]
    lea rsi, [rsp + 8]
    call main
