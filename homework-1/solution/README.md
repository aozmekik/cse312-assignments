### a brief explanation of the implementation

Besides the simple assembly files, the heavy load in the assignment was to write the `CreateProcess` system call that would be
required for the `Shell.asm` program. For this, I have added this system call to code `syscall.h` and `syscall.cpp` with
number `18`. Before proceeding with the implementation to handle the `CreateProcess` call, I have reviewed and tried some 
codes in other parts of the source code, especially by examining the file `spim.cpp` (since the it contains _spim simulator_'s
the main file). I moved the code from the source code to the `syscall.cpp` file, which will allow the assembly file to be 
loaded and then executed. As a result, the code became executable, but since we created a new process, the main 
problem was to ensure that the program that made the system call could continue from where it left off, by restoring the 
memory and registers. Being able to do this means doing context switch operation. 

I first took a backup of the global 
variables defined in `mem.h`, that is, all the elements of the context, to preserve the state of our context, that is, memory, 
and then recover after the process is over and continue back from where we left off. And after the process was completed, 
I successfully provided the context switch operation by recovering backup variables to present memory variables in order 
to change the context back and continue where we left off.

It can be examined that the desired output is provided on the following simple test. After running a simple program 
(_"Hello World"_ on the screen), a new shell is opened on the existing shell and the same process is repeated on the new 
shells that is opened. Then the shells are closed by giving the `exit` command four times in total to close the three shells,
 which is opened one by one, and the first shell. Then back to the spim console.

    (spim) read "Shell.asm"
    (spim) run
    
    ~$ test
    Hello World
    ~$ test
    Hello World
    ~$ Shell
    
    ~$ Shell
    
    ~$ Shell
    
    ~$ exit
    
    ~$ exit
    
    ~$ exit
    
    $ exit
    (spim) exit
