%macro SYSCALL_WRAPPER 1
%if %1 > 6
    %error "syscall wrapper arity > 6 not supported"
%endif
global syscall%1
syscall%1:
%if %1 >= 6
    mov r10, [rsp + 8] 
%endif
    int 0x80
    ret
%endmacro

SYSCALL_WRAPPER 0
SYSCALL_WRAPPER 1
SYSCALL_WRAPPER 2
SYSCALL_WRAPPER 3
SYSCALL_WRAPPER 4
SYSCALL_WRAPPER 5
SYSCALL_WRAPPER 6