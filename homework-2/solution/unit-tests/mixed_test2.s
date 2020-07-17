.data
program1:   .asciiz "tests/helloworld.s"
program2:   .asciiz "tests/helloworld.s"
program3:   .asciiz "tests/helloworld.s"
msg4:       .asciiz "waiting for childs..\n"

.text

main:   li $v0, 18
        syscall
        bnez $v0, cont1 # parent continues to forking.
        la $a0, program1
        li $v0, 20      # fork and execve #1
        syscall
        j exit

cont1:  li $v0, 18
        syscall
        bnez $v0, cont2 # parent continues to forking.
        la $a0, program2
        li $v0, 20     # fork and execve #2
        syscall

cont2:  li $v0, 18
        syscall
        bnez $v0, loop # parent exits
        la $a0, program3
        li $v0, 20      # fork and execve #3
        syscall
        j exit

    # wait for all child process here.
loop:   li $v0, 4       # syscall 4 (print_str)
        la $a0, msg4     # argument: string
        syscall
        li $a0, 0      # wait for any childprocess.
        li $v0, 19
        syscall
        beqz $v0, loop  # $v0 == 0, child is still running.
        bgtz $v0, loop  # $v0> 0 one child has terminated, continue waiting.

exit:	li $v0, 21
    	syscall