# Simple binary search implementation


# change this values only if you want to increase the length of your input array.
.data
buffer: .space 40 	# list capacity = 10
buffer_size: .word 10	

str1: .asciiz    "Enter the items of your list in increasing order (10 item): \n"
str2: .asciiz	 "Enter the item to be searched (via binary search): "
str3: .asciiz	 "Item is found. Index is: "
str4: .asciiz	 "Item is not found. \n"

.text
j main

search_item:	# $a0 = l_index, $a1 = r_index, $a3 item, return $v0=index if found.
	la $t0, buffer
	lw $t1, buffer_size
	
search_loop:	sub $t3, $a0, $a1
		bgtz $t3 exit_loop
		sub $t4, $a1, $a0	# $t4 = r - l
		div $t4, $t4, 2		# $t4 = (r-l)/2
		add $t4, $t4, $a0	# $t4 = mid index
		mul $t6, $t4, 4
		add $t5, $t6, $t0 	# $t5 = &arr[mid]
		lw $t5, ($t5) 		# $t5 = arr[mid]
		sub $t5, $t5, $a3
		beqz $t5, exit_search
		bltz $t5, ignore_left
		bgtz $t5, ignore_right
		j search_loop

exit_loop:
		addi $v0, $zero, -1		# not found
		la $a0, str4
		addi $v0, $0, 4
		syscall
		move $v0, $zero			# v0 == -1
		jr $ra
		 
	
ignore_left: 
	addi $a0, $t4, 1
	j search_loop
	
ignore_right:
	addi $a1, $t4, -1
	j search_loop 

exit_search:	# item is found.
	move $t0, $a0
	la $a0, str3
	addi $v0, $0, 4
	syscall
	move $a0, $t4
	li $v0, 1
 	syscall
 	move $v0, $t4
 	jr $ra

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
	


main:
# get the integer list.
la $a0, str1
addi $v0, $0, 4
syscall
jal read_list

# get the item to be searched
la $a0, str2
addi $v0, $0, 4
syscall
li $v0, 5
syscall


# search the item
move $a0, $zero
lw $a1, buffer_size
addi $a1, $a1, -1
move $a3, $v0 	# $a3 = item
jal search_item
li $v0, 10
syscall


		
