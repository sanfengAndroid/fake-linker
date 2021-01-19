//
// Created by beich on 2020/12/18.
//

#pragma once

#include <gtype.h>
#include <linker_export.h>


/*
 * @param function_offset 方法对应在JNINativeInterface中的偏移
 * @param hook_method hook替换方法指针
 * @param backup_method 备份原方法指针
 * */
HookJniError HookJniNativeInterface(size_t function_offset, void *hook_method, void **backup_method);

int HookJniNativeInterfaces(HookJniUnit *items, size_t len);