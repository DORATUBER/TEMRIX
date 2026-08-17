SECTIONS
{
    . = 0xFFFFFFFF80000000;

    .header : {
        QUAD(_kernel_end - 0xFFFFFFFF80000000)
    }

    .text : {
        *(.text._start)
        *(.text)
        *(.text.*)
    }

    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }

    .bss : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    }

    _kernel_end = .;
}