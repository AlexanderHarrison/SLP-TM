#ifndef MEX_H_MEX
#define MEX_H_MEX

#include "structs.h"
#include "datatypes.h"
#include "obj.h"

#define DB_FLAG 0

typedef struct MEXInstructionTable
{
    u32 cmd : 8;          // 0x0
    u32 code_offset : 24; // 0x01
    uint reloc_offset;    // 0x4
} MEXInstructionTable;

typedef struct MEXFunctionTable
{
    int index;        // 0x0
    uint code_offset; // 0x4
} MEXFunctionTable;

typedef struct MEXDebugSymbol
{
    uint code_offset; // 0x0
    uint code_length; // 0x4
    char *symbol;     // 0x8
} MEXDebugSymbol;

typedef struct MEXFunctionLookup
{
    int num;
    void *function[30];
} MEXFunctionLookup;

typedef struct MEXFunction
{
    u8 *code;                                     // 0x0
    MEXInstructionTable *instruction_reloc_table; // 0x4
    int instruction_reloc_table_num;              // 0x8
    MEXFunctionTable *func_reloc_table;           // 0xC
    int func_reloc_table_num;                     // 0x10
    int code_size;                                // 0x14
    int debug_symbol_num;                         // 0x18
    MEXDebugSymbol *debug_symbol;                 // 0x1c
} MEXFunction;

#endif
