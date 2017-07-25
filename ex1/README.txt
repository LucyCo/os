Ex: 1

FILES:
osm.c -- implementation for the library functions which are designed to measure the time
         it takes to perform three operations, arithmetic, empty and trap. 
osm.h -- library that provides functions to measure the time it takes to perform three
         different operations.

REMARKS:

ANSWERS:
Answer to question 1:
Strace for WhatIDo:
The give program creates an allas copy of a given file, checks the file access mode,
allocated memory for the data access and then reallocates (both succeed). print the status
of the file, creates a new mapping, writes the given strings into the allocated memory,
closes the file, deletes the mapping and closes the process.

dup: Creates an allas copy of a given file. Receives an int of the file description
(register 2, in which the address for the file is found.) action succeeds.

fcntl: Gets the file access mode and the file status flags from the given fd. (register 3)

brk: changes the location of the program break (allocates memory for input). returns the address.

brk: receives an address for reallocating the ending point for the data access.

fstat: file status. returns information about the given file.

mmap: creates a new mapping in the virtual address space in the calling process. the
address is null-so the kernel chooses the address itself. the wanted space for allocation
is 4096 byte. the protection args for the memory is read and write for anonymous and
private processes. the file description is -1 and offset is 0 (as default to anonymous).
returns the address for the mapping.

lseek: attempts to reposition the file offset, though the offset value is 0, so the
returned value is -1 (illegal request).

write: write function writes to the given fd the array stream for 36 bytes and succeeds.
returns the amount of bytes written on.

write: the same as the previous command, only 10-byte long array, also succeeds. returns
the amount of bytes written on.

close: closes the file in fd 3.

munmap: deletes the mapping of the specified address range. (the args are the address
returned by mmap function and the length allocated-4096-byte).

exit_group: closes the process.


Answer to question 2:
1) Some side effects on the running process may be: While two programs attempt to write to
the same file. If one program is paused and the second program was meant to write after
the first program, causing the input data to be not relevant and maybe even cause errors
such as with mathematical values. Also if the second program was meant to read from a
given register that the first program was meant to write into but was paused, then that
result could be wrong. (While the binding is delayed from unification to interrupt
handling, other code can be executed in between that relies on variables being bound).
Also there could be a memory problem or access issues if one program overrides the other.
 
2) blocking all interrupts while a program is running would pause the entire system and
make it impossible to work with the computer. as one would have to wait until the program
ends running in order to do any task at all. 
