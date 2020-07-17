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

#include <iostream>
#include <cassert>
#include "spim-utils.h"
using namespace std;

#define DEAD 0xffff

#ifdef _WIN32
/* Windows has an handler that is invoked when an invalid argument is passed to a system
   call. https://msdn.microsoft.com/en-us/library/a9yf33zb(v=vs.110).aspx

   All good, except that the handler tries to invoke Watson and then kill spim with an exception.

   Override the handler to just report an error.
*/

#include <stdio.h>
#include <stdlib.h>
#include <crtdbg.h>

void myInvalidParameterHandler(const wchar_t *expression,
                               const wchar_t *function,
                               const wchar_t *file,
                               unsigned int line,
                               uintptr_t pReserved)
{
  if (function != NULL)
  {
    run_error("Bad parameter to system call: %s\n", function);
  }
  else
  {
    run_error("Bad parameter to system call\n");
  }
}

static _invalid_parameter_handler oldHandler;

void windowsParameterHandlingControl(int flag)
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
    _CrtSetReportMode(_CRT_ASSERT, 1); // Enable the message box for assertions.
  }
}
#endif

enum State
{
  READY,
  RUNNING,
  BLOCKED
};

// copied this piece of code. (which is considered to be a utility code.)
// from "spim.cpp".
void run(mem_addr addr)
{
  bool continuable;
  if (run_program(addr, DEFAULT_RUN_STEPS, false, false, &continuable))
    write_output(message_out, "Breakpoint encountered at 0x%08x\n", PC);
}

/**
 * Process kept in the assembly data structure. 
 * 
 ***/
class Process
{
public:
  Process(char *_name, int _id)
  {
    p_data_size = ROUND_UP(initial_data_size, BYTES_PER_WORD);
    p_stack_size = ROUND_UP(initial_stack_size, BYTES_PER_WORD);
    p_k_data_size = ROUND_UP(initial_k_data_size, BYTES_PER_WORD);

    // if process is the kernel, allocations has been already made for it.
    if (_id != -1)
    {
      p_data_seg = (mem_word *)xmalloc(p_data_size);
      p_stack_seg = (mem_word *)xmalloc(p_stack_size);
      p_k_data_seg = (mem_word *)xmalloc(p_k_data_size);
    }

    id = _id;
    parent_id = CCR[1][0];
    memcpy(name, _name, strlen(_name));
    state = READY;
    fromCPU(true); // copy the img from current context.
    p_CCR[1][0] = id;
  }

  Process &operator=(const Process &other)
  {
    memcpy(p_R, other.p_R, R_LENGTH * sizeof(reg_word));
    p_HI = other.p_HI;
    p_LO = other.p_LO;
    p_PC = other.p_PC;
    p_nPC = other.p_nPC;
    memcpy(p_CCR, other.p_CCR, 128 * sizeof(reg_word));
    memcpy(p_CPR, other.p_CPR, 128 * sizeof(reg_word));
    p_text_modified = other.p_text_modified;
    p_text_top = other.p_text_top;
    p_data_modified = other.p_data_modified;
    p_data_seg_h = other.p_data_seg_h;
    p_data_seg_b = other.p_data_seg_b;
    p_data_top = other.p_data_top;
    p_gp_midpoint = other.p_gp_midpoint;
    p_stack_seg_h = other.p_stack_seg_h;
    p_stack_seg_b = other.p_stack_seg_b;
    p_stack_bot = other.p_stack_bot;
    p_k_text_seg = other.p_k_text_seg;
    p_k_text_top = other.p_k_text_top;
    p_k_data_seg_h = other.p_k_data_seg_h;
    p_k_data_seg_b = other.p_k_data_seg_b;
    p_k_data_top = other.p_k_data_top;
    p_k_data_seg_b = (BYTE_TYPE *)other.p_k_data_seg;
    p_k_data_seg_h = (short *)other.p_k_data_seg;
    p_data_seg = other.p_data_seg;
    p_stack_seg = other.p_stack_seg;
    p_k_data_seg = other.p_k_data_seg;
    p_text_seg = other.p_text_seg;
    p_k_text_seg = other.p_k_text_seg;
    return *this;
  }

