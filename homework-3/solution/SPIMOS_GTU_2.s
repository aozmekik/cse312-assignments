# kernel #1.

.data
process_limit: .word  1024
process_table: .space 4096  # contains reference to processes resources.
process_queue: .space 4096  # queue for robin round scheduling. (circular queue)
program1:   .asciiz "asm-files/BinarySearch.asm"
program2:   .asciiz "asm-files/LinearSearch.asm"
program3:   .asciiz "asm-files/Collatz.asm"
program4:   .asciiz "asm-files/Palindrome.s"
msg4:       .asciiz "waiting for children...\n"

.text
main:

## inititalizing of the kernel routine.
lw      $s1, process_limit
li	    $s2, 0		    # $s2 = size.
li      $s3, 0          # $s3 = queue front.
addi    $s4, $s1, -1
li		$s5, 2 
  

la      $a0, process_table
la      $a1, process_queue        
jal		init_process_table

li		$v0, 18		    # init os and create init process.
syscall

## init process routine.
        li $t0, 5  # will be loaded 10 times.
        li $v0, 24
        syscall
        beqz $v0, p1 # $v0 == 0
        addi $v0, $v0, -1
        beqz $v0, p2 # $v0 == 1
        addi $v0, $v0, -1
        beqz $v0, p3 # v0 == 2
        addi $v0, $v0, -1   
        beqz $v0, p4 # $v0 == 3

p1:     addi $t0, $t0, -1
        li $v0, 19
        syscall
        bnez $v0, cont1 # parent continues to forking.
        la $a0, program1
        li $v0, 20      # fork and execve #1
        syscall

cont1:  beqz $t0, loop
        j p1    # continue.

p2:     addi $t0, $t0, -1
        li $v0, 19
        syscall
        bnez $v0, cont2 # parent continues to forking.
        la $a0, program2
        li $v0, 20      # fork and execve #1
        syscall

cont2:  beqz $t0, loop
        j p2    # continue.

p3:     addi $t0, $t0, -1
        li $v0, 19
        syscall
        bnez $v0, cont3 # parent continues to forking.
        la $a0, program3
        li $v0, 20      # fork and execve #1
        syscall

cont3:  beqz $t0, loop
        j p3    # continue.

p4:     addi $t0, $t0, -1
        li $v0, 19
        syscall
        bnez $v0, cont4 # parent continues to forking.
        la $a0, program4
        li $v0, 20      # fork and execve #1
        syscall

cont4:  beqz $t0, loop
        j p4    # continue.

    # wait for all children process here.
loop:   li $v0, 4       # syscall 4 (print_str)
        la $a0, msg4     # argument: string
        syscall
        li $a0, 0      # wait for any childprocess.
        li $v0, 21
        syscall
        beqz $v0, loop  # $v0 == 0, child is still running.
        bgtz $v0, loop  # $v0> 0 one child has terminated, continue waiting.

kernel_loop:
li		$v0, 10		    # exit.
syscall 

## interrupt handler and fork routines.


.globl fork
fork:
addi    $a0, $s2, 1       # $a0 = process index.
jal     enqueue
j       kernel_loop

.globl interrupt_handler   # $a1 = cont flag, $a0 = current process id.
interrupt_handler:
jal     is_empty
beqz    $v0, no_interrupt
beqz    $a1, not_cont
jal enqueue
not_cont:
jal     dequeue            # next process to execute from queue.
li      $a2, 1             # print.
move    $a1, $v0           # $a1 = next process id.
li      $v0, 22            # next process from queue to cpu.
syscall
no_interrupt:
move    $a1, $a0
li      $a2, 0             # no print.
li      $v0, 22
syscall
j kernel_loop


enqueue:                
addi	$t0, $s4, 1      
div     $t0, $s1        
mfhi    $s4             # back = (back + 1) % limit.
la      $t1, process_queue
sll 	$t2, $s4, 2			
add     $t1, $t1, $t2
sw      $a0, ($t1)        # process_table[back] = $a0.
addi    $s2, $s2, 1
jr     $ra

dequeue:               
la      $t0, process_queue
sll 	$t2, $s3, 2
add     $t0, $t2, $t0
lw      $v0, ($t0)        # $v0 = process_table[front].
li      $t3, 0xffff
sw      $t3, ($t0)
addi    $t1, $s3, 1
div     $t1, $s1
mfhi    $s3              # front = (front + 1) % limit.
addi    $s2, $s2, -1
jr     $ra

front:
la      $t0, process_table
sll 	$t2, $s3, 2
add     $t0, $t2, $t0
lw      $v0, ($t0)        # $v0 = process_table[front].
jr $ra

is_empty:
addi    $v0, $s2, 0
jr		$ra				

init_process_table:
la      $t0, process_table
la      $t6, process_queue
li      $t1, 0xffff
li      $t2, 0
loop_init_process_table:
sub     $t5, $t2, 4096
beqz    $t5, exit_init_process_table
add     $t3, $t0, $t2
add     $t7, $t6, $t2
sw      $t1, ($t3)      # process_table[i] = 0xffff 
sw      $t1, ($t7)
addi    $t2, $t2, 4
j		loop_init_process_table
exit_init_process_table:
jr $ra