        .data
msg:   .asciiz "tests/helloworld.s"
msg3:  .asciiz "Child has returned!\n"
msg2:  .asciiz "Waiting for child...\n"

.text

main:   li $v0, 18      # fork
        syscall
        beqz $v0, child
        j exit

child:
        li $v0, 20
        la $a0, msg     # execve. helloworld.s
        syscall


exit:   li $v0, 4       # syscall 4 (print_str)
        la $a0, msg2
        syscall

        li $v0, 19      # wait for any child.
        li $a0, 0
        syscall
        beqz $v0, exit  # return == 0, keep waiting.

        li $v0, 4       # syscall 4 (print_str)
        la $a0, msg3
        syscall

exit1:   li $v0, 10
    	syscall	