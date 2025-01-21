.intel_syntax noprefix
.text
  .globl _start

  _start:
    mov esi, [esp + 0]
    lea ecx, [esp + 4]
    and esp, -16
    sub esp, 8
    push ecx
    push esi
    call main
