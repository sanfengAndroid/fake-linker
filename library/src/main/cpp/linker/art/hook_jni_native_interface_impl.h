//
// Created by beich on 2020/12/18.
//

#pragma once
#include <jni.h>

#include <fake_linker.h>

namespace fakelinker {
/**
 * @brief Hook Java JNI Native Interface
 *
 * @param  function_offset    Methods correspond to offsets in JNINativeInterface
 * @param  hook_method        Hooked function address
 * @param  backup_method      Backup function out address
 * @return FakeLinkerError    Return hook error code
 */
FakeLinkerError HookJniNativeInterface(int function_offset, void *hook_method, void **backup_method);

int HookJniNativeInterfaces(HookJniUnit *items, int len);

int HookJavaNativeFunctions(JNIEnv *env, jclass clazz, HookRegisterNativeUnit *items, size_t len);

bool InitJniFunctionOffset(JNIEnv *env, jclass clazz, jmethodID methodId, void *native, uint32_t flags,
                           uint32_t unmask);

bool DefaultInitJniFunctionOffset(JNIEnv *env);

/**
 * @brief Read the native function pointer directly from the ArtMethod structure
 *
 * @param  env        JNIEnv pointer, find ArtMethod use
 * @param  clazz      The class where the method being searched is located
 * @param  methodId   The method id being searched
 * @return void*      Return -1 if the native pointer offset initialization fails,
 *                    -2 if the art method is not found,
 *                    other values ​​are function pointers
 */
void *ReadNativeFunction(JNIEnv *env, jclass clazz, jmethodID methodId);

/* ArtMethod* */ void *ToArtMethod(JNIEnv *env, jmethodID methodId, jclass clazz);
} // namespace fakelinker
