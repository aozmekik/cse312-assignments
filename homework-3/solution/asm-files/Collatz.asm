# Prints the collatz sequence of the given integer.<= 25
.data
msg: .asciiz "Give the number <= 25, for to found it's collatz: "

.text
j main

printInt:
move $t5, $a0	# save $a0
li $a0, ' '
li $v0, 11    # print_character
syscall
move $a0, $t5
li $v0, 1
syscall
jr $ra
 

# prints the collatz sequence of the input $a0.
printCollatz:	
loop:	addi $t0, $a0, -1   # t0 = n - 1
	beqz $t0, exit
	move $t4, $a0	
	jal printInt
	move $a0, $t4	    # restore $a0.
	andi $t1, $a0, 1    # if n is odd.
	beqz $t1, else
	mul $t2, $a0, 3     # t2 = 3*n
	addi $a0, $t2, 1    # n = 3*n + 1
	j loop
else:	sra $a0, $a0, 1	   # n = n / 2
	j loop
	
exit:	li $a0, 1
	jal printInt
	li $v0, 23
	syscall
	

main:
li $v0, 4       # print msg to user.
la $a0, msg
syscall

li $v0,5		# read integer from input.
syscall
move $a0,$v0
j printCollatz



		
