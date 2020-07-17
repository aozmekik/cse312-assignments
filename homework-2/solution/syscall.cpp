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

#include <assert.h>
#include <map>
#include <queue>
#include <list>

using namespace std;

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

/* utility functions implement for the hw. */
#include "spim-utils.h"
#define BYTES_TO_INST(N) (((N) + BYTES_PER_WORD - 1) / BYTES_PER_WORD * sizeof(instruction*))


/**
 * process, process-table entry.
 * contains the fields necessary to store for a typical process.
 * fields are exported from 'mem.h' header.
**/
class Process {
public:

   /* process states */
    enum State {
        Running,
        Ready,
        Blocked,
    };

    // TODO: don't forget to return the $v0=0 to child process and $v0=child_pid to parent process.

    Process() = delete;
    Process(Process&) = delete;

    /**
     * initialize a process image, by copying the image of given current caller.
     * fork design implemented here for creating the process, because we won't
     * need any other way after all.
     */
    Process(char *name, bool init, int id){
        p_data_size = ROUND_UP(initial_data_size, BYTES_PER_WORD);
        p_stack_size = ROUND_UP(initial_stack_size, BYTES_PER_WORD);
        p_k_data_size = ROUND_UP(initial_k_data_size, BYTES_PER_WORD);

        /* start of image cloning  */

        if (!init) { // create segment if not init.
            p_data_seg = (mem_word *) xmalloc(p_data_size);
            p_stack_seg = (mem_word *) xmalloc(p_stack_size);
            p_k_data_seg = (mem_word *) xmalloc(p_k_data_size);
        }


        ID = id;
        parentID = CCR[1][0];
        strcpy(this->name, name);
        this->init = init;
        p_state = State::Ready;
        fromCPU(true); // copy the img from current context.

        /* end of image cloning. */

        /* we need to store process id to somewhere in the context of process.
         * I chose CCRs specific index, in this context to store id. */
        p_CCR[1][0] = (reg_word) ID;
        if (init)
            storeID();
    };

    int getID() const {
        return ID;
    }

    State getState() const {
        return p_state;
    }

    char* getName(){
        return name;
    }

    void changeState(State state){
        p_state = state;
    }

    int getParentID() const {
        return parentID;
    }

    void addChild(int id){
        children.push_back(id);
    }

    void removeChild(int id){
        children.remove(id);
    }

    int getChild() const{
        if (children.empty())
            return -1;
        else
            return children.front();
    }

    bool isMyChild(int id) const {
        for (auto &it: children)
            if (it == id)
                return true;
        return false;
    }

    void setName(char *n){
        strcpy(name, n);
    }

    void print() const {
        write_output(console_out, "ID\t\t\t:\t%d\nParentID\t\t:\t%d\nProcessName\t\t:\t%s\n"
                                  "PragramCounter\t\t:\t0x%x\nR[V0]\t\t\t:\t%d\n", ID, parentID, name, p_PC, p_R[REG_V0]);

        if (!children.empty()){
            write_output(console_out, "ChildList\t\t:\t[ ");
            for (auto &it : children){
                write_output(console_out, "%d ", it);
            }
            write_output(console_out, "]\n");
        }
        else
            write_output(console_out, "ChildList\t\t:\t[ Empty ]\n");


        switch (p_state){
            case State::Blocked: write_output(console_out, "State\t\t\t:\tBlocked\n\n"); break;
            case State::Running: write_output(console_out, "State\t\t\t:\tRunning\n\n"); break;
            case State::Ready: write_output(console_out, "State\t\t\t:\tReady\n\n"); break;
            default: exit(EXIT_FAILURE);
        }

    }

    void fromCPU(bool first=false){
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
        p_k_data_seg_b = (BYTE_TYPE *) k_data_seg;
        p_k_data_seg_h = (short *) k_data_seg;

        if (first && strcmp(name, "init") == 0 && !init){
            memcpy(p_data_seg, data_seg, p_data_size);
            memcpy(p_stack_seg, stack_seg, p_stack_size);
            memcpy(p_k_data_seg, k_data_seg, p_k_data_size);
        }
        else {
            p_data_seg = data_seg;
            p_stack_seg = stack_seg;
            p_k_data_seg = k_data_seg;
        }
        p_text_seg = text_seg;
        p_k_text_seg = k_text_seg;

    }
    void toCPU(){
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
        k_data_seg_b = (BYTE_TYPE *) p_k_data_seg;
        k_data_seg_h = (short *) p_k_data_seg;
    }

