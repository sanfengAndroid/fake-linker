//
// Created by beich on 2020/12/18.
//

#include "hook_jni_native_interface_impl.h"

#include <jni.h>
#include <macros.h>
#include <maps_util.h>

#include "../linker_globals.h"
#include <jni_helper.h>

extern "C" JNINativeInterface *original_functions;

static int api;

static constexpr uint32_t kAccFastNative = 0x00080000;  // method (runtime; native only)
static constexpr uint32_t kAccNative = 0x0100;  // method

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
    if (backup_method != nullptr) {
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
        if (item->backup_method != nullptr) {
            *item->backup_method = *target;
        }
        *target = item->hook_method;
        item++;
        num++;
    }
    util.RecoveryPageProtect();
    return num;
}

inline static bool IsIndexId(jmethodID mid) {
    return ((reinterpret_cast<uintptr_t>(mid) % 2) != 0);
}


static jfieldID field_art_method = nullptr;
int access_flags_art_method_offset = -1;
static int jni_offset;

static void InitArt(JNIEnv *env) {
#if __ANDROID_API__ >= __ANDROID_API_R__
    if (field_art_method != nullptr) {
        return;
    }
    jclass clazz = env->FindClass("java/lang/reflect/Executable");
    field_art_method = env->GetFieldID(clazz, "artMethod", "J");
    CHECK(field_art_method);
#endif
    api = android_get_device_api_level();
}

static void *GetArtMethod(JNIEnv *env, jclass clazz, jmethodID methodId) {
#if __ANDROID_API__ >= __ANDROID_API_R__
    if (IsIndexId(methodId)){
        jobject method = env->ToReflectedMethod(clazz, methodId, true);
        return reinterpret_cast<void *>(env->GetLongField(method, field_art_method));
    }
#endif
    return methodId;
}

bool InitJniFunctionOffset(JNIEnv *env, jclass clazz, jmethodID methodId, void *native, uint32_t flags) {
    InitArt(env);
    uintptr_t *artMethod = static_cast<uintptr_t *>(GetArtMethod(env, clazz, methodId));
    bool success = false;
    for (int i = 0; i < 30; ++i) {
        if (reinterpret_cast<void *>(artMethod[i]) == native) {
            jni_offset = i;
            success = true;
            LOGD("found art method entrypoint jni offset: %d", i);
            break;
        }
    }
    CHECK(success);
    if (api >= __ANDROID_API_Q__){
        // 非内部隐藏类
        flags |= 0x10000000;
    }
    char *start = reinterpret_cast<char *>(artMethod);
    for (int i = 1; i < 18; ++i) {
        uint32_t value = *(uint32_t *) (start + i * 4);
        if (value == flags) {
            access_flags_art_method_offset = i * 4;
            LOGD("found art method match access flags offset: %d", i * 4);
            success &= true;
            break;
        }
    }
    if (access_flags_art_method_offset < 0){
        if (api >= __ANDROID_API_N__) {
            access_flags_art_method_offset = 4;
        }else if (api == __ANDROID_API_M__){
            access_flags_art_method_offset = 12;
        }else if (api == __ANDROID_API_L_MR1__){
            access_flags_art_method_offset = 20;
        }else if (api == __ANDROID_API_L__){
            access_flags_art_method_offset = 56;
        }
    }
    return success;
}

static void *GetOriginalNativeFunction(const uintptr_t *art_method) {
    if (__predict_false(art_method == nullptr)) {
        return nullptr;
    }
    return (void *) art_method[jni_offset];
}

/*
 * 由于手动修改 jni入口点没有加锁,因此尽量做到每次读写
 * */
inline static uint32_t GetAccessFlags(const char *art_method) {
    return *reinterpret_cast<const uint32_t *>(art_method + access_flags_art_method_offset);
}

inline static bool SetAccessFlags(char *art_method, uint32_t flags) {
    *reinterpret_cast<uint32_t *>(art_method + access_flags_art_method_offset) = flags;
    return true;
}

