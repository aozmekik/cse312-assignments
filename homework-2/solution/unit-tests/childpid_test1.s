        .data
msg:   .asciiz "Hello World, from parent!\n"
msg2:  .asciiz "Hello World, from child!\n"
msg1:  .asciiz "Waiting for child...\n"
	.extern foobar 4

.text

main:   li $v0, 18      # fork
        syscall
        beqz $v0, child # if fork() == 0, process is child.
        j wait

child:  li $v0, 4       # print child.
        la $a0, msg2
        syscall
        li $v0, 21
        syscall

wait:   li $v0, 4       # print child.
        la $a0, msg1
        syscall
        li $v0, 19      # wait for any child.
        li $a0, 0
        syscall
        beqz $v0, wait  # return == 0, keep waiting.

parent: li $v0, 4       # print parent
        la $a0, msg
        syscall

exit:  li $v0, 10
    	syscall