  void
  fromCPU(bool first = false)
  {
    memcpy(p_R, R, R_LENGTH * sizeof(reg_word));
    p_HI = HI;
    p_LO = LO;
    p_PC = PC;
    p_nPC = nPC;
    memcpy(p_CCR, CCR, 128 * sizeof(reg_word));
    memcpy(p_CPR, CPR, 128 * sizeof(reg_word));
    p_text_modified = text_modified;
    p_text_top = text_top;
    p_data_modified = data_modified;
    p_data_seg_h = data_seg_h;
    p_data_seg_b = data_seg_b;
    p_data_top = data_top;
    p_gp_midpoint = gp_midpoint;
    p_stack_seg_h = stack_seg_h;
    p_stack_seg_b = stack_seg_b;
    p_stack_bot = stack_bot;
    p_k_text_seg = k_text_seg;
    p_k_text_top = k_text_top;
    p_k_data_seg_h = k_data_seg_h;
    p_k_data_seg_b = k_data_seg_b;
    p_k_data_top = k_data_top;
    p_k_data_seg_b = (BYTE_TYPE *)k_data_seg;
    p_k_data_seg_h = (short *)k_data_seg;

    if (first && id != -1)
    {
      memcpy(p_data_seg, data_seg, p_data_size);
      memcpy(p_stack_seg, stack_seg, p_stack_size);
      memcpy(p_k_data_seg, k_data_seg, p_k_data_size);
    }
    else
    {
      p_data_seg = data_seg;
      p_stack_seg = stack_seg;
      p_k_data_seg = k_data_seg;
    }
    p_text_seg = text_seg;
    p_k_text_seg = k_text_seg;
  }
  void toCPU()
  {
    text_seg = p_text_seg;
    k_text_seg = p_k_text_seg;
    data_seg = p_data_seg;
    stack_seg = p_stack_seg;
    k_data_seg = p_k_data_seg;

    memcpy(R, p_R, R_LENGTH * sizeof(reg_word));
    HI = p_HI;
    LO = p_LO;
    PC = p_PC;
    nPC = p_nPC;
    memcpy(CCR, p_CCR, 128 * sizeof(reg_word));
    memcpy(CPR, p_CPR, 128 * sizeof(reg_word));
    text_modified = p_text_modified;
    text_top = p_text_top;
    data_modified = p_data_modified;
    data_seg_h = p_data_seg_h;
    data_seg_b = p_data_seg_b;
    data_top = p_data_top;
    gp_midpoint = p_gp_midpoint;
    stack_seg_h = p_stack_seg_h;
    stack_seg_b = p_stack_seg_b;
    stack_bot = p_stack_bot;
    k_text_seg = p_k_text_seg;
    k_text_top = p_k_text_top;
    k_data_seg_h = p_k_data_seg_h;
    k_data_seg_b = p_k_data_seg_b;
    k_data_top = p_k_data_top;
    k_data_seg_b = (BYTE_TYPE *)p_k_data_seg;
    k_data_seg_h = (short *)p_k_data_seg;
  }

  // TODO. carefull with kernel process.
  void clear()
  {
    free(p_data_seg);
    free(p_stack_seg);
    free(p_k_data_seg);
    free(p_text_seg);
    free(p_k_text_seg);
  }

  int getID()
  {
    return id;
  }

  char *getName()
  {
    return (char *)name;
  }

  void setName(char *_name)
  {
    memcpy(name, _name, 32);
  }

  void setParent(int _id)
  {
    parent_id = _id;
  }

  void setPC(mem_addr pc)
  {
    p_PC = pc;
  }

  mem_addr getPC()
  {
    return p_PC;
  }

  void incrementPC()
  {
    p_PC += BYTES_PER_WORD;
  }

  void setReturn(int val)
  {
    p_R[REG_V0] = val;
  }

  int getState()
  {
    return state;
  }

