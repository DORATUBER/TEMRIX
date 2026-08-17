bits 64

%include "cpu.inc"
%include "gdt.inc"

extern bootMain
global _start
global enableSse
global setupGdt

section .text

_start:
    sub  rsp, 40               
    call bootMain
    add  rsp, 40
.halt:
    hlt
    jmp  .halt

enableSse:
    mov  eax, 1
    cpuid
    test edx, CPUID_FEAT_EDX_SSE
    jz   .no_sse

    mov rax, cr0
    and eax, ~(CR0_EM)
    or  eax, CR0_MP
    mov cr0, rax

    mov  rax, cr4
    or   rax, (CR4_OSFXSR | CR4_OSXMMEXCPT)
    mov  cr4, rax

    mov  eax, 1
    cpuid
    test ecx, CPUID_FEAT_ECX_AVX
    jz   .done
    test ecx, CPUID_FEAT_ECX_XSAVE 
    jz   .done     

    mov  rax, cr4
    or   rax, CR4_OSXSAVE
    mov  cr4, rax

    xor  rcx, rcx
    xgetbv
    or   eax, (XCR0_X87 | XCR0_SSE | XCR0_AVX)
    xor  edx, edx
    xsetbv

.done:
    ret

.no_sse:
.sse_halt:
    hlt
    jmp  .sse_halt

setupGdt:
    lea  rax, [rel gdt_start]
    mov  [rel gdt_descriptor + 2], rax

    cli
    lgdt [rel gdt_descriptor]

    push KERNEL_CODE_SEL
    lea  rax, [rel .reloadCs]
    push rax
    retfq

.reloadCs:
    xor  ax, ax
    mov  ss, ax
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    ret

section .data
align 8
gdt_start:
    dq 0                      
    dq KERNEL_CODE_SEG         
    dq KERNEL_DATA_SEG         
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1 
    dq 0                      

section .bss
align 4096