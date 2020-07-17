# Simple linear search implementation


# change this values only if you want to increase the length of your input array.
.data
# buffer: .space 40 	# list capacity = 10
buffer: .word 12, 1230, 63, 4, 15, 72, 43, 1, 45, 645
msg: .asciiz "Buffer: [12, 1230, 63, 4, 15, 72, 43, 1, 45, 645]\n"
buffer_size: .word 10		

str1: .asciiz    "Enter the items of your list:"
str2: .asciiz	 "Enter the item to be searched upon the list:"
str3: .asciiz	 "Item is found. Index is:"
str4: .asciiz	 "Item is not found."

.text
j main

read_list:	# read the integers from user to buffer.
	la $t0, buffer
	lb $t1, buffer_size
		
read_loop:	li $v0, 5
		syscall		# read int
		sw $v0,($t0)
		addi $t0, $t0, 4
		addi $t1, $t1, -1
		beqz $t1, exit_read
		j read_loop

exit_read:	jr $ra	
	

find_item:	# $a0 is the item, return $v0 = 0 if found, 1 otherwise. $v1 is the index.
		la $t0, buffer
		li $v0, 1	# clear the $v0 as $v0 != 0
		lb $t1, buffer_size
		
find_loop:	lw $t2, ($t0)
		sub $t3, $t2, $a0
		beqz $t3, item_found	# item is found
		add $t1, $t1, -1
		beqz $t1, exit_find	# item not found
		addi $t0, $t0, 4
		j find_loop
		
item_found:	la $a0, str3
		addi $v0, $0, 4
		syscall
		lb $t4, buffer_size
		sub $t4, $t4, $t1	# get the index
		move $a0, $t4
 		li $v0, 1
 		syscall
 		move $v0, $zero
 		jr $ra
		
exit_find:	la $a0, str4
		addi $v0, $0, 4
		syscall
		jr $ra

main: j here
# get the integer list.
la $a0, str1
addi $v0, $0, 4
syscall
jal read_list

here: 
la $a0, msg
li $v0, 4
syscall


# get the item to be searched
la $a0, str2
addi $v0, $0, 4
syscall
li $v0, 5
syscall
move $a0, $v0	# $a0 is the item

# search the item 
jal find_item

li $v0, 21
syscall


		
