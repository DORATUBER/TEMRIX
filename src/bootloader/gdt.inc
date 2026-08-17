%ifndef GDT_INC
%define GDT_INC

%define KERNEL_CODE_SEL  0x08
%define KERNEL_DATA_SEL  0x10

%define GDT_ACCESS_PRESENT    (1 << 47)
%define GDT_ACCESS_NOT_SYS    (1 << 44)
%define GDT_ACCESS_EXEC       (1 << 43)
%define GDT_ACCESS_RW         (1 << 41)
%define GDT_FLAG_LONG_MODE    (1 << 53)

%define GDT_LIMIT_LOW 0xFFFF

%define KERNEL_CODE_SEG ( \
    GDT_LIMIT_LOW | \
    GDT_ACCESS_PRESENT | \
    GDT_ACCESS_NOT_SYS | \
    GDT_ACCESS_EXEC | \
    GDT_ACCESS_RW | \
    GDT_FLAG_LONG_MODE )

%define KERNEL_DATA_SEG ( \
    GDT_LIMIT_LOW | \
    GDT_ACCESS_PRESENT | \
    GDT_ACCESS_NOT_SYS | \
    GDT_ACCESS_RW )

%endif