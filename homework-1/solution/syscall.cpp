/* SPIM S20 MIPS simulator.
   Execute SPIM syscalls, both in simulator and bare mode.
   Execute MIPS syscalls in bare mode, when running on MIPS systems.
   Copyright (c) 1990-2010, James R. Larus.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

   Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   Neither the name of the James R. Larus nor the names of its contributors may be
   used to endorse or promote products derived from this software without specific
   prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#ifndef _WIN32
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>

#ifdef _WIN32
#include <io.h>
#endif

#include "spim.h"
#include "string-stream.h"
#include "inst.h"
#include "reg.h"
#include "mem.h"
#include "sym-tbl.h"
#include "syscall.h"


#ifdef _WIN32
/* Windows has an handler that is invoked when an invalid argument is passed to a system
   call. https://msdn.microsoft.com/en-us/library/a9yf33zb(v=vs.110).aspx

   All good, except that the handler tries to invoke Watson and then kill spim with an exception.

   Override the handler to just report an error.
*/

#include <stdio.h>
#include <stdlib.h>
#include <crtdbg.h>

void myInvalidParameterHandler(const wchar_t* expression,
   const wchar_t* function, 
   const wchar_t* file, 
   unsigned int line, 
   uintptr_t pReserved)
{
  if (function != NULL)
    {
      run_error ("Bad parameter to system call: %s\n", function);
    }
  else
    {
      run_error ("Bad parameter to system call\n");
    }
}

static _invalid_parameter_handler oldHandler;

void windowsParameterHandlingControl(int flag )
{
  static _invalid_parameter_handler oldHandler;
  static _invalid_parameter_handler newHandler = myInvalidParameterHandler;

  if (flag == 0)
    {
      oldHandler = _set_invalid_parameter_handler(newHandler);
      _CrtSetReportMode(_CRT_ASSERT, 0); // Disable the message box for assertions.
    }
  else
    {
      newHandler = _set_invalid_parameter_handler(oldHandler);
      _CrtSetReportMode(_CRT_ASSERT, 1);  // Enable the message box for assertions.
    }
}
#endif

#include "spim-utils.h"
#define BYTES_TO_INST(N) (((N) + BYTES_PER_WORD - 1) / BYTES_PER_WORD * sizeof(instruction*))


// copied this piece of code. (which is considered to be a utility code.)
// from "spim.cpp".
void run(){
    static mem_addr addr;
    bool continuable = false;

    addr = starting_address ();

    if (addr != 0)
    {
        char *undefs = undefined_symbol_string ();
        if (undefs != NULL)
        {
            write_output (message_out, "The following symbols are undefined:\n");
            write_output (message_out, undefs);
            write_output (message_out, "\n");
            free (undefs);
        }

        if (run_program (addr, DEFAULT_RUN_STEPS, false, false, &continuable))
            write_output (message_out, "Breakpoint encountered at 0x%08x\n", PC);
    }

}


void create_process(char* filename){
    initialize_world(exception_file_name, false);
    read_assembly_file(filename);
    run();
}



/* Decides which syscall to execute or simulate.  Returns zero upon
   exit syscall and non-zero to continue execution. */

