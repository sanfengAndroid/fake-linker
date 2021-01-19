//
// Created by beich on 2020/12/18.
//

#include <jni.h>
#include "hook_jni_native_interface_impl.h"
#include <macros.h>
#include <maps_util.h>

extern "C" JNINativeInterface *original_functions;

HookJniError HookJniNativeInterface(size_t function_offset, void *hook_method, void **backup_method) {
    if (function_offset < offsetof(JNINativeInterface, reserved0) || function_offset > offsetof(JNINativeInterface, GetObjectRefType)) {
        return kHJErrorOffset;
    }
    if (!ALIGN_CHECK(sizeof(void *), function_offset)) {
        return kHJErrorOffset;
    }
    if (hook_method == nullptr) {
        return kHJErrorMethodNull;
    }
    void **target = (void **) (((char *) original_functions) + function_offset);
    if (*target == hook_method) {
        return kHJErrorRepeatOperation;
    }
    MapsUtil util;
    if (!util.GetMemoryProtect(target)) {
        LOGE("The specified memory protection permission is not obtained: %p", target);
        return kHJErrorExec;
    }
    if (!util.UnlockPageProtect()) {
        LOGE("Unlock address protect error: %p", target);
        return kHJErrorExec;
    }
    if (backup_method != nullptr){
        *backup_method = *target;
    }
    *target = hook_method;
    util.RecoveryPageProtect();
    LOGD("replace target method ptr address: %p, old ptr: %p, new ptr: %p", target, *backup_method, hook_method);
    return kHJErrorNO;
}

int HookJniNativeInterfaces(HookJniUnit *items, size_t len) {
    void **target;
    size_t min, max;
    int num = 0;
    HookJniUnit *item = &items[0];

    MapsUtil util;
    if (!util.GetMemoryProtect(original_functions)) {
        LOGE("The specified memory protection permission is not obtained: %p", original_functions);
        return -1;
    }
    min = offsetof(JNINativeInterface, reserved0);
    max = offsetof(JNINativeInterface, GetObjectRefType);
    if (!util.UnlockPageProtect()) {
        LOGE("Unlock address protect error: %p", original_functions);
        return -1;
    }
    while (len-- > 0) {
        if (item->offset < min || item->offset > max || item->hook_method == nullptr) {
            continue;
        }
        // 这里再判断下是否已经Hook,避免存在多次相同调用陷入死循环
        target = reinterpret_cast<void **>((char *) original_functions + item->offset);
        if (*target == item->hook_method) {
            continue;
        }
        if (item->backup_method != nullptr){
            *item->backup_method = *target;
        }
        *target = item->hook_method;
        item++;
        num++;
    }
    util.RecoveryPageProtect();
    return num;
}