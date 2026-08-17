bits 64

%include "cpu.inc"

global enableSse

section .text

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
    test ecx, CPUID_FEAT_ECX_OSXSAVE
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
    ret