  int getParent()
  {
    return parent_id;
  }

  void setState(int _state)
  {
    state = _state;
  }

  void print() const
  {
    write_output(console_out, "ID\t\t\t:\t%d\nParentID\t\t:\t%d\nProcessName\t\t:\t%s\n"
                              "PragramCounter\t\t:\t0x%x\nR[V0]\t\t\t:\t%d\n",
                 id, parent_id, (char *)name, p_PC, p_R[REG_V0]);

    switch (state)
    {
    case BLOCKED:
      write_output(console_out, "State\t\t\t:\tBlocked\n");
      break;
    case RUNNING:
      write_output(console_out, "State\t\t\t:\tRunning\n");
      break;
    case READY:
      write_output(console_out, "State\t\t\t:\tReady\n");
      break;
    default:
      exit(EXIT_FAILURE);
    }
  }

private:
  /* attributes */
  mem_word id;
  mem_word parent_id;
  mem_word name[8]; // 8*4   = 32 byte name length limit.
  mem_word state;   // states: 0, 1, 2.

  /* context */
  reg_word p_HI, p_LO;
  reg_word p_R[R_LENGTH];
  reg_word p_CCR[4][32], p_CPR[4][32];
  mem_addr p_PC, p_nPC;
  instruction **p_text_seg;
  bool p_text_modified;
  mem_addr p_text_top;
  mem_word *p_data_seg;
  bool p_data_modified;
  short *p_data_seg_h;
  BYTE_TYPE *p_data_seg_b;
  mem_addr p_data_top;
  mem_addr p_gp_midpoint;
  mem_word *p_stack_seg;
  short *p_stack_seg_h;
  BYTE_TYPE *p_stack_seg_b;
  mem_addr p_stack_bot;
  instruction **p_k_text_seg;
  mem_addr p_k_text_top;
  mem_word *p_k_data_seg;
  short *p_k_data_seg_h;
  BYTE_TYPE *p_k_data_seg_b;
  mem_addr p_k_data_top;
  int p_data_size;
  int p_stack_size;
  int p_k_data_size;
};

/**
 * .cpp codes to reach the data structures and procedures in assembly code.
 * singleton object should only be reached when the kernel is on the cpu.
 ***/
class OS
{
public:
  OS(int _limit, mem_word _table_addr, mem_word _queue_addr)
      : index(0),
        limit(_limit),
        block_interrupt(true),
        prev_id(0),
        table_addr(_table_addr),
        queue_addr(_queue_addr)
  {
    readTable();
    kernel = new Process("kernel", -1);
    temp = new Process("temp", -2);
    fork_routine = find_symbol_address("fork");
    interrupt_handler_routine = find_symbol_address("interrupt_handler");
    srand(time(NULL));
  }

  void readTable()
  {
    process_table = (mem_word *)mem_reference(table_addr);
    process_queue = (mem_word *)mem_reference(queue_addr);
  }

  Process *get(int idx) const
  {
    assert(idx <= index);
    return (Process *)mem_reference(process_table[idx]);
  }

  void set(Process *process) const
  {
    assert(process->getID() <= index);
    char *data = (char *)process;

    mem_addr addr = process_table[process->getID()];
    for (size_t i = 0; i < sizeof(Process); i++)
      set_mem_byte(addr + i, *(data + i));
  }

  // caller is not necessarily kernel.
  Process *create(char *name, bool init = false)
  {
    assert(index != limit);
    // fork the current process.
    Process *new_process = new Process(name, index);

    if (!init)
      toKernel();
    process_table[index] = data_top;
    expand_data(sizeof(Process));
    readTable();
    data_modified = true;
    set(new_process); // save the process in the kernel's heap.
    kernel->fromCPU();
    index++;
    if (!init)
      fromKernel();

    return new_process;
  }

  int currentID()
  {
    return CCR[1][0];
  }
  int toKernel()
  {
    prev_id = currentID();
    temp->fromCPU();
    kernel->toCPU();
    Process *prev_process = get(prev_id);
    *prev_process = *temp;
    set(prev_process);
    return prev_id;
  }

