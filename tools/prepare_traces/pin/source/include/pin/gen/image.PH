//Groups: @ingroup\s+(API_REF|KNOBS|IMG_BASIC_API|INS_BASIC_API|INS_INST_API|INS_BASIC_API_GEN_IA32|INS_BASIC_API_IA32|INS_MOD_API_GEN_IA32|SEC_BASIC_API|RTN_BASIC_API|REG_BASIC_API|REG_CPU_GENERIC|REG_CPU_IA32|TRACE_BASIC_API|BBL_BASIC_API|SYM_BASIC_API|MISC_PRINT|MISC_PARSE|KNOB_API|KNOB_BASIC|KNOB_PRINT|LOCK|PIN_CONTROL|TRACE_VERSION_API|BUFFER_API|PROTO_API|PIN_PROCESS_API|PIN_THREAD_API|PIN_SYSCALL_API|WINDOWS_SYSCALL_API_UNDOC|DEBUG_API|ERROR_FILE_BASIC|TYPE_BASE|INSTLIB|ALARM|CHILD_PROCESS_API|UTILS|MISC|CONTEXT_API|PHYSICAL_CONTEXT_API|PIN_CALLBACKS|EXCEPTION_API|APPDEBUG_API|STOPPED_THREAD_API|BUFFER_API|PROTO|INST_ARGS|DEPRECATED_PIN_API|INTERNAL_EXCEPTION_PRIVATE_UNDOCUMENTED|PIN_THREAD_PRIVATE|CHILD_PROCESS_INTERNAL|BBL_BASIC|ROGUE_BASIC_API|MESSAGE_TYPE|MESSAGE_BASIC|ERRFILE|MISC_BASIC|ITC_INST_API|CONTEXT_API_UNDOC|EXCEPTION_API_UNDOC|UNDOCUMENTED_PIN_API|OPIN|TRACE_VERSIONS
/* PIN API */

/* THIS FILE IS AUTOMAGICALLY GENERATED - DO NOT CHANGE DIRECTLY*/


extern IMG IMG_CreateAt(const char* filename, ADDRINT start, USIZE size, ADDRINT loadOffset, BOOL mainExecutable);

                                                                  /* DO NOT EDIT */
extern VOID IMG_ReplayImageLoad(IMG img);

                                                                  /* DO NOT EDIT */
extern VOID* IMG_GetLoaderInfo(IMG img);

                                                                  /* DO NOT EDIT */
extern VOID IMG_SetLoaderInfo(IMG img, VOID* loaderInfo);

                                                                  /* DO NOT EDIT */
extern IMG IMG_Next(IMG img);

                                                                  /* DO NOT EDIT */
extern IMG IMG_Prev(IMG img);

                                                                  /* DO NOT EDIT */
extern IMG IMG_Invalid();

                                                                  /* DO NOT EDIT */
extern BOOL IMG_Valid(IMG img);

                                                                  /* DO NOT EDIT */
extern SEC IMG_SecHead(IMG img);

                                                                  /* DO NOT EDIT */
extern SEC IMG_SecTail(IMG img);

                                                                  /* DO NOT EDIT */
extern SYM IMG_RegsymHead(IMG img);

                                                                  /* DO NOT EDIT */
extern PIN_DEPRECATED_API ADDRINT IMG_Entry(IMG img);

                                                                  /* DO NOT EDIT */
extern ADDRINT IMG_EntryAddress(IMG img);

                                                                  /* DO NOT EDIT */
extern BOOL IMG_HasProperty(IMG img, IMG_PROPERTY property);

                                                                  /* DO NOT EDIT */
extern const std::string& IMG_Name(IMG img);

                                                                  /* DO NOT EDIT */
extern ADDRINT IMG_Gp(IMG img);

                                                                  /* DO NOT EDIT */
extern ADDRINT IMG_LoadOffset(IMG img);

                                                                  /* DO NOT EDIT */
extern ADDRINT IMG_LowAddress(IMG img);

                                                                  /* DO NOT EDIT */
extern ADDRINT IMG_HighAddress(IMG img);

                                                                  /* DO NOT EDIT */