    void clear(){
        free(p_data_seg);
        free(p_stack_seg);
        free(p_k_data_seg);
        if ((strcmp(name, "init") != 0)){
            free(p_text_seg);
            free(p_k_text_seg);
        }

    }

    /**
     * once the process becomes dead, memory is freed.
     * init is kept on the memory, because it's resources are allocated by the spim
     * so freed is done by them. to be able to make spim terminate properly (without seg. fault)
     * we shouldn't free init.
     */
    ~Process(){
        if (ID != 0)
            clear();
    }

private:
    /* they are all named with p_ leading convention because,
     * there exits a global alias taken without it.  */

    /* identifier */
    int ID;
    int parentID;
    char name[30];
    bool init;
    std::list<int> children;

    /* state */
    State p_state;

    /* resource */
    reg_word p_R[R_LENGTH]; // registers.
    reg_word p_HI, p_LO;
    mem_addr p_PC, p_nPC; // program counter
    reg_word p_CCR[4][32], p_CPR[4][32];
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
    /* end of resource */

    int storeID(){
        int oldID = CCR[1][0];
        CCR[1][0] = ID;
        return oldID;
    };

};


class OS {
public:
    OS (): init(nullptr) {
        srand(time(NULL));
    };

    /**
     * creates a process by cloning the caller's image. (as the posix's fork do.)
     * sets the V0 of child to 0, sets the V0 of caller to childpid.
     * @param name, the name of the process.
     */
    void fork(){
        PC += BYTES_PER_WORD;
        process_table[count+1] = new Process(current()->getName(), false, count+1);
        PC -= BYTES_PER_WORD;
        requestQueue.push(count+1); // process in the queue.
        ++count;
        setReturn(count, 0); // set return of child to 0
        setReturn(current()->getID(), count); // set return of parent to child pid.
        current()->addChild(count);
    }

    void setReturn(int id, int ret){
        int prev_id = contextSwitch(id);
        R[REG_V0] = ret;
        contextSwitch(prev_id);
    }


    /**
     * interrupt handler.
     * implements robin round scheduling.
     * interrupts the current process and makes the requested process goes in front,
     * executes new process until being interrupted.
     */
    int schedule(bool cont=true){
        if (requestQueue.empty()){
            return 0;
        }

        write_output(console_out, "\n\n____________________________________________________________________\n\n");
        write_output(console_out, "[***\t\tContext Scheduling Has Happened!\t\t***]\n");
        write_output(console_out, "\t\t{\tProcess Table:\t\t}\n");
        print();
        write_output(console_out, ">Blocked:\t\tID %d\n", getCurrentID());
        write_output(console_out, ">Changing State:\tID %d\n", requestQueue.front());
        write_output(console_out, "\n[***\t\t{\t\tEND\t\t}\t\t***]\n");
        write_output(console_out, "_____________________________________________________________________\n\n");



        Process* p = process(requestQueue.front());
        requestQueue.pop();
        assert(p->getState() != Process::State::Running);
        if(cont) // current process is not finished, so interrupt.
            interrupt(getCurrentID());
        p->toCPU();
        p->changeState(Process::State::Running);
        return p->getID();
    }

    /**
     * wait for a child process (of pid) with given id to change state.
     * @id, if 0, waits for any child process, if >0, waits the child process with given id.
     * @return cid, if the requested process is terminated.
     * 0, if child process is running.
     * -1, is error. (no cid exits)
     */
    void waitpid(int pid, int cid){
        int status = 0;
        if (cid == 0)
            cid = process(pid)->getChild();
        if (cid > 0){
            if (isTerminated(pid, cid)){
                status = cid;
                process(pid)->removeChild(cid);
            } // child's state changed to terminated.
            else
                status = 0;
        }else
            status = -1;
        setReturn(pid, status);
    }

