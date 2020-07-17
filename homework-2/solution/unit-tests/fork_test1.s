
.text

main:   li $v0, 18      # fork
        syscall

exit1:  li $v0, 10
    	syscall