extern  ADDRINT IMG_StartAddress(IMG img);

                                                                  /* DO NOT EDIT */
extern USIZE IMG_SizeMapped(IMG img);

                                                                  /* DO NOT EDIT */
extern IMG_TYPE IMG_Type(IMG img);

                                                                  /* DO NOT EDIT */
extern BOOL IMG_IsMainExecutable(IMG x);

                                                                  /* DO NOT EDIT */
extern BOOL IMG_hasLinesData(IMG x);

                                                                  /* DO NOT EDIT */
extern BOOL IMG_IsInterpreter(IMG x);

                                                                  /* DO NOT EDIT */
extern BOOL IMG_IsStaticExecutable(IMG x);

                                                                  /* DO NOT EDIT */
extern BOOL IMG_IsVDSO(IMG img);

                                                                  /* DO NOT EDIT */
extern UINT32 IMG_NumRegions(IMG img);

                                                                  /* DO NOT EDIT */
extern ADDRINT IMG_RegionHighAddress(IMG img, UINT32 n);

                                                                  /* DO NOT EDIT */
extern ADDRINT IMG_RegionLowAddress(IMG img, UINT32 n);

                                                                  /* DO NOT EDIT */
extern IMG SEC_Img(SEC sec);

                                                                  /* DO NOT EDIT */
extern SEC SEC_Next(SEC sec);

                                                                  /* DO NOT EDIT */
extern SEC SEC_Prev(SEC sec);

                                                                  /* DO NOT EDIT */
extern SEC SEC_Invalid();

                                                                  /* DO NOT EDIT */
extern BOOL SEC_Valid(SEC x);

                                                                  /* DO NOT EDIT */
extern RTN SEC_RtnHead(SEC sec);

                                                                  /* DO NOT EDIT */
extern RTN SEC_RtnTail(SEC sec);

                                                                  /* DO NOT EDIT */
extern const std::string& SEC_Name(SEC sec);

                                                                  /* DO NOT EDIT */
extern SEC_TYPE SEC_Type(SEC sec);

                                                                  /* DO NOT EDIT */
extern BOOL SEC_Mapped(SEC sec);

                                                                  /* DO NOT EDIT */
extern const VOID *SEC_Data(SEC sec);

                                                                  /* DO NOT EDIT */
extern SEC RTN_Sec(RTN x);

                                                                  /* DO NOT EDIT */
extern RTN RTN_Next(RTN x);

                                                                  /* DO NOT EDIT */
extern RTN RTN_Prev(RTN x);

                                                                  /* DO NOT EDIT */
extern RTN RTN_Invalid();

                                                                  /* DO NOT EDIT */
extern BOOL RTN_Valid(RTN x);

                                                                  /* DO NOT EDIT */
extern const std::string& RTN_Name(RTN x);

                                                                  /* DO NOT EDIT */
extern BOOL RTN_IsArtificial(RTN x);

                                                                  /* DO NOT EDIT */
extern SYM RTN_Sym(RTN x);

                                                                  /* DO NOT EDIT */
extern UINT RTN_DynamicMethodId(RTN x);

                                                                  /* DO NOT EDIT */
extern AFUNPTR RTN_Funptr(RTN x);

                                                                  /* DO NOT EDIT */
extern UINT32 RTN_Id(RTN x);

                                                                  /* DO NOT EDIT */
extern INS BBL_InsHead(BBL x);

                                                                  /* DO NOT EDIT */
extern INS BBL_InsTail(BBL x);

                                                                  /* DO NOT EDIT */
extern BBL BBL_Next(BBL x);

                                                                  /* DO NOT EDIT */
extern BBL BBL_Prev(BBL x);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_Valid(BBL x);

                                                                  /* DO NOT EDIT */
extern RTN INS_Rtn(INS x);

                                                                  /* DO NOT EDIT */
extern INS INS_Next(INS x);

                                                                  /* DO NOT EDIT */
extern INS INS_Prev(INS x);

                                                                  /* DO NOT EDIT */
extern INS INS_Invalid();

                                                                  /* DO NOT EDIT */
