# Palindrome find implementation in mips.

# hardcoded strings.
.data
str:  .asciiz "abbccbba"
str1: .asciiz    ": Palindrome"
str2: .asciiz    ": Not Palindrome"
str_list: .asciiz "another0anything0are0at0bad0badly0because0beginner0beside0best0better0big0bird0black0blue0book0both0brown0but0carefully0cat0certainly0chance0clean0common0complete0correct0could0cow0custom0dangerous0definitely0did0different0difficult0dirty0disturb0done0door0electricity0embarrassed0empty0enough0every0everything0example0excellent0excuse0extremely0fact0fat0flag0flat0for0found0from0funny0future0garbage0goat0good0grass0green0happy0has0heavy0her0herself0high0himself0his0idea0if0important0impossible0impressive0joke0king0lamp0inch0is0it0joke0king0lamp0Anna0Civic0Kayak0Level0Madam0Mom0Noon0Racecar0Radar0Redder0Refer0Repaper0Rotator0Rotor0Sagas0"
str3: .asciiz ". "
str4: .asciiz "\nDo you want to continue (y/n)?\n"
str5: .asciiz "\nPlease enter the last word:"
str6: .asciiz "\nBad input!"
str7: .asciiz "\nGoodbye!"
input_str: .space 128

.text
j main

# to_lowercase function for case insensitivity

to_lowercase:	# given the char, returns uppercase of it
	blt $a0, 'a', case
	move $v0, $a0
	jr $ra
case:
	addi $v0, $a0, 32
	jr $ra	


strlen:				# strlen, $a0= str.
	move $t0, $a0   	# str.
	li $t1, 0		# len = 0.
	li $t3, '0'
loop_strlen:
	lb $t2, ($t0)		# str[i].
	sub $t2, $t2, $t3
	beqz $t2, exit_strlen
	addi $t0, $t0, 1
	addi $t1, $t1, 1
	j loop_strlen
exit_strlen:
	move $v0, $t1 		# $v0 = len.
	jr $ra

is_palindrome:
	addi $sp, $sp, -4				# allocate stack frame
	sw $ra, ($sp)					# save ret address
	move $s0, $a0		# $s0 = str. 
	li $s1, 0		# i
	jal strlen
	move $s2, $v0		# $s3 = strlen(str). j
	add $s5, $s0, $s2
	addi $s2, $s2, -1 	# $s3 = strlen(str) -1
	li   $s6, 0
	sb   $s6, ($s5)
	addi $v0, $0, 4		# print the word.
    	syscall
loop_palindrome:
	sub $s4, $s2, $s1	# while (i > j)
	blez $s4, palindrome	
	add $t0, $s0, $s1	# &str[i]
	addi $s1, $s1, 1	# i++.
	add $t1, $s0, $s2	# &str[j]
	addi $s2, $s2, -1	# j--.
	lb $t0, ($t0)		# str[i]
	move $a0, $t0
	jal to_lowercase
	move $t0, $v0
	lb $t1, ($t1)		# str[j]
	move $a0, $t1
	jal to_lowercase
	move $t1, $v0
	sub $t2, $t0, $t1	# str[i] ==? str[j]
	bnez $t2, not_palindrome
	j loop_palindrome

palindrome:
	la $a0, str1
    	addi $v0, $0, 4
    	syscall
    	j exit_palindrome
	
not_palindrome:
	la $a0, str2
    	addi $v0, $0, 4
    	syscall
   
exit_palindrome:
	lw, $ra, ($sp)				# load ret address
	addi $sp, $sp, 4			# Free stack frame
	jr $ra

main:
	la $t6, str_list
	li $t7, 100
	li $t8, 101
main_loop:
	beqz $t7, main_exit
	li $a0, '\n'
	li $v0, 11    # print_character
	syscall
	sub $a0, $t8, $t7
	li $v0, 1
	syscall
	la $a0, str3
    	addi $v0, $0, 4
    	syscall
    	move $a0, $t6
	jal strlen
	move $t9, $v0  
	move $a0, $t6
	jal is_palindrome
	add $t6, $t6, $t9
	addi $t6, $t6, 1
	addi $t7, $t7, -1
	j main_loop
	
main_exit:
	la $a0, str4
    	addi $v0, $0, 4
    	syscall
    	addi $v0, $0, 12 # read y/n.
    	syscall
    	beq $v0, 'y', yes
    	beq $v0, 'n', exit
    	la $a0, str6
    	addi $v0, $0, 4
    	syscall
    	j exit
yes:	
	la $a0, str5
    addi $v0, $0, 4
    syscall 
	la $a0, input_str
	li $a1, 128
	addi $v0, $0, 8
	syscall ## flush the stdout.
	syscall 
	li $a0, 101
	li $v0, 1
	syscall
	la $a0, str3
    addi $v0, $0, 4
    syscall
        la $a0, input_str
loop_zero:
        lb $t0, ($a0)
        sub $t0, $t0, 10 
        beqz $t0, make_zero
        addi $a0, $a0, 1
        j loop_zero
make_zero:
    	li $t0, '0'
    	sb $t0, ($a0)
    	la $a0, input_str
    	jal is_palindrome
	    
exit:	la $a0, str7
    	addi $v0, $0, 4
    	syscall
	li $v0, 23
syscall


	


		
