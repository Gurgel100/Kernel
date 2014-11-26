OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
/*  Bei _start soll die Ausfuehrung losgehen */
ENTRY(_start)

/*
 * Hier wird festgelegt, in welcher Reihenfolge welche Sektionen in die Binary
 * geschrieben werden sollen
 */
SECTIONS
{
    /*
     * . ist die aktuelle Position in der Datei. Wir wollen den Kernel wie gehabt
     * an 1 MB laden, also muessen wir dort die erste Sektion hinlegen
     */
    . = 0x100000;
	kernel_start = .;

    /*
     * Der Multiboot-Header muss zuerst kommen (in den ersten 8 kB).
     * Die Standardsektionen einfach hintereinander weg einbinden.
     */
     kernel_code_start = .;
    .text : {
        /*(multiboot) kein Multibootheader vorhanden.*/
        *(.text)
    }
    kernel_code_end = .;
    .data ALIGN(4096) : {
        *(.data)
    }
    /*cdi_drivers ALIGN(4096) : {
        __start_cdi_drivers = .;
        *(cdi_drivers)
    }
    __stop_cdi_drivers = .;*/
    .rodata ALIGN(4096) : {
        *(.rodata)
    }
    .bss ALIGN(4096) : {
        *(.bss)
    }
	kernel_end = .;
}

