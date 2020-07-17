# Simple selection sort implementation


# change this values only if you want to increase the length of your input array.
.data
buffer: .space 40 	# list capacity = 10
buffer_size: .word 10	

str1: .asciiz    "Enter the items of your list: "
str2: .asciiz    "Sorted list (via selection sort): "

.text
j main


read_list:	# read the integers from user to buffer.
	la $t0, buffer
	lw $t1, buffer_size
		
read_loop:	li $v0, 5
		syscall		# read int
		sw $v0,($t0)
		addi $t0, $t0, 4
		addi $t1, $t1, -1
		beqz $t1, exit_read
		j read_loop

exit_read:	jr $ra


	# swap operation. swap buffer[$a0] and buffer + [$a1]
swap:	
	addi $sp, $sp, -16
	sw $s3, ($sp)
	sw $s4, 4($sp)
	sw $s5, 8($sp)
	sw $s6, 12($sp)
	la $t0, buffer
	lw $t1, buffer_size
	mul $a0, $a0, 4
	mul $a1, $a1, 4
	add $s3, $t0, $a0
	add $s4, $t0, $a1
	lw $s5, ($s3)
	lw $s6, ($s4)
	sw $s6, ($s3)
	sw $s5, ($s4)
	lw $s3, ($sp)
	lw $s4, 4($sp)
	lw $s5, 8($sp)
	lw $s6, 12($sp)
	addi $sp, $sp, 16	
	jr $ra
		
	
sort:		addi $sp, $sp, -4
		sw $ra, ($sp)
		la $t0, buffer
		lw $t1, buffer_size
		li $t2, -1		# i = 0
		 
sort_loop:	addi $t2, $t2, 1
		sub $t3, $t2, $t1
		addi $t3, $t3, 1	# i - n + 1 < 0; i<n-1
		bgez $t3, exit_sort 
		move $t4, $t2		# min idx = i
		add $t5, $t2, 0		#  j = i +1
inner_loop:	add $t5, $t5, 1
		sub $t6, $t5, $t1	
		beqz $t6, do_swap
		mul $a0, $t5, 4
		add $t7, $t0, $a0	# &buffer[j]
		lw $t7, ($t7)		# buffer[i]
		mul $a0, $t4, 4
		add $t8, $t0, $a0	# &buffer[min_idx]
		lw $t8, ($t8)		# buffer[min_idx]
		sub $t9, $t7, $t8	# buffer[j] - buffer[min_idx]
		bgez $t9, inner_loop
		move $t4, $t5
		j inner_loop
		
	
do_swap:	move $a0, $t4
		move $a1, $t2
		jal swap
		j sort_loop	

exit_sort:	lw $ra, ($sp)
		addi $sp, $sp, 4
		jr $ra

print_list:
		la $t0, buffer
		lw $t1, buffer_size
		addi $t2, $t1, -1
		
print_loop:	bltz $t2, exit_print
		lw $a0, ($t0)
		addi $v0, $0, 1
		syscall
		addi $t0, $t0, 4
		addi $t2, $t2, -1
		li $a0, ' '
		li $v0, 11    		# print char 
		syscall
		j print_loop
		
exit_print:	jr $ra

main:
# get the integer list.
la $a0, str1
addi $v0, $0, 4
syscall
jal read_list
jal sort
la $a0, str2
addi $v0, $0, 4
syscall
jal print_list
li $v0, 10
syscall


		
