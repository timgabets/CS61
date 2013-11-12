WEENSYOS
========

Type `make run` to run WeensyOS using the QEMU emulator. We expect
this only to work on CS50 Appliance and other Linux hosts. If you have
problems, check out Troubleshooting below.

Running WeensyOS
----------------

WeensyOS offers several running modes.

*   `make run`

	Pops up a QEMU window running the OS. Press 'q' in the
	window to exit. Press 'a', 'f', or 'e' to soft-reboot the OS
	running a different initial process.

*   `make run-console`

	Runs QEMU in the current terminal window. Press 'q' in the
	terminal to exit.

*   `make run-gdb`

	Pops up a QEMU window running the OS, but the QEMU virtual
	machine immediately blocks for debugging. Then gdb is started
	in the terminal. You can set breakpoints on OS functions (for
	instance, try `b start` to break at the kernel's entry point)
	and then type `c` to run the virtual machine until a
	breakpoint hits. Use `si`, `x`, `info reg`, etc. to view
	machine state. We compile the kernel with debugging symbols;
	this means even commands like `next` will work, and you can
	see the source lines corresponding to individual instructions.
	Enter `q` in gdb to exit both gdb and the OS.

*   `make run-gdb-console`

	Same as `make run-gdb`, but runs QEMU in the current terminal
	window. You must run gdb yourself in a *different* terminal window.
	Run `gdb -x .gdbinit` from the problem set directory.

In all of these run modes, QEMU also creates a file named `log.txt`.
The code we hand out doesn't actually log anything yet, but you may
find it useful to add your own calls to `log_printf` from the kernel.

Finally, run `make clean` to clean up your directory.

WeensyOS Source
---------------

Real operating systems are big. We have tried to boil down the OS to a
minimum, comment it to help you, and separate x86 specifics from more
fundamental issues. Here is an overview of the code.

=== Important code ===

*   `kernel.c`: The kernel. Uses functions declared and described in
    `kernel.h` and `lib.h`.
*   `p-allocator.c`, `p-fork.c`, and `p-forkexit.c`: The applications.
    Uses functions declared and described in `process.h` and `lib.h`.

=== Support code ===

You may read these if you're interested but you should be able to do
the pset using only the code and descriptions in `kernel.c`, `lib.h`,
and `kernel.h`.

*   `lib.c`: Support code useful in both the kernel and applications.
*   `k-hardware.c`: Functions that set up x86 hardware state using
    programmed I/O and memory-mapped I/O instructions.
*   `k-interrupt.S`: Kernel assembly code for handling interrupts and
    exceptions.
*   `k-loader.c`: Kernel program loader, which loads processes from
    "image files" into memory.
*   `process.c`: Support code for applications.
*   `boot.c`, `bootstart.S`: The bootloader.
*   `x86.h`: x86 hardware definitions, including functions that
    correspond to important x86 instructions.
*   `elf.h`: ELF support information. (ELF is a format used for
    executables.)

WeensyOS Build Files
--------------------

The build process produces a disk image, `weensyos.img`. QEMU "boots"
off this disk image, which could also boot on real hardware. It also
produces other files that you can look at. These other files all go in
the `obj/` directory.

*   `obj/kernel.asm`

	This file is the output of `objdump -S` on the kernel. Use it to see
	the kernel's assembly code.

*   `obj/kernel.sym`

	This smaller file just lists all the kernel's symbols (i.e.,
	variable names).

*   `obj/p-allocator.asm`, `obj/p-allocator.sym`, ...

	Similar files are generated for process code.

Troubleshooting
---------------

WeensyOS runs using the QEMU full-system emulator. On CS50 Appliance,
`make run` will install QEMU for you. On your own Linux machine, you
will need to install QEMU yourself. On Ubuntu-based hosts, run `sudo
apt-get install qemu`. On Fedora-based hosts, run `sudo yum install
qemu-system-x86`.

If Control-C doesn't work on your QEMU, make sure you are using an
actual Control key. On some machines QEMU ignores key remappings (such
as swapping Control and Caps Lock).

If Control-C still doesn't work on your QEMU, forcibly close it by
running `make kill`.
