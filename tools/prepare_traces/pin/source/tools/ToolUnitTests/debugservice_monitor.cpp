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

#include <stdio.h>
#include "pin.H"

using std::string;

FILE * out;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "debugservice_monitor.out", "output file");

/*
 * Return TRUE if baseName matches tail of imageName. Comparison is case-insensitive.
 * @param[in]  imageName  image file name in either form with extension
 * @param[in]  baseName   image base name with extension (e.g. kernel32.dll)
 */
static BOOL CmpBaseImageName(const string & imageName, const string & baseName)
{
    if (imageName.size() >= baseName.size())
    {
        return _stricmp(imageName.c_str() + imageName.size() - baseName.size(), baseName.c_str()) == 0;
    }
    return FALSE;
}

void BeforeOutputDebugString ()
{
    fprintf(out, "BeforeOutputDebugString\n");
    fflush (out);
}

VOID Fini(INT32 code, VOID *v)
{
    fprintf(out, "PinFiniFunction\n");
    fclose(out);
}

VOID Image(IMG img, VOID *v)
{
    // Skip all images, but kernel32.dll
    if (!CmpBaseImageName(IMG_Name(img), "kernel32.dll"))
    {
        return;
    }

    // hook the functions in the image. If these functions are called then it means
    // that pin has not lost control.
    RTN rtn = RTN_FindByName(img, "OutputDebugStringA");
    if (RTN_Valid(rtn))
    {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(BeforeOutputDebugString), IARG_END);
        RTN_Close(rtn);
    }
    rtn = RTN_FindByName(img, "OutputDebugStringW");
    if (RTN_Valid(rtn))
    {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(BeforeOutputDebugString), IARG_END);
        RTN_Close(rtn);
    }
    
}

int main(INT32 argc, CHAR **argv)
{
    PIN_InitSymbols();
    PIN_Init(argc, argv);

    out = fopen(KnobOutputFile.Value().c_str(), "w");

    IMG_AddInstrumentFunction(Image, 0);

    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}