  void fromKernel()
  {
    kernel->fromCPU();
    get(prev_id)->toCPU();
  }

  int contextSwitch(int to)
  {
    assert(to <= index);

    // save current process.
    Process *current_process = get(prev_id);
    current_process->setState(BLOCKED);
    set(current_process);

    // restore next process.
    Process *next_process = get(to);
    next_process->setState(RUNNING);
    next_process->toCPU(); // leaving kernel by switching context to <to>.
    return prev_id;
  }

  void fork()
  {
    int parent_id = toKernel();
    Process *parent = get(parent_id);
    fromKernel();

    Process *child = create(parent->getName());
    child->incrementPC();
    child->setReturn(0);
    // child->setParent(parent_id);

    toKernel();
    set(child);
    parent = get(parent_id);
    parent->setReturn(child->getID());
    set(parent);
    block_interrupt = true;
    run(fork_routine); // jump the fork routine in the kernel assembly.
    block_interrupt = false;
    fromKernel();
    // delete child; // FIXME.
  }

  void execve(char *name)
  {
    if (access(name, F_OK) != -1) // if file exist.
    {
      int id = toKernel();
      Process *current = get(id);
      current->setName(name);
      fromKernel();
      text_seg = NULL;
      data_seg = NULL;
      stack_seg = NULL;
      k_text_seg = NULL;
      k_data_seg = NULL;
      initialize_world(exception_file_name, false);
      read_assembly_file(name);
      PC = starting_address();
      CCR[1][0] = id;
      current->fromCPU();

      // save the changed process in the assembly data structure.
      toKernel();
      set(current);
      fromKernel();
    }
    else
    {
      write_output(console_out, "No such file: %s", name);
      exit(EXIT_FAILURE);
    }
  }

  void interruptHandler(int cont)
  {
    if (block_interrupt)
      return;
    int current_id = toKernel();
    block_interrupt = true;
    R[REG_A0] = current_id;
    R[REG_A1] = cont;
    run(interrupt_handler_routine);
    block_interrupt = false;
  }

  bool exist(int id)
  {
    return process_table[id] != DEAD;
  }

  bool isChild(int parent, int child)
  {
    return get(child)->getParent() == parent;
  }

  void printChildren(int id)
  {
    bool found = false;
    write_output(console_out, "ChildList\t\t:\t[ ");
    for (int i = 0; i < index; i++)
    {
      if (exist(i) && isChild(id, i))
      {
        found = true;
        write_output(console_out, "%d ", i);
      }
    }
    if (found)
      write_output(console_out, "]\n\n");
    else
      write_output(console_out, "Empty ]\n\n");
  }

  int getChild(int pid)
  {
    for (int i = 0; i < index; i++)
      if (exist(i) && isChild(pid, i))
        return i;
    return -1;
  }

  bool isTerminated(int pid, int cid)
  {
    return !exist(cid) && isChild(pid, cid);
  }

  void removeProcess(int cid)
  {
    toKernel(); // FIXME. check here.
    Process *p = get(cid);
    p->clear();
    fromKernel();
    interruptHandler(false); // go to assembly interrupt handler.
    process_table[cid] = DEAD;
  }

  int queue_front()
  {
    return process_queue[R[19]];
  }

  void print_queue()
  {
    write_output(console_out, "\nqueue:[");
    for (int i = 0; i < limit; i++)
    {
      if (process_queue[i] != DEAD)
        write_output(console_out, "%d--", process_queue[i]);
    }
    write_output(console_out, "]\n");
  }

  void print(int cid, int nid)
  {
    write_output(console_out, "\n\n____________________________________________________________________\n\n");
    write_output(console_out, "[***\t\tContext Scheduling Has Happened!\t\t***]\n");
    write_output(console_out, "\t\t{\tProcess Table:\t\t}\n");

    for (int i = 0; i < index; i++)
    {
      if (exist(i))
      {
        Process *p = get(i);
        p->print();
        printChildren(i);
      }
    }

    write_output(console_out, ">Blocked:\t\tID %d\n", cid);
    write_output(console_out, ">Changing State:\tID %d\n", nid);
    write_output(console_out, "\n[***\t\t{\t\tEND\t\t}\t\t***]\n");
    write_output(console_out, "_____________________________________________________________________\n\n");
  }

