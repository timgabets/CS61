OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(multiboot_start)

SECTIONS
{
	. = 0x040000;

	.text : { *(.text .stub .text.* .gnu.linkonce.t.*) }
	PROVIDE(etext = .);	/* Define the 'etext' symbol to this value */

	.rodata : { *(.rodata .rodata.* .gnu.linkonce.r.*) }

	/* Adjust the address for the data segment to the next page */
	. = ALIGN(0x1000);

	/* The data segment */
	.data : { *(.data) }
	PROVIDE(edata = .);

	.bss : { *(.bss) }
	PROVIDE(end = .);

	/DISCARD/ : {
		*(.eh_frame .note.GNU-stack)
	}
}
