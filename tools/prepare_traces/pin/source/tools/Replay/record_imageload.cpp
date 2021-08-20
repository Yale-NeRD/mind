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

//
// This tool records or replays image load events and associated image data.
// The test requires using it twice.
//
// Note that on replay it doesn't execute the loaded code, just re-generates the
// image load trace.
//

#include <stdio.h>
#include <stdlib.h>
#include "pin.H"
using std::string;

KNOB<BOOL> KnobReplay(KNOB_MODE_WRITEONCE, "pintool", "r", "0", "replay if 1, default is to log");
KNOB<BOOL> KnobVerbose(KNOB_MODE_WRITEONCE, "pintool", "v", "0", "print more verbose messages");

// This is questionable, but should be fine. Doing something smarter does not seem warranted
// for this simple test.
// The problems with MAX_PATH (or Windows PATH_MAX) are described here
// http://stackoverflow.com/questions/833291/is-there-an-equivalent-to-winapis-max-path-under-linux-unix
// for instance.
//
#define MAX_FILENAME_LENGTH 4096

static FILE * trace;
static FILE * imgLog;
static int  imageCount = 0;
static BOOL logging = FALSE;

static int getReplayImageType(IMG img)
{
    if (IMG_IsMainExecutable(img))
        return REPLAY_IMAGE_TYPE_MAIN_EXE;
    if (IMG_IsInterpreter(img))
        return REPLAY_IMAGE_TYPE_INTERPRETER;
    return REPLAY_IMAGE_TYPE_REGULAR;
}

// Save the image load event 
static void LogImageLoad(IMG img, void *v)
{
    if (IMG_IsVDSO(img))
    {
        return;
    }

    // Ensure that we can't overflow when we read it back.
    ASSERTX (IMG_Name(img).length() < MAX_FILENAME_LENGTH);

    // Log the data needed to restore it
    fprintf (imgLog, "L '%s' 0x%lx %d\n", IMG_Name(img).c_str(), (unsigned long)IMG_LoadOffset(img), getReplayImageType(img));
}

// Save the image unload event 
static void LogImageUnload(IMG img, void *)
{
    if (IMG_IsVDSO(img))
    {
        return;
    }

    ASSERTX (IMG_Name(img).length() < MAX_FILENAME_LENGTH);

    // Log the unload event.
    fprintf (imgLog, "U '%s'\n", IMG_Name(img).c_str());
}


// Parse the image description
static void ParseImageLoadLine(string &imageName,  ADDRINT *offset, REPLAY_IMAGE_TYPE* type)
{
    // Data was written like this :-
    // fprintf (imgLog, "L '%s' 0x%x\n", IMG_Name(img).c_str(), 
    //          IMG_LoadOffset(img), base, IMG_HighAddress(img));
    char imgNameBuffer[MAX_FILENAME_LENGTH];
    int typeInt = (int)REPLAY_IMAGE_TYPE_REGULAR;

    int itemsRead = fscanf(imgLog," '%[^']' %p %d\n",&imgNameBuffer[0], (void**)offset, &typeInt);
    if (itemsRead != 3)
    {
        fprintf (trace, "ParseImageLoadLine: Failed to parse; parsed %d expected to parse 3\n", itemsRead);
        exit(1);
    }
    imageName = imgNameBuffer;
    *type = (REPLAY_IMAGE_TYPE)typeInt;
}

static void ParseImageUnloadLine(string &imageName)
{
    // Data was written like this :-
    // fprintf (imgLog, "U '%s');
    char imgNameBuffer[MAX_FILENAME_LENGTH];

    int itemsRead = fscanf(imgLog," '%[^']'\n",&imgNameBuffer[0]);
    if (itemsRead != 1)
    {
        fprintf (trace, "ParseImageUnloadLine: Failed to parse\n");
        exit(1);
    }
    imageName = imgNameBuffer;
}

static IMG FindNamedImg(const string& imgName)
{
    // Visit every loaded image
    for (IMG img= APP_ImgTail(); IMG_Valid(img); img = IMG_Prev(img))
    {
        if (IMG_Name(img) == imgName)
            return img;
    }
    return IMG_Invalid();
}

