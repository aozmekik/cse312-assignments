# shell program. listens for a command, executes and
# then returns back to listening after execution.
.data
sgn: .asciiz "\n~$ "    # typical interpreter sign to indicate listening state.
err: .asciiz "program not found\n"
buffer: .space 30   # max length for the program name.

.text

main:	j loop

error:  la $a0, err
        li $v0, 4
        syscall

loop:   la $a0, sgn
        li $v0, 4
        syscall
        li $v0, 8
        la $a0, buffer
        li $a1, 30
        syscall
        li $v0, 18      # create process from that program.
        syscall
        beqz $v0, exit  # ret $v0 == 0, "exit" command has given, exiting.
        add $v0, $v0, 1
        beqz $v0, error # ret $v0 == -1, program does not found, listening.
        j loop

exit:	li $a0, 0
        li $v0, 17
    	syscall
