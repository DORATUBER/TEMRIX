PHDRS
{
    text    PT_LOAD FLAGS(5); /* R-X */
    rodata  PT_LOAD FLAGS(4); /* R-- */
    data    PT_LOAD FLAGS(6); /* RW- */
}

SECTIONS {
    . = 0x0;

    .text : {
        *(.text .text.*)
    } :text

    . = ALIGN(0x1000);
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(0x1000);
    .data : {
        *(.data .data.*)
    } :data

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        __bss_end = .;
    } :data

    .init_array : {
        PROVIDE(__init_array_start = .);
        KEEP(*(.init_array .init_array.*))
        PROVIDE(__init_array_end = .);
    } :data

    /DISCARD/ : {
        *(.note.*)
        *(.comment)
    }
}