// Replay the image log.
// We run this before the each instruction of the code as an analysis routine.
// So we eat up the image loads one instruction at a time!
// We can also call it before PIN_StartProgram, to check that queuing
// the replay calls up works.
//
static void ReplayImageEntry()
{
    if (feof(imgLog))
        exit(0);

    char tag = fgetc(imgLog);
    switch (tag)
    {
        case 'L':
            {
                string imageName;
                ADDRINT offset;
                REPLAY_IMAGE_TYPE type;

                ParseImageLoadLine(imageName, &offset, &type);
                if (KnobVerbose)
                    fprintf (trace, "Replaying load for %s\n", imageName.c_str());
                // And, finally, inform Pin that it is all there, which will invoke
                // image load callbacks.
                PIN_LockClient();
                // Tag the first image as the main program
                PIN_ReplayImageLoad(imageName.c_str(), imageName.c_str(), offset, type);
                PIN_UnlockClient();

                break;
            }
        case 'U':
            {
                string imageName;
                ParseImageUnloadLine(imageName);
                
                IMG img = FindNamedImg(imageName);
                if (KnobVerbose)
                    fprintf (trace, "Replaying unload for %s\n", imageName.c_str());
                // And, finally, inform Pin that it has gone, which will invoke
                // image unload callbacks.
                PIN_LockClient();
                PIN_ReplayImageUnload(img);
                PIN_UnlockClient();
                break;
            }            
        default:
            fprintf (trace, "Unexpected line in log file starting with '%c'\n", tag);
            exit(0);
    }
}

static VOID InstrumentInstruction (INS ins, void *)
{
    INS_InsertCall (ins, IPOINT_BEFORE, (AFUNPTR)ReplayImageEntry, IARG_END);
}

// Logging to demonstrate that the rest of the tool does replay the image load
// and unload operations, and that the Pin data structures are valid.

// Grab some detailed information about the image, the number of SECs and RTNs
static VOID CountImageSecsAndRtns (IMG img, int *nSecs, int *nRtns)
{
    int numSecs = 0;
    int numRtns = 0;

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        numSecs++;
        for (RTN rtn=SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
            numRtns++;
        }
    }

    *nSecs = numSecs;
    *nRtns = numRtns;
}

// Print the list of images currently loaded, with some information about each.
static VOID PrintImageList()
{
    for (IMG img= APP_ImgHead(); IMG_Valid(img); img = IMG_Next(img))
    {
        int nSecs;
        int nRtns;

        if (IMG_IsVDSO(img))
        {
            continue;
        }

        CountImageSecsAndRtns (img, &nSecs, &nRtns);
        fprintf (trace, "   L  %-40s [0x%lx:0x%lx] offset 0x%lx %2d SECs %4d RTNs\n", IMG_Name(img).c_str(),
                 (unsigned long)IMG_LowAddress(img), (unsigned long)IMG_HighAddress(img), (unsigned long)IMG_LoadOffset(img), nSecs, nRtns);
    }    
}

// Trace an image load event
static VOID TraceImageLoad(IMG img, VOID *v)
{
    if (IMG_IsVDSO(img))
    {
        return;
    }

    fprintf(trace, "[%2d]+ %-40s\n", imageCount++, IMG_Name(img).c_str());
    PrintImageList();
}

// Trace an image unload event
static VOID TraceImageUnload(IMG img, VOID *v)
{
    if (IMG_Name(img).find("[vdso]") != std::string::npos)
    {
        return;
    }

    fprintf(trace, "[%2d]- %-40s\n", imageCount--, IMG_Name(img).c_str());
    PrintImageList();
}

// This function is called when the application exits
static VOID Fini(INT32 code, VOID *v)
{
    fclose(trace);
    fclose(imgLog);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

static INT32 Usage()
{
    PIN_ERROR("This tool prints a log of image load and unload events, logs them and can replay them\n"
             + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char * argv[])
{
    // Initialize symbol processing
    PIN_InitSymbols();
    
    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();

    logging = ! KnobReplay.Value();

    if (logging)
    {
        trace  = fopen("record_imageload_rec.out", "w");
        imgLog = fopen("imageload.log", "w");

        IMG_AddInstrumentFunction(LogImageLoad, 0);
        IMG_AddUnloadFunction    (LogImageUnload, 0);
    }
    else
    {  // Replaying
        trace  = fopen("record_imageload_play.out", "w");
        imgLog = fopen("imageload.log", "r");

        // We will handle image load operations.
        PIN_SetReplayMode (REPLAY_MODE_IMAGEOPS);
        // And then we replay the first two image load ops before we start the program.
        // We do this even before adding the image instrumentation callback, that should still work,
        // Pin should defer these and replay them inside PIN_StartProgram.
        ReplayImageEntry();
        ReplayImageEntry();

        INS_AddInstrumentFunction (InstrumentInstruction, 0);
    }

    // These Trace functions demonstrate that the events are happening, they are the client...
    IMG_AddInstrumentFunction(TraceImageLoad, 0);
    IMG_AddUnloadFunction    (TraceImageUnload, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
