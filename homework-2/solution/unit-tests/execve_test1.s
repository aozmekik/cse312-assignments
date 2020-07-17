        .data
msg:   .asciiz "tests/helloworld.s"

.text

main:   li $v0, 20      # execve current process.
        la $a0, msg
        syscall


exit:  li $v0, 10
    	syscall