int
do_syscall ()
{
#ifdef _WIN32
    windowsParameterHandlingControl(0);
#endif

  /* Syscalls for the source-language version of SPIM.  These are easier to
     use than the real syscall and are portable to non-MIPS operating
     systems. */

  switch (R[REG_V0])
    {
    case PRINT_INT_SYSCALL:
      write_output (console_out, "%d", R[REG_A0]);
      break;

    case PRINT_FLOAT_SYSCALL:
      {
	float val = FPR_S (REG_FA0);

	write_output (console_out, "%.8f", val);
	break;
      }

    case PRINT_DOUBLE_SYSCALL:
      write_output (console_out, "%.18g", FPR[REG_FA0 / 2]);
      break;

    case PRINT_STRING_SYSCALL:
      write_output (console_out, "%s", mem_reference (R[REG_A0]));
      break;

    case READ_INT_SYSCALL:
      {
	static char str [256];

	read_input (str, 256);
	R[REG_RES] = atol (str);
	break;
      }

    case READ_FLOAT_SYSCALL:
      {
	static char str [256];

	read_input (str, 256);
	FPR_S (REG_FRES) = (float) atof (str);
	break;
      }

    case READ_DOUBLE_SYSCALL:
      {
	static char str [256];

	read_input (str, 256);
	FPR [REG_FRES] = atof (str);
	break;
      }

    case READ_STRING_SYSCALL:
      {
	read_input ( (char *) mem_reference (R[REG_A0]), R[REG_A1]);
	data_modified = true;
	break;
      }

    case SBRK_SYSCALL:
      {
	mem_addr x = data_top;
	expand_data (R[REG_A0]);
	R[REG_RES] = x;
	data_modified = true;
	break;
      }

    case PRINT_CHARACTER_SYSCALL:
      write_output (console_out, "%c", R[REG_A0]);
      break;

    case READ_CHARACTER_SYSCALL:
      {
	static char str [2];

	read_input (str, 2);
	if (*str == '\0') *str = '\n';      /* makes xspim = spim */
	R[REG_RES] = (long) str[0];
	break;
      }

    case EXIT_SYSCALL:
      spim_return_value = 0;
      return (0);

    case EXIT2_SYSCALL:
      spim_return_value = R[REG_A0];	/* value passed to spim's exit() call */
      initialize_world(exception_file_name, false);
      return (0);

    case OPEN_SYSCALL:
      {
#ifdef _WIN32
        R[REG_RES] = _open((char*)mem_reference (R[REG_A0]), R[REG_A1], R[REG_A2]);
#else
	R[REG_RES] = open((char*)mem_reference (R[REG_A0]), R[REG_A1], R[REG_A2]);
#endif
	break;
      }

    case READ_SYSCALL:
      {
	/* Test if address is valid */
	(void)mem_reference (R[REG_A1] + R[REG_A2] - 1);
#ifdef _WIN32
	R[REG_RES] = _read(R[REG_A0], mem_reference (R[REG_A1]), R[REG_A2]);
#else
	R[REG_RES] = read(R[REG_A0], mem_reference (R[REG_A1]), R[REG_A2]);
#endif
	data_modified = true;
	break;
      }

    case WRITE_SYSCALL:
      {
	/* Test if address is valid */
	(void)mem_reference (R[REG_A1] + R[REG_A2] - 1);
#ifdef _WIN32
	R[REG_RES] = _write(R[REG_A0], mem_reference (R[REG_A1]), R[REG_A2]);
#else
	R[REG_RES] = write(R[REG_A0], mem_reference (R[REG_A1]), R[REG_A2]);
#endif
	break;
      }

    case CLOSE_SYSCALL:
      {
#ifdef _WIN32
	R[REG_RES] = _close(R[REG_A0]);
#else
	R[REG_RES] = close(R[REG_A0]);
#endif
	break;
      }

    case CREATE_PROCESS_SYSCALL: {
        char program_name[30];
        strcpy(program_name, (char *) mem_reference(R[REG_A0]));
        program_name[strlen(program_name)-1] = '.'; // delete the '\n' char.

        if (strcmp(program_name, "exit.") == 0) // exiting shell.
            R[REG_V0] = 0;
        else {
            strcat(program_name, "asm");
            if (access(program_name, F_OK) != -1) { // if file exist.

                /* before doing context switch: */
                /* save up the registers and memory before the context switch. */
                /* those variables exported from mem.h file." */
                int data_size = ROUND_UP(initial_data_size, BYTES_PER_WORD);
                int stack_size = ROUND_UP(initial_stack_size, BYTES_PER_WORD);
                int k_data_size = ROUND_UP(initial_k_data_size, BYTES_PER_WORD);

                reg_word ba_R[R_LENGTH];
                reg_word ba_HI, ba_LO;
                mem_addr ba_PC, ba_nPC;
                reg_word ba_CCR[4][32], ba_CPR[4][32];

                instruction **ba_text_seg;
                bool ba_text_modified;
                mem_addr ba_text_top;
                mem_word *ba_data_seg;
                bool ba_data_modified;
                short *ba_data_seg_h;
                BYTE_TYPE *ba_data_seg_b;
                mem_addr ba_data_top;
                mem_addr ba_gp_midpoint;
                mem_word *ba_stack_seg;
                short *ba_stack_seg_h;
                BYTE_TYPE *ba_stack_seg_b;
                mem_addr ba_stack_bot;
                instruction **ba_k_text_seg;
                mem_addr ba_k_text_top;
                mem_word *ba_k_data_seg;
                short *ba_k_data_seg_h;
                BYTE_TYPE *ba_k_data_seg_b;
                mem_addr ba_k_data_top;

                ba_text_seg = (instruction **) xmalloc (BYTES_TO_INST(initial_text_size));
                ba_data_seg = (mem_word *) xmalloc (data_size);
                ba_stack_seg = (mem_word *) xmalloc (stack_size);
                ba_k_text_seg = (instruction **) xmalloc (BYTES_TO_INST(initial_k_text_size));
                ba_k_data_seg = (mem_word*) xmalloc(k_data_size);
                memcpy(ba_R, R, R_LENGTH* sizeof(reg_word));
                ba_HI = HI;
                ba_LO = LO;
                ba_PC = PC;
                ba_nPC = nPC;
                memcpy(ba_CCR, CCR, 128 * sizeof(reg_word));
                memcpy(ba_CPR, CPR, 128 * sizeof(reg_word));
                memcpy(ba_text_seg, text_seg,  BYTES_TO_INST(initial_text_size));
                memcpy(ba_data_seg, data_seg, data_size);
                memcpy(ba_stack_seg, stack_seg, stack_size);
                memcpy(ba_k_text_seg, k_text_seg, BYTES_TO_INST(initial_k_text_size));
                memcpy(ba_k_data_seg, k_data_seg, k_data_size);
                ba_text_modified = text_modified;
                ba_text_top = text_top;
                ba_data_modified = data_modified;
                ba_data_seg_h = data_seg_h;
                ba_data_seg_b = data_seg_b;
                ba_data_top = data_top;
                ba_gp_midpoint = gp_midpoint;
                ba_stack_seg_h = stack_seg_h;
                ba_stack_seg_b = stack_seg_b;
                ba_stack_bot = stack_bot;
                ba_k_text_seg = k_text_seg;
                ba_k_text_top = k_text_top;
                ba_k_data_seg_h = data_seg_h;
                ba_k_data_seg_b = k_data_seg_b;
                ba_k_data_top = k_data_top;
                /* end of backing up the context. */


                /* clear the memory, do context switch. */
                text_seg = NULL;
                data_seg = NULL;
                stack_seg = NULL;
                k_text_seg = NULL;
                k_data_seg = NULL;

                create_process(program_name);

                /* context switch again, restore the old context. */
                memcpy(R, ba_R, R_LENGTH*sizeof(reg_word));
                HI = ba_HI;
                LO = ba_LO;
                PC = ba_PC;
                nPC = ba_nPC;
                memcpy(CCR, ba_CCR, 128 * sizeof(reg_word));
                memcpy(CPR, ba_CPR, 128 * sizeof(reg_word));
                memcpy(text_seg, ba_text_seg,  BYTES_TO_INST(initial_text_size));
                memcpy(data_seg, ba_data_seg, initial_data_size);
                memcpy(stack_seg, ba_stack_seg, initial_stack_size);
                memcpy(k_text_seg, ba_k_text_seg, BYTES_TO_INST(initial_k_text_size));
                memcpy(k_data_seg, ba_k_data_seg, k_data_size);
                text_modified = ba_text_modified;
                text_top = ba_text_top;
                data_modified = ba_data_modified;
                data_seg_h = ba_data_seg_h;
                data_seg_b = ba_data_seg_b;
                data_top = ba_data_top;
                gp_midpoint = ba_gp_midpoint;
                stack_seg_h = ba_stack_seg_h;
                stack_seg_b = ba_stack_seg_b;
                stack_bot = ba_stack_bot;
                k_text_seg = ba_k_text_seg;
                k_text_top = ba_k_text_top;
                k_data_seg_h = ba_k_data_seg_h;
                k_data_seg_b = ba_k_data_seg_b;
                k_data_top = ba_k_data_top;
                R[REG_V0] = 1;

            }
            else
                R[REG_V0] = -1;
        }
        break;
    }
    default:
      run_error ("Unknown system call: %d\n", R[REG_V0]);
      break;
    }

#ifdef _WIN32
    windowsParameterHandlingControl(1);
#endif
  return (1);
}


