
Throughout the semester, we will try to write our operating system that runs on a MIPS CPU
that you are already familiar with from the CSE 331 course. The MIPS CPU will be simulated
by the open source package SPIM (http://spimsimulator.sourceforge.net/, full source code
https://sourceforge.net/p/spimsimulator/code/HEAD/tree/). From the SPIM documentation
_“Spim is a self_ - contained simulator that runs MIPS32 programs. It reads and executes
assembly language programs written for this processor. Spim also provides a simple
debugger and minimal set of operating system services. Spim does not execute binary
_(compiled) programs.”_
The operating system that comes with SPIM is very primitive. It only supports a few trivial
system calls, such as reading and printing, which are defined in /CPU/syscall.h.
The position of our SPIM OS is shown in the main software architecture diagram below,
which indicates that our SPIM OS will be implemented on both SPIM and LINUX. In other
words, some of our OS packages will be in MIPS assembly and some of them will be in C++.

```
Shell.asm Search.asm ... Sort.asm
```
SPIM OS (^)
SPIM
(MIPS simulator)
(^)
LINUX (VMware)
HARDWARE
Figure 1 : System architecture
For this homework, you will implement 3 regular MIPS assembly programs, which can run
with the simple SPIM OS without any modification. You will write another MIPS assembly
program that needs a new OS service, which will be implemented by you as part of this
homework.


First, download and study the SPIM package very carefully from the above link. Then, you
will write and test the following MIPS assembly files. Use the provided sample assembly files
to learn about how to use the MIPS simulator.

1. ShowDivisibleNumbers.asm : given 3 integers by user, your code will find and show
    the numbers between the given first and second integers that can be exactly divided by
    the third given number. For example: Given 10, 20, 3 Print 12,15, 18.
2. BinarySearch.asm : your code will find the target number by implementing the binary
    search algorithm for in given integer list. Note that the list should be given in
    increasing order for the binary search.
3. LinearSearch.asm : this will be very similar to the previous example. This time the list
    does not have to be sorted.
4. SelectionSort.asm : your code will sort the given integers in increasing order by
    implementing selection sort algorithm.
5. Shell.asm: You will write a shell program that will be very similar to the example
    given in the textbook in Fig 1-19. Note that, our current SPIM OS does not support
    fork, waitpid, and execve system calls, so you will have to implement some system
    services for this shell. Instead of implementing these three system services, you will
    implement the Windows system procedure call CreateProcess which loads an
    assembly file from the disk and executes it. After this call is processed, the OS returns
    to the caller. You will have to change the SPIM source code in two places:
    /CPU/syscall.h and /CPU/syscall.cpp. Do not modify any other SPIM code other than
    these files.

Below are the instructions for homework submission

1. Download and Install Vmware Player from official site.
2. Download and install our virtual machine from
    https://drive.google.com/open?id=1YppX3lNkyTsHV_lvA4w9TomNCUkpLeEg
3. Carefully examine SPIM simulator and MIPS instruction set.
    https://web.stanford.edu/class/cs143/materials/SPIM_Manual.pdf is a very good SPIM
    documentation.
4. Your submission includes only the files below
     ShowDivisibleNumbers.asm
     BinarySearch.asm
     LinearSearch.asm
     SelectionSort.asm
     Shell.asm
     syscall.cpp
     syscall.h


