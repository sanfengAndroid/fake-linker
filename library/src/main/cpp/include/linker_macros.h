//
// Created by beich on 2022/5/3.
//
#ifndef FAKE_LINKER_LINKER_MACROS_H
#define FAKE_LINKER_LINKER_MACROS_H

#define ANDROID_GE_M
#define ANDROID_GE_L1
#define ANDROID_GE_N
#define ANDROID_GE_O
#define ANDROID_GE_P
#define ANDROID_GE_Q
#define ANDROID_GE_R
#define ANDROID_GE_S
#define ANDROID_GE_T
#define ANDROID_GE_U
#define ANDROID_GE_V
#define ANDROID_LE_M
#define ANDROID_LE_L1
#define ANDROID_LE_O1
#define ANDROID_LE_S
#define ANDROID_LE_U
#define ANDROID_LE_V

#define MEMORY_FREE
#define ONLY_READ

#if defined(__cplusplus)
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS   }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

#define API_LOCAL                     __attribute__((visibility("hidden")))
#define API_PUBLIC                    __attribute__((visibility("default")))

#define C_API                         extern "C"

#define strong_alias(name, aliasname) __strong_alias(name, aliasname)
#define weak_alias(name, aliasname)   _weak_alias(name, aliasname)
#define _weak_alias(name, aliasname)  extern __typeof(name) aliasname __attribute__((weak, alias(#name)));

#endif // FAKE_LINKER_LINKER_MACROS_H