extern BOOL INS_Valid(INS x);

                                                                  /* DO NOT EDIT */
extern ADDRINT INS_Address(INS ins);

                                                                  /* DO NOT EDIT */
extern USIZE INS_Size(INS ins);

                                                                  /* DO NOT EDIT */
extern PIN_DEPRECATED_API ADDRINT INS_DirectBranchOrCallTargetAddress(INS ins);

                                                                  /* DO NOT EDIT */
extern ADDRINT INS_DirectControlFlowTargetAddress(INS ins);

                                                                  /* DO NOT EDIT */
extern ADDRINT INS_NextAddress(INS ins);

                                                                  /* DO NOT EDIT */
extern SYM SYM_Next(SYM x);

                                                                  /* DO NOT EDIT */
extern SYM SYM_Prev(SYM x);

                                                                  /* DO NOT EDIT */
extern const std::string& SYM_Name(SYM x);

                                                                  /* DO NOT EDIT */
extern SYM SYM_Invalid();

                                                                  /* DO NOT EDIT */
extern BOOL SYM_Valid(SYM x);

                                                                  /* DO NOT EDIT */
extern BOOL SYM_Dynamic(SYM x);

                                                                  /* DO NOT EDIT */
extern BOOL SYM_GeneratedByPin(SYM x);

                                                                  /* DO NOT EDIT */
extern BOOL SYM_IFuncImplementation(SYM x);

                                                                  /* DO NOT EDIT */
extern BOOL SYM_IFuncResolver(SYM x);

                                                                  /* DO NOT EDIT */
extern ADDRINT SYM_Value(SYM x);

                                                                  /* DO NOT EDIT */
extern UINT32 SYM_Index(SYM x);

                                                                  /* DO NOT EDIT */
extern ADDRINT SYM_Address(SYM x);

                                                                  /* DO NOT EDIT */
typedef VOID (*IMAGECALLBACK)(IMG, VOID *);

                                                                  /* DO NOT EDIT */
enum UNDECORATION {
    UNDECORATION_COMPLETE,  ///< Undecorate to full symbol signature
    UNDECORATION_NAME_ONLY  ///< Undecorate to [scope::]name
};

                                                                  /* DO NOT EDIT */
enum SYMBOL_INFO_MODE {
    NO_SYMBOLS     = 0,       ///< No symbols required
    EXPORT_SYMBOLS = (1<<0),  ///< Only symbols taken from export table are provided (Windows only)
    DEBUG_SYMBOLS  = (1<<1),  ///< Debug symbols (Windows only, currently implemented as DEBUG_OR_EXPORT_SYMBOLS)
    IFUNC_SYMBOLS  = (1<<2),  ///< IFUNC symbols (Linux only)
    DEBUG_OR_EXPORT_SYMBOLS = (DEBUG_SYMBOLS | EXPORT_SYMBOLS) ///< First debug symbols if available,
                                                               ///<  otherwise export symbols (Windows only)
};

                                                                  /* DO NOT EDIT */
class SymbolAddressRange
{
    public:
        SymbolAddressRange() : base(0), size(0), name("") {}
        SymbolAddressRange(std::string sym_name, ADDRINT sym_base, size_t sym_size) :
            base(sym_base), size(sym_size), name(sym_name) {}
        SymbolAddressRange(const SymbolAddressRange &obj) {base=obj.base; size=obj.size; name=obj.name;}
        SymbolAddressRange& operator = (const SymbolAddressRange &obj) {base=obj.base; size=obj.size; name=obj.name; return *this;}
        BOOL operator < (const SymbolAddressRange &  range) const {return (base < range.base);}
        BOOL Contains(const ADDRINT addr) const { return ((addr-base)<size); }
    public:
        ADDRINT base;
        size_t size;
        std::string name;
};

                                                                  /* DO NOT EDIT */
struct SymbolDebugInfo
{
    SymbolDebugInfo(std::string sym_name, ADDRINT sym_base, size_t sym_size) :
        name(sym_name), range(sym_name, sym_base, sym_size) {}
    std::string name;
    SymbolAddressRange range;
};

                                                                  /* DO NOT EDIT */