  // TODO. test this.
  void waitpid(int pid, int cid)
  {
    toKernel();
    int status = 0;
    if (cid == 0)
      cid = getChild(pid);
    if (cid > 0)
    {
      if (isTerminated(pid, cid))
      {
        status = cid;
        removeProcess(cid);
      }
      else
        status = 0;
    }
    else
      status = -1;
    fromKernel();
    R[REG_V0] = status;
  }

  void blockInterrupt()
  {
    block_interrupt = true;
  }

  void unblockInterrupt()
  {
    block_interrupt = false;
  }

  Process *kernel, *temp;

private:
  mem_word *process_table; // reference to process table kept in assembly data structure.
  mem_word *process_queue; // reference to process queue kept in assembly data structure.
  int index, limit;
  bool block_interrupt;
  int prev_id;
  mem_addr fork_routine, interrupt_handler_routine;
  mem_addr table_addr, queue_addr;
};

OS *os = NULL;

/*You implement your handler here*/
void SPIM_timerHandler()
{
  if (os != NULL)
    os->interruptHandler(true);
}

/* Decides which syscall to execute or simulate.  Returns zero upon
   exit syscall and non-zero to continue execution. */
int do_syscall()
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
    write_output(console_out, "%d", R[REG_A0]);
    break;

  case PRINT_FLOAT_SYSCALL:
  {
    float val = FPR_S(REG_FA0);

    write_output(console_out, "%.8f", val);
    break;
  }

  case PRINT_DOUBLE_SYSCALL:
    write_output(console_out, "%.18g", FPR[REG_FA0 / 2]);
    break;

  case PRINT_STRING_SYSCALL:
    write_output(console_out, "%s", mem_reference(R[REG_A0]));
    break;

  case READ_INT_SYSCALL:
  {
    static char str[256];

    read_input(str, 256);
    R[REG_RES] = atol(str);
    break;
  }

  case READ_FLOAT_SYSCALL:
  {
    static char str[256];

    read_input(str, 256);
    FPR_S(REG_FRES) = (float)atof(str);
    break;
  }

  case READ_DOUBLE_SYSCALL:
  {
    static char str[256];

    read_input(str, 256);
    FPR[REG_FRES] = atof(str);
    break;
  }

  case READ_STRING_SYSCALL:
  {
    read_input((char *)mem_reference(R[REG_A0]), R[REG_A1]);
    data_modified = true;
    break;
  }

  case SBRK_SYSCALL:
  {
    mem_addr x = data_top;
    expand_data(R[REG_A0]);
    R[REG_RES] = x;
    data_modified = true;
    break;
  }

  case PRINT_CHARACTER_SYSCALL:
    write_output(console_out, "%c", R[REG_A0]);
    break;

  case READ_CHARACTER_SYSCALL:
  {
    static char str[2];

    read_input(str, 2);
    if (*str == '\0')
      *str = '\n'; /* makes xspim = spim */
    R[REG_RES] = (long)str[0];
    break;
  }

  case EXIT_SYSCALL:
    spim_return_value = 0;
    return (0);

  case EXIT2_SYSCALL:
    spim_return_value = R[REG_A0]; /* value passed to spim's exit() call */
    return (0);

  case OPEN_SYSCALL:
  {
#ifdef _WIN32
    R[REG_RES] = _open((char *)mem_reference(R[REG_A0]), R[REG_A1], R[REG_A2]);
#else
    R[REG_RES] = open((char *)mem_reference(R[REG_A0]), R[REG_A1], R[REG_A2]);
#endif
    break;
  }

  case READ_SYSCALL:
  {
    /* Test if address is valid */
    (void)mem_reference(R[REG_A1] + R[REG_A2] - 1);
#ifdef _WIN32
    R[REG_RES] = _read(R[REG_A0], mem_reference(R[REG_A1]), R[REG_A2]);
#else
    R[REG_RES] = read(R[REG_A0], mem_reference(R[REG_A1]), R[REG_A2]);
#endif
    data_modified = true;
    break;
  }

  case WRITE_SYSCALL:
  {
    /* Test if address is valid */
    (void)mem_reference(R[REG_A1] + R[REG_A2] - 1);
#ifdef _WIN32
    R[REG_RES] = _write(R[REG_A0], mem_reference(R[REG_A1]), R[REG_A2]);
#else
    R[REG_RES] = write(R[REG_A0], mem_reference(R[REG_A1]), R[REG_A2]);
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

  case INIT_SYSCALL:
  {
    os = new OS(1024, R[REG_A0], R[REG_A1]);
    Process *init = os->create("init", true);
    init->setParent(-1);
    os->set(init);
    os->unblockInterrupt();
    init->toCPU();
    // delete init; // FIXME.
    break;
  }

  case FORK_SYSCALL:
  {
    os->fork();
    break;
  }

  case EXECVE_SYSCALL:
  {
    char name[32];
    strcpy(name, (char *)mem_reference(R[REG_A0]));
    os->execve(name);
    PC -= BYTES_PER_WORD;
    break;
  }

  case WAITPID_SYSCALL:
  {
    os->waitpid(os->currentID(), 0);
    break;
  }

  case CONTEXT_SWITCH_SYSCALL:
  {
    os->kernel->fromCPU();
    if (R[REG_A2])
    {
      os->print(R[REG_A0], R[REG_A1]);
      os->contextSwitch(R[REG_A1]);
    }
    else
      os->fromKernel();
    return 0;
  }

  case CUSTOM_EXIT_SYSCALL:
  {
    os->removeProcess(os->currentID());
    PC -= BYTES_PER_WORD;
    break;
  }

  case RANDOM_SYSCALL:
  {
    R[REG_V0] = rand() % 4;
    break;
  }

  default:
    run_error("Unknown system call: %d\n", R[REG_V0]);
    break;
  }