void
handle_exception ()
{
  if (!quiet && CP0_ExCode != ExcCode_Int)
    error ("Exception occurred at PC=0x%08x\n", CP0_EPC);

  exception_occurred = 0;
  PC = EXCEPTION_ADDR;

  switch (CP0_ExCode)
    {
    case ExcCode_Int:
      break;

    case ExcCode_AdEL:
      if (!quiet)
	error ("  Unaligned address in inst/data fetch: 0x%08x\n", CP0_BadVAddr);
      break;

    case ExcCode_AdES:
      if (!quiet)
	error ("  Unaligned address in store: 0x%08x\n", CP0_BadVAddr);
      break;

    case ExcCode_IBE:
      if (!quiet)
	error ("  Bad address in text read: 0x%08x\n", CP0_BadVAddr);
      break;

    case ExcCode_DBE:
      if (!quiet)
	error ("  Bad address in data/stack read: 0x%08x\n", CP0_BadVAddr);
      break;

    case ExcCode_Sys:
      if (!quiet)
	error ("  Error in syscall\n");
      break;

    case ExcCode_Bp:
      exception_occurred = 0;
      return;

    case ExcCode_RI:
      if (!quiet)
	error ("  Reserved instruction execution\n");
      break;

    case ExcCode_CpU:
      if (!quiet)
	error ("  Coprocessor unuable\n");
      break;

    case ExcCode_Ov:
      if (!quiet)
	error ("  Arithmetic overflow\n");
      break;

    case ExcCode_Tr:
      if (!quiet)
	error ("  Trap\n");
      break;

    case ExcCode_FPE:
      if (!quiet)
	error ("  Floating point\n");
      break;

    default:
      if (!quiet)
	error ("Unknown exception: %d\n", CP0_ExCode);
      break;
    }
}
