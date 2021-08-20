/*
 * Copyright 2002-2020 Intel Corporation.
 * 
 * This software is provided to you as Sample Source Code as defined in the accompanying
 * End User License Agreement for the Intel(R) Software Development Products ("Agreement")
 * section 1.L.
 * 
 * This software and the related documents are provided as is, with no express or implied
 * warranties, other than those that are expressly stated in the License.
 */

/*
 * A sample tool that extends GDB by adding a "watchpoint" command, which stops
 * at a breakpoint when a specified memory address is written.
 */

#include <sstream>
#include "pin.H"
using std::string;

KNOB<BOOL> KnobUseIargConstContext(KNOB_MODE_WRITEONCE, "pintool",
                                   "const_context", "0", "use IARG_CONST_CONTEXT");

static VOID Instruction(INS, VOID *);
static ADDRINT OnMemWriteIf(ADDRINT);
static VOID DoBreakpoint(CONTEXT *, THREADID);
static BOOL DebugInterpreter(THREADID, CONTEXT *, const string &, string *, VOID *);

static ADDRINT WatchAddr = 0;
static bool SkipOne = false;


int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv))
        return 1;

    PIN_AddDebugInterpreter(DebugInterpreter, 0);
    INS_AddInstrumentFunction(Instruction, 0);

    PIN_StartProgram();
    return 0;
}


static VOID Instruction(INS ins, VOID *)
{
    if (INS_IsMemoryWrite(ins))
    {
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)OnMemWriteIf, IARG_MEMORYWRITE_EA, IARG_END);
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)DoBreakpoint, 
                           (KnobUseIargConstContext)?IARG_CONST_CONTEXT:IARG_CONTEXT,
                           // IARG_CONST_CONTEXT has much lower overhead 
                           // than IARG_CONTEX for passing the CONTEXT* 
                           // to the analysis routine. Note that IARG_CONST_CONTEXT
                           // passes a read-only CONTEXT* to the analysis routine 
                           IARG_THREAD_ID, IARG_END);
    }
}


static ADDRINT OnMemWriteIf(ADDRINT addr)
{
    bool skipOne = SkipOne;
    SkipOne = false;
    return ((WatchAddr == addr) && !skipOne);
}


static VOID DoBreakpoint(CONTEXT *ctxt, THREADID tid)
{
    SkipOne = true;

    std::ostringstream os;
    os << "Watchpoint triggered at 0x" << std::hex << WatchAddr;
    PIN_ApplicationBreakpoint(ctxt, tid, FALSE, os.str());
}


static BOOL DebugInterpreter(THREADID, CONTEXT *ctxt, const string &cmd, string *res, VOID *)
{
    if (cmd.find("watch") == 0)
    {
        std::istringstream is(&cmd.c_str()[sizeof("watch")-1]);
        ADDRINT addr;
        is >> std::hex >> addr;
        if (is)
        {
            WatchAddr = addr;
            std::ostringstream os;
            os << "Watching address 0x" << std::hex << addr << "\n";
            *res = os.str();
        }
        return TRUE;
    }
    return FALSE;
}
