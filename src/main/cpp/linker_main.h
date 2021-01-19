//
// Created by beich on 2019/4/12.
//
#pragma once

#include <linker_export.h>

#include <macros.h>

API_PUBLIC void *call_soinfo_function(SoinfoFunType fun_type, SoinfoParamType find_type, const void *find_param, SoinfoParamType param_type, const void *param, int *error_code);

API_PUBLIC void *call_common_function(CommonFunType fun_type, SoinfoParamType find_type, const void *find_param, SoinfoParamType param_type, const void *param, int *error_code);

#if __ANDROID_API__ >= __ANDROID_API_N__
API_PUBLIC void *
call_namespace_function(NamespaceFunType fun_type, NamespaceParamType find_type, const void *find_param, NamespaceParamType param_type, const void *param, int *error_code);
#endif