%macro ISR_STUB 1
global %1Stub
extern %1Handler

%1Stub:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call %1Handler

    mov rsp, rax

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq
%endmacro

ISR_STUB apTimer

global syscallStub
extern syscallHandler

syscallStub:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call syscallHandler

    mov [rsp + 14*8], rax
    mov rsp, rdx

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

global pagefault_isr
extern pagefault_handler

pagefault_isr:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov rdi, [rsp + 120]    
    mov rsi, rsp            
    
    call pagefault_handler

    mov rsp, rdx             

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    
    iretq

%macro EXC_STUB_NOERR 1
global exceptionStub%1
exceptionStub%1:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov rdi, %1       
    mov rsi, rsp
    call exceptionHandlerNamed

    mov rsp, rax

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq
%endmacro

%macro EXC_STUB_ERR 1
global exceptionErrorStub%1
exceptionErrorStub%1:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov rdi, %1          
    mov rsi, rsp
    mov rdx, [rsp + 120] 
    call exceptionErrorHandlerNamed

    mov rsp, rax

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq
%endmacro

extern exceptionHandlerNamed
extern exceptionErrorHandlerNamed

EXC_STUB_NOERR 0 
EXC_STUB_NOERR 1   
EXC_STUB_NOERR 3  
EXC_STUB_NOERR 4  
EXC_STUB_NOERR 5  
EXC_STUB_NOERR 6   
EXC_STUB_NOERR 7  
EXC_STUB_NOERR 16 
EXC_STUB_NOERR 19 

EXC_STUB_ERR   10  
EXC_STUB_ERR   11  
EXC_STUB_ERR   12
EXC_STUB_ERR   13  
EXC_STUB_ERR   17 

%macro MSIX_STUB 1
global msixStub%1
extern msixHandler

msixStub%1:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov rdi, %1
    mov rsi, rsp
    call msixHandler

    mov rsp, rax

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq
%endmacro

%assign i 49
%rep 206
MSIX_STUB i
%assign i i+1
%endrep

section .data
global msixStubTable
msixStubTable:
%assign i 49
%rep 206
    dq msixStub %+ i
%assign i i+1
%endrep