extern UINT32 IMG_Id(IMG x);

                                                                  /* DO NOT EDIT */
extern VOID* IMG_DynamicRawData(IMG img);

                                                                  /* DO NOT EDIT */
extern IMG IMG_FindImgById(UINT32 id);

                                                                  /* DO NOT EDIT */
extern IMG IMG_FindByAddress(ADDRINT address);

                                                                  /* DO NOT EDIT */
extern PIN_CALLBACK RTN_AddInstrumentFunction(RTN_INSTRUMENT_CALLBACK fun, VOID *val);

                                                                  /* DO NOT EDIT */
extern PIN_CALLBACK IMG_AddInstrumentFunction(IMAGECALLBACK fun, VOID *v);

                                                                  /* DO NOT EDIT */
extern PIN_CALLBACK IMG_AddUnloadFunction(IMAGECALLBACK fun, VOID *v);

                                                                  /* DO NOT EDIT */
extern USIZE RTN_Range(RTN rtn);

                                                                  /* DO NOT EDIT */
extern USIZE RTN_Size(RTN rtn);

                                                                  /* DO NOT EDIT */
extern RTN RTN_IFuncResolver(RTN rtn);

                                                                  /* DO NOT EDIT */
extern RTN RTN_IFuncImplementation(RTN rtn);

                                                                  /* DO NOT EDIT */
extern IMG IMG_Open(const std::string& filename);

                                                                  /* DO NOT EDIT */
extern VOID IMG_Close(IMG img);

                                                                  /* DO NOT EDIT */
extern std::string RTN_FindNameByAddress(ADDRINT address);

                                                                  /* DO NOT EDIT */
extern RTN RTN_FindByAddress(ADDRINT address);

                                                                  /* DO NOT EDIT */
extern RTN RTN_FindByName(IMG img, const CHAR * name);

                                                                  /* DO NOT EDIT */
extern VOID RTN_Open(RTN rtn);

                                                                  /* DO NOT EDIT */
extern VOID RTN_Close(RTN rtn);

                                                                  /* DO NOT EDIT */
extern INS RTN_InsHead(RTN rtn);

                                                                  /* DO NOT EDIT */
extern INS RTN_InsHeadOnly(RTN rtn);

                                                                  /* DO NOT EDIT */
extern INS RTN_InsTail(RTN rtn);

                                                                  /* DO NOT EDIT */
extern UINT32 RTN_NumIns(RTN rtn);

                                                                  /* DO NOT EDIT */
extern VOID RTN_InsertCall(RTN rtn, IPOINT action, AFUNPTR funptr, ...);

                                                                  /* DO NOT EDIT */
extern ADDRINT RTN_Address(RTN rtn);

                                                                  /* DO NOT EDIT */
extern RTN RTN_CreateAt(ADDRINT address, std::string name);

                                                                  /* DO NOT EDIT */
extern BOOL RTN_IsDynamic(RTN rtn);

                                                                  /* DO NOT EDIT */
extern ADDRINT SEC_Address(SEC sec);

                                                                  /* DO NOT EDIT */
extern BOOL SEC_IsReadable(SEC sec);

                                                                  /* DO NOT EDIT */
extern BOOL SEC_IsWriteable(SEC sec);

                                                                  /* DO NOT EDIT */
extern BOOL SEC_IsExecutable(SEC sec);

                                                                  /* DO NOT EDIT */
extern USIZE SEC_Size(SEC sec);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_Original(BBL bbl);

                                                                  /* DO NOT EDIT */
extern ADDRINT BBL_Address(BBL bbl);

                                                                  /* DO NOT EDIT */
extern USIZE BBL_Size(BBL bbl);

                                                                  /* DO NOT EDIT */
extern IMG APP_ImgHead();

                                                                  /* DO NOT EDIT */
extern IMG APP_ImgTail();

                                                                  /* DO NOT EDIT */
extern std::string PIN_UndecorateSymbolName(const std::string & symbolName, UNDECORATION style);

                                                                  /* DO NOT EDIT */