    void execve(int id, char *name){
        assert(id <= count); // id doesn't exit in the process table.
        load(id, name);
    }

    void print()  {
        for (auto & itr : process_table)
            itr.second->print();
    }

    void kill(int id) {
//        process(process(id)->getParentID())->removeChild(id);
        process_table.erase(id);
    }

    void updateInit() {
        if (!process_table[0]){
            init = new Process("init", true, 0);
            process_table[0] = init; // first item is init process in table.
        }
    }


    /**
     * loads the .asm file to the image of process with given id.
     * @param id, process id.
     * @param name, program name to image.
     */
    void load(int id, char *name){
        if (access(name, F_OK) != -1) { // if file exist.
            /* to load img, process must be moved to CPU */
            process(id)->setName(name);
            int prev_id = contextSwitch(id);
            text_seg = NULL;
            data_seg = NULL;
            stack_seg = NULL;
            k_text_seg = NULL;
            k_data_seg = NULL;
            initialize_world(exception_file_name, false);
            read_assembly_file(name);
            PC = starting_address();
            contextSwitch(prev_id);
        }
        else
        {
            write_output(console_out, "%s", name);
        }

    }

    int getCurrentID(){
        return (int) CCR [1][0];
    }

    ~OS() {
        for (auto & itr : process_table)
            delete itr.second;

    }
private:
    Process *init; // first process created.
    std::map<int, Process*> process_table;
    std::queue<int> requestQueue; // for round robing scheduling process queue.
    int count = 0;

    /**
     * returns the process with given id.
     */
    Process* process(int id){
        return process_table[id];
    }

    /**
     * get the current process on CPU.
     */
    Process* current(){
        int id = getCurrentID();
        assert(id <= count);
        return process(id);
    }

    /**
     * change current process on CPU, to process with given id.
     * @return previous process's id.
     */
    int contextSwitch(int id) {
        assert(id <= count);
        Process *p = process(id); // will be switched to.
        int old_id = current()->getID();
        current()->fromCPU();
        p->toCPU();
        return old_id;
    }

    /**
     * the requested process being interrupted.
     */
    void interrupt(int id){
        assert(process(id)->getState() != Process::State::Blocked);
        process(id)->changeState(Process::State::Blocked);
        process(id)->fromCPU();

        requestQueue.push(id); // added to queue for scheduling.
    }

    bool isTerminated(int pid, int cid){
//        assert(pid == process(cid)->getParentID() && process(pid)->isMyChild(cid));
        bool not_found = process_table.find(cid) == process_table.end();

        return not_found && process(pid)->isMyChild(cid);
    }

};

OS  os;

/*You implement your handler here*/
void SPIM_timerHandler()
{
   // Implement your handler..
   os.schedule();

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

      /**
       * this call creates and exact clone of the calling process. after the
       * fork, the two processes, the parent and the child have the same memory image.
       */
        case FORK_SYSCALL: {
            os.updateInit();
            os.fork();
            break;
        }

        /**
         * $a0 <= -1, wait for any child process.
         * $a0 > 0 wait for the child process ID with given $a0.
         */
        case WAITPID_SYSCALL: {
            os.updateInit();
            os.waitpid(os.getCurrentID(), R[REG_A0]);
            break;
        }

        /**
         * this call changes the image of a given id and run a new program.
         */
        case EXECVE_SYSCALL: {
            os.updateInit();
            char name[30];
            strcpy(name, (char*) mem_reference (R[REG_A0]));
            os.execve(os.getCurrentID(), name);
            break;
        }

        /**
         * exit syscall for the os, kills the caller process (also erases it from p table) and
         * schedules it again.
         */
        case PROCESS_EXIT_SYSCALL:{
            spim_return_value = 0;

            if (os.getCurrentID() == 0) // if init, terminate os.
                return 0;
            else // delete current process from process table.
                os.kill(os.getCurrentID());

            os.schedule(false);
            PC -=4; // decrement PC to neutralize increment after this syscall.
            break;
        }

        /**
         * generates a random number between 0<=x<=2.
         * very specific usage for picking random program for kernel flavors.
         */
        case RANDOM_SYSCALL: {
            R[REG_V0] = rand() % 3;
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
