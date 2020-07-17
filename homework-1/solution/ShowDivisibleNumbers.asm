# Given given 3 integers by user, your code will find and show
# the numbers between the given first and second integers that can be exactly divided by
# the third given number. For example: Given 10, 20, 3 Print 12,15, 18.

.data
buffer: .space 64 	# expected capacity of number of divisible numbers

str1: .asciiz    "Give 3 integers: "
str2: .asciiz	 "Divisible numbers in range: "


.text
j main

is_divisible:		# $v0 = 0 if $a0 % $a1 == 0.
		sub $a0, $a0, $a1	# $a0 <- $a0 - $a1
		blez $a0, end_iteration
		j is_divisible
end_iteration:  move $v0, $a0
		jr $ra

main:

# get the integer list.
la $a0, str1
addi $v0, $0, 4
syscall

# read int from user.
li $v0, 5
syscall
move $t0, $v0	# $to = first integer
li $v0, 5
syscall
move $t1, $v0	# $t1 = second integer
li $v0, 5
syscall
move $a1, $v0		# $a1 = third integer
move $t2, $zero		# t2 is buffer length
la $t5, buffer

# find the divisible numbers between $t0 and $t1
loop:		addi $t0, $t0, 1
		beq $t0, $t1, print
		move $a0, $t0
		jal is_divisible
		beqz $v0, div_true	# $a0 % $a1 == 0
		j loop
		
div_true:	sw $t0, ($t5)		# store the int
		addi $t5, $t5, 4	# incr index
		addi $t2, $t2, 1
		j loop
		


# print the found numbers		
print:		la $a0, str2
		addi $v0, $0, 4
		syscall
		la $t5, buffer		# restore index 
		li $v0, 1
		
print_loop:	beqz $t2, exit
 		lw $a0, ($t5)		# print int
 		li $v0, 1
 		syscall
 		li $a0, ','
		li $v0, 11    		# print char 
		syscall
 		addi $t5, $t5, 4
 		add $t2, $t2, -1
 		j print_loop

exit:	li $v0, 10
        syscall