inline static bool AddAccessFlag(char *art_method, uint32_t flag) {
    uint32_t old_flag = GetAccessFlags(art_method);
    uint32_t new_flag = old_flag | flag;
    return new_flag != old_flag && SetAccessFlags(art_method, new_flag);

}

inline static bool ClearAccessFlag(char *art_method, uint32_t flag) {
    uint32_t old_flag = GetAccessFlags(art_method);
    uint32_t new_flag = old_flag & ~flag;
    return new_flag != old_flag && SetAccessFlags(art_method, new_flag);
}

inline static bool HasAccessFlag(char *art_method, uint32_t flag) {
    uint32_t flags = GetAccessFlags(art_method);
    return (flags & flag) == flag;
}

inline static bool ClearFastNativeFlag(char *art_method) {
    // Android 9.0以上没有判断 FastNative 标志
    return api < __ANDROID_API_P__ && ClearAccessFlag(art_method, kAccFastNative);

}

int RegisterNativeAgain(JNIEnv *env, jclass clazz, HookRegisterNativeUnit *items, size_t len) {
    if (clazz == nullptr || items == nullptr || len < 1) {
        LOGE("Registration class or method cannot be empty");
        return -1;
    }
    int success = 0;
    JNINativeMethod methods[1];
    for (int i = 0; i < len; ++i) {
        JNINativeMethod hook = items[i].hook_method;
        const char *sign = hook.signature;
        if (sign[0] == '!') {
            sign++;
        }
        jmethodID methodId = items[i].is_static ? env->GetStaticMethodID(clazz, hook.name, sign) : env->GetMethodID(clazz, hook.name, sign);
        if (methodId == nullptr) {
            LOGE("Find method failed, name: %s, signature: %s, is static: %d", hook.name, hook.signature, items[i].is_static);
            JNIHelper::PrintAndClearException(env);
            continue;
        }
        void *artMethod = GetArtMethod(env, clazz, methodId);
        void *backup = GetOriginalNativeFunction(static_cast<uintptr_t *>(artMethod));
        if (backup == hook.fnPtr) {
            LOGE("The same native method has been registered, name: %s, signature: %s, address: %p, is static: %d",
                 hook.name, hook.signature, hook.fnPtr, items[i].is_static);
            continue;
        }
        if (items[i].backup_method != nullptr) {
            *(items[i].backup_method) = backup;
        }
        if (!HasAccessFlag(reinterpret_cast<char *>(artMethod), kAccNative)) {
            LOGE("You are hooking a non-native method, name: %s, signature: %s, is static: %d", hook.name, hook.signature, items[i].is_static);
            continue;
        }
        bool restore = ClearFastNativeFlag(reinterpret_cast<char *>(artMethod));
        if (api >= __ANDROID_API_O__) {
            hook.signature = sign;
        }
        methods[0] = hook;
        if (env->RegisterNatives(clazz, methods, 1) == JNI_OK) {
            success++;
            /*
             * Android 8.0 ,8.1 必须清除 FastNative 标志才能注册成功,所以如果原来包含 FastNative 标志还得恢复,
             * 否者调用原方法可能会出现问题
             * */
            if (restore && (api == __ANDROID_API_O__ || api == __ANDROID_API_O_MR1__)) {
                AddAccessFlag(reinterpret_cast<char *>(artMethod), kAccFastNative);
            }
        } else {
            LOGE("register native function failed, method name: %s, sign: %s, is static: %d", hook.name, hook.signature, items[i].is_static);
            JNIHelper::PrintAndClearException(env);
            if (restore) {
                AddAccessFlag(reinterpret_cast<char *>(artMethod), kAccFastNative);
            }
            if (items[i].backup_method != nullptr) {
                *(items[i].backup_method) = nullptr;
            }
        }
    }
    return success;
}