#ifdef _WIN32
  windowsParameterHandlingControl(1);
#endif
  return (1);
}

void handle_exception()
{
  if (!quiet && CP0_ExCode != ExcCode_Int)
    error("Exception occurred at PC=0x%08x\n", CP0_EPC);

  exception_occurred = 0;
  PC = EXCEPTION_ADDR;

  switch (CP0_ExCode)
  {
  case ExcCode_Int:
    break;

  case ExcCode_AdEL:
    if (!quiet)
      error("  Unaligned address in inst/data fetch: 0x%08x\n", CP0_BadVAddr);
    break;

  case ExcCode_AdES:
    if (!quiet)
      error("  Unaligned address in store: 0x%08x\n", CP0_BadVAddr);
    break;

  case ExcCode_IBE:
    if (!quiet)
      error("  Bad address in text read: 0x%08x\n", CP0_BadVAddr);
    break;

  case ExcCode_DBE:
    if (!quiet)
      error("  Bad address in data/stack read: 0x%08x\n", CP0_BadVAddr);
    break;

  case ExcCode_Sys:
    if (!quiet)
      error("  Error in syscall\n");
    break;

  case ExcCode_Bp:
    exception_occurred = 0;
    return;

  case ExcCode_RI:
    if (!quiet)
      error("  Reserved instruction execution\n");
    break;

  case ExcCode_CpU:
    if (!quiet)
      error("  Coprocessor unuable\n");
    break;

  case ExcCode_Ov:
    if (!quiet)
      error("  Arithmetic overflow\n");
    break;

  case ExcCode_Tr:
    if (!quiet)
      error("  Trap\n");
    break;

  case ExcCode_FPE:
    if (!quiet)
      error("  Floating point\n");
    break;

  default:
    if (!quiet)
      error("Unknown exception: %d\n", CP0_ExCode);
    break;
  }
}
