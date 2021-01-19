//
// Created by lenovo-s on 2019/4/15.
//
#pragma once


#include <vector>

#include <gtype.h>
#include <macros.h>
#include <variable_length_object.h>
#include "local_block_allocator.h"



typedef enum {
	kImportOrExport = 0,
	kImported,
	kExported,
	kInner
} SymbolType;

typedef VarLengthObject<gaddress> symbols_address;

/*
 * 查找内部符号必须要求原文件有完整的符号表,linker通常是有完整符号表,
 * 但是其它库和可执行文件通常都没有完整符号表
 * */
gaddress ResolveLibrarySymbolAddress(const char *library_name, SymbolType type, const char *symbol_name);

symbols_address *ResolveLibrarySymbolsAddress(const char *library_name, SymbolType type, gsize len, ...);
