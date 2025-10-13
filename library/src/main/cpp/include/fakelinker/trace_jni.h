//
// Created by beichen on 2025/4/25.
//
#pragma once

#include <cinttypes>
#include <jni.h>

#include <map>
#include <string_view>
#include <type_traits>
#include <vector>

#include "fake_linker.h"
#include "jni_helper.h"
#include "macros.h"
#include "proxy_jni.h"
#include "symbol_resolver.h"
#include "type.h"


namespace fakelinker {

template <typename T, typename = void>
struct _is_valid_container : std::false_type {};

template <typename T>
struct _is_valid_container<
  T, std::void_t<decltype(std::declval<T>().begin()), decltype(std::declval<T>().end()), typename T::value_type>>
    : std::is_same<typename T::value_type, size_t> {};


/**
 * @struct TraceInvokeContext
 * @brief Context structure for tracing JNI method invocations.
 *
 * This structure holds information relevant to a JNI method invocation trace,
 * including the JNI environment, caller address, result type, method ID (for
 * varargs), and a message buffer for logging or debugging purposes.
 *
 * Members:
 * - env: Pointer to the JNI environment associated with the current thread.
 * - caller: Address of the entity invoking the JNI method.
 * - result: The result type of the JNI call, if applicable.
 * - method: The JNI method ID, used when the method has variable arguments.
 * - message: Buffer to store trace or debug messages (up to 4096 characters).
 * - message_length: The current length of the message stored in the buffer.
 *
 * Constructor:
 * - Initializes the context with the given result type, JNI environment, and caller address.
 *   The method ID is initialized to nullptr.
 */
template <typename jtype, bool AllowAccessArgs = true>
struct TraceInvokeContext {
  // JNIEnv object pointer, note: it contains function pointers after our Hook
  JNIEnv *env;
  // Caller's address
  void *caller;
  // JNI call return value
  jtype result;
  // Specific method when there are variable arguments va_list
  jmethodID method;
  // Store messages
  char message[4096];
  uint32_t message_length = 0;

  TraceInvokeContext(jtype result, JNIEnv *env, void *caller) :
      env(env), caller(caller), result(result), method(nullptr) {}
};

template <>
struct TraceInvokeContext<void, true> {
  // JNIEnv object pointer, note: it contains function pointers after our Hook
  JNIEnv *env;
  // Caller's address
  void *caller;
  jmethodID method;
  char message[4096];
  uint32_t message_length = 0;

  TraceInvokeContext(JNIEnv *env, void *caller) : env(env), caller(caller), method(nullptr) {}
};

class GuardJNIEnv {
public:
  GuardJNIEnv(JNIEnv *env, JNIEnv *original_env) : env_(env) {
    backup_ = env_->functions;
    env->functions = original_env->functions;
  }

  ~GuardJNIEnv() { env_->functions = backup_; }

  JNIEnv *env_;
  const JNINativeInterface *backup_;
};

template <typename Derived>
class JNIMonitor;

template <typename Derived>
class BaseTraceJNICallback;

namespace jni_trace {
// Directly declare global variables to avoid adding addressing operations
extern JNINativeInterface backup_jni;
extern JNINativeInterface *org_jni;
extern JNINativeInterface hook_jni;
extern void *monitor;
extern void *callback;
extern JNIEnv org_env;
} // namespace jni_trace

struct ScopedVAArgs {
  explicit ScopedVAArgs(va_list *args) : args(args) {}

  ScopedVAArgs(const ScopedVAArgs &) = delete;

  ScopedVAArgs(ScopedVAArgs &&) = delete;

  ~ScopedVAArgs() { va_end(*args); }

private:
  va_list *args;
};

#define MONITOR_CALL(name, ...)                                                                                        \
  auto result = jni_trace::org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__);                             \
  if (reinterpret_cast<JNIMonitor<Derived> *>(jni_trace::monitor)->IsMonitoring(__builtin_return_address(0))) {        \
    TraceInvokeContext<return_type_object_trait_t<decltype(&JNIEnv::name)>> context(                                   \
      result, reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                                          \
    reinterpret_cast<Derived *>(jni_trace::callback)->name(context, ##__VA_ARGS__);                                    \
    return context.result;                                                                                             \
  }                                                                                                                    \
  return result;

#define MONITOR_CALL_VA_LIST(name, methodID, args, ...)                                                                \
  va_list args_copy;                                                                                                   \
  va_copy(args_copy, args);                                                                                            \
  ScopedVAArgs scoped_args(&args_copy);                                                                                \
  auto result = jni_trace::org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__, args);                       \
  if (reinterpret_cast<JNIMonitor<Derived> *>(jni_trace::monitor)->IsMonitoring(__builtin_return_address(0))) {        \
    TraceInvokeContext<return_type_object_trait_t<decltype(&JNIEnv::name)>> context(                                   \
      result, reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                                          \
    context.method = methodID;                                                                                         \
    reinterpret_cast<Derived *>(jni_trace::callback)->name(context, ##__VA_ARGS__, args_copy);                         \
    return context.result;                                                                                             \
  }                                                                                                                    \
  return result;

#define MONITOR_CALL_INVOKE(name, methodID, ...)                                                                       \
  auto result = jni_trace::org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__);                             \
  if (reinterpret_cast<JNIMonitor<Derived> *>(jni_trace::monitor)->IsMonitoring(__builtin_return_address(0))) {        \
    TraceInvokeContext<return_type_object_trait_t<decltype(&JNIEnv::name)>> context(                                   \
      result, reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                                          \
    context.method = methodID;                                                                                         \
    reinterpret_cast<Derived *>(jni_trace::callback)->name(context, ##__VA_ARGS__);                                    \
    return context.result;                                                                                             \
  }                                                                                                                    \
  return result;


#define MONITOR_CALL_INVALID_ARGS(name, ...)                                                                           \
  auto result = jni_trace::org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__);                             \
  if (reinterpret_cast<JNIMonitor<Derived> *>(jni_trace::monitor)->IsMonitoring(__builtin_return_address(0))) {        \
    TraceInvokeContext<return_type_object_trait_t<decltype(&JNIEnv::name)>, false> context(                            \
      result, reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                                          \
    reinterpret_cast<Derived *>(jni_trace::callback)->name(context, ##__VA_ARGS__);                                    \
    return context.result;                                                                                             \
  }                                                                                                                    \
  return result;


#define MONITOR_VOID_CALL(name, ...)                                                                                   \
  jni_trace::org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__);                                           \
  if (reinterpret_cast<JNIMonitor<Derived> *>(jni_trace::monitor)->IsMonitoring(__builtin_return_address(0))) {        \
    TraceInvokeContext<void> context(reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                   \
    reinterpret_cast<Derived *>(jni_trace::callback)->name(context, ##__VA_ARGS__);                                    \
  }

#define MONITOR_VOID_CALL_INVOKE(name, methodID, ...)                                                                  \
  jni_trace::org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__);                                           \
  if (reinterpret_cast<JNIMonitor<Derived> *>(jni_trace::monitor)->IsMonitoring(__builtin_return_address(0))) {        \
    TraceInvokeContext<void> context(reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                   \
    context.method = methodID;                                                                                         \
    reinterpret_cast<Derived *>(jni_trace::callback)->name(context, ##__VA_ARGS__);                                    \
  }

#define MONITOR_VOID_CALL_VA_LIST(name, methodID, args, ...)                                                           \
  va_list args_copy;                                                                                                   \
  va_copy(args_copy, args);                                                                                            \
  ScopedVAArgs scoped_args(&args_copy);                                                                                \
  jni_trace::org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__, args);                                     \
  if (reinterpret_cast<JNIMonitor<Derived> *>(jni_trace::monitor)->IsMonitoring(__builtin_return_address(0))) {        \
    TraceInvokeContext<void> context(reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                   \
    context.method = methodID;                                                                                         \
    reinterpret_cast<Derived *>(jni_trace::callback)->name(context, ##__VA_ARGS__, args_copy);                         \
  }

#define MONITOR_VOID_CALL_AFTER(name, ...)                                                                             \
  if (reinterpret_cast<JNIMonitor<Derived> *>(jni_trace::monitor)->IsMonitoring(__builtin_return_address(0))) {        \
    TraceInvokeContext<void> context(reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                   \
    reinterpret_cast<Derived *>(jni_trace::callback)->name(context, ##__VA_ARGS__);                                    \
  }                                                                                                                    \
  jni_trace::org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__);


template <typename From, typename To>
union FuncPtrConverter {
  From src;
  To dest;
};

template <typename To, typename From>
constexpr To ForceConvertFuncPtr(From f) {
  FuncPtrConverter<From, To> converter{.src = f};
  return converter.dest;
}

template <typename Derived>
class HookJNIEnv {
public:
  void BindMethod() {
#define BIND_JNI_FUN(name)                                                                                             \
  jni_trace::hook_jni.name = ForceConvertFuncPtr<decltype(JNINativeInterface::name)>(&HookJNIEnv::name)
    // region bind jni function
    {
      BIND_JNI_FUN(GetVersion);
      BIND_JNI_FUN(DefineClass);
      BIND_JNI_FUN(FindClass);
      BIND_JNI_FUN(FromReflectedMethod);
      BIND_JNI_FUN(FromReflectedField);
      BIND_JNI_FUN(ToReflectedMethod);
      BIND_JNI_FUN(GetSuperclass);
      BIND_JNI_FUN(IsAssignableFrom);
      BIND_JNI_FUN(ToReflectedField);
      BIND_JNI_FUN(Throw);
      BIND_JNI_FUN(ThrowNew);
      BIND_JNI_FUN(ExceptionOccurred);
      BIND_JNI_FUN(ExceptionDescribe);
      BIND_JNI_FUN(ExceptionClear);
      BIND_JNI_FUN(FatalError);
      BIND_JNI_FUN(PushLocalFrame);
      BIND_JNI_FUN(PopLocalFrame);
      BIND_JNI_FUN(NewGlobalRef);
      BIND_JNI_FUN(DeleteGlobalRef);
      BIND_JNI_FUN(DeleteLocalRef);
      BIND_JNI_FUN(IsSameObject);
      BIND_JNI_FUN(NewLocalRef);
      BIND_JNI_FUN(EnsureLocalCapacity);
      BIND_JNI_FUN(AllocObject);
      BIND_JNI_FUN(NewObject);
      BIND_JNI_FUN(NewObjectV);
      BIND_JNI_FUN(NewObjectA);
      BIND_JNI_FUN(GetObjectClass);
      BIND_JNI_FUN(IsInstanceOf);
      BIND_JNI_FUN(GetMethodID);
      BIND_JNI_FUN(CallObjectMethod);
      BIND_JNI_FUN(CallObjectMethodV);
      BIND_JNI_FUN(CallObjectMethodA);
      BIND_JNI_FUN(CallBooleanMethod);
      BIND_JNI_FUN(CallBooleanMethodV);
      BIND_JNI_FUN(CallBooleanMethodA);
      BIND_JNI_FUN(CallByteMethod);
      BIND_JNI_FUN(CallByteMethodV);
      BIND_JNI_FUN(CallByteMethodA);
      BIND_JNI_FUN(CallCharMethod);
      BIND_JNI_FUN(CallCharMethodV);
      BIND_JNI_FUN(CallCharMethodA);
      BIND_JNI_FUN(CallShortMethod);
      BIND_JNI_FUN(CallShortMethodV);
      BIND_JNI_FUN(CallShortMethodA);
      BIND_JNI_FUN(CallIntMethod);
      BIND_JNI_FUN(CallIntMethodV);
      BIND_JNI_FUN(CallIntMethodA);
      BIND_JNI_FUN(CallLongMethod);
      BIND_JNI_FUN(CallLongMethodV);
      BIND_JNI_FUN(CallLongMethodA);
      BIND_JNI_FUN(CallFloatMethod);
      BIND_JNI_FUN(CallFloatMethodV);
      BIND_JNI_FUN(CallFloatMethodA);
      BIND_JNI_FUN(CallDoubleMethod);
      BIND_JNI_FUN(CallDoubleMethodV);
      BIND_JNI_FUN(CallDoubleMethodA);
      BIND_JNI_FUN(CallVoidMethod);
      BIND_JNI_FUN(CallVoidMethodV);
      BIND_JNI_FUN(CallVoidMethodA);
      BIND_JNI_FUN(CallNonvirtualObjectMethod);
      BIND_JNI_FUN(CallNonvirtualObjectMethodV);
      BIND_JNI_FUN(CallNonvirtualObjectMethodA);
      BIND_JNI_FUN(CallNonvirtualBooleanMethod);
      BIND_JNI_FUN(CallNonvirtualBooleanMethodV);
      BIND_JNI_FUN(CallNonvirtualBooleanMethodA);
      BIND_JNI_FUN(CallNonvirtualByteMethod);
      BIND_JNI_FUN(CallNonvirtualByteMethodV);
      BIND_JNI_FUN(CallNonvirtualByteMethodA);
      BIND_JNI_FUN(CallNonvirtualCharMethod);
      BIND_JNI_FUN(CallNonvirtualCharMethodV);
      BIND_JNI_FUN(CallNonvirtualCharMethodA);
      BIND_JNI_FUN(CallNonvirtualShortMethod);
      BIND_JNI_FUN(CallNonvirtualShortMethodV);
      BIND_JNI_FUN(CallNonvirtualShortMethodA);
      BIND_JNI_FUN(CallNonvirtualIntMethod);
      BIND_JNI_FUN(CallNonvirtualIntMethodV);
      BIND_JNI_FUN(CallNonvirtualIntMethodA);
      BIND_JNI_FUN(CallNonvirtualLongMethod);
      BIND_JNI_FUN(CallNonvirtualLongMethodV);
      BIND_JNI_FUN(CallNonvirtualLongMethodA);
      BIND_JNI_FUN(CallNonvirtualFloatMethod);
      BIND_JNI_FUN(CallNonvirtualFloatMethodV);
      BIND_JNI_FUN(CallNonvirtualFloatMethodA);
      BIND_JNI_FUN(CallNonvirtualDoubleMethod);
      BIND_JNI_FUN(CallNonvirtualDoubleMethodV);
      BIND_JNI_FUN(CallNonvirtualDoubleMethodA);
      BIND_JNI_FUN(CallNonvirtualVoidMethod);
      BIND_JNI_FUN(CallNonvirtualVoidMethodV);
      BIND_JNI_FUN(CallNonvirtualVoidMethodA);
      BIND_JNI_FUN(GetFieldID);
      BIND_JNI_FUN(GetObjectField);
      BIND_JNI_FUN(GetBooleanField);
      BIND_JNI_FUN(GetByteField);
      BIND_JNI_FUN(GetCharField);
      BIND_JNI_FUN(GetShortField);
      BIND_JNI_FUN(GetIntField);
      BIND_JNI_FUN(GetLongField);
      BIND_JNI_FUN(GetFloatField);
      BIND_JNI_FUN(GetDoubleField);
      BIND_JNI_FUN(SetObjectField);
      BIND_JNI_FUN(SetBooleanField);
      BIND_JNI_FUN(SetByteField);
      BIND_JNI_FUN(SetCharField);
      BIND_JNI_FUN(SetShortField);
      BIND_JNI_FUN(SetIntField);
      BIND_JNI_FUN(SetLongField);
      BIND_JNI_FUN(SetFloatField);
      BIND_JNI_FUN(SetDoubleField);
      BIND_JNI_FUN(GetStaticMethodID);
      BIND_JNI_FUN(CallStaticObjectMethod);
      BIND_JNI_FUN(CallStaticObjectMethodV);
      BIND_JNI_FUN(CallStaticObjectMethodA);
      BIND_JNI_FUN(CallStaticBooleanMethod);
      BIND_JNI_FUN(CallStaticBooleanMethodV);
      BIND_JNI_FUN(CallStaticBooleanMethodA);
      BIND_JNI_FUN(CallStaticByteMethod);
      BIND_JNI_FUN(CallStaticByteMethodV);
      BIND_JNI_FUN(CallStaticByteMethodA);
      BIND_JNI_FUN(CallStaticCharMethod);
      BIND_JNI_FUN(CallStaticCharMethodV);
      BIND_JNI_FUN(CallStaticCharMethodA);
      BIND_JNI_FUN(CallStaticShortMethod);
      BIND_JNI_FUN(CallStaticShortMethodV);
      BIND_JNI_FUN(CallStaticShortMethodA);
      BIND_JNI_FUN(CallStaticIntMethod);
      BIND_JNI_FUN(CallStaticIntMethodV);
      BIND_JNI_FUN(CallStaticIntMethodA);
      BIND_JNI_FUN(CallStaticLongMethod);
      BIND_JNI_FUN(CallStaticLongMethodV);
      BIND_JNI_FUN(CallStaticLongMethodA);
      BIND_JNI_FUN(CallStaticFloatMethod);
      BIND_JNI_FUN(CallStaticFloatMethodV);
      BIND_JNI_FUN(CallStaticFloatMethodA);
      BIND_JNI_FUN(CallStaticDoubleMethod);
      BIND_JNI_FUN(CallStaticDoubleMethodV);
      BIND_JNI_FUN(CallStaticDoubleMethodA);
      BIND_JNI_FUN(CallStaticVoidMethod);
      BIND_JNI_FUN(CallStaticVoidMethodV);
      BIND_JNI_FUN(CallStaticVoidMethodA);
      BIND_JNI_FUN(GetStaticFieldID);
      BIND_JNI_FUN(GetStaticObjectField);
      BIND_JNI_FUN(GetStaticBooleanField);
      BIND_JNI_FUN(GetStaticByteField);
      BIND_JNI_FUN(GetStaticCharField);
      BIND_JNI_FUN(GetStaticShortField);
      BIND_JNI_FUN(GetStaticIntField);
      BIND_JNI_FUN(GetStaticLongField);
      BIND_JNI_FUN(GetStaticFloatField);
      BIND_JNI_FUN(GetStaticDoubleField);
      BIND_JNI_FUN(SetStaticObjectField);
      BIND_JNI_FUN(SetStaticBooleanField);
      BIND_JNI_FUN(SetStaticByteField);
      BIND_JNI_FUN(SetStaticCharField);
      BIND_JNI_FUN(SetStaticShortField);
      BIND_JNI_FUN(SetStaticIntField);
      BIND_JNI_FUN(SetStaticLongField);
      BIND_JNI_FUN(SetStaticFloatField);
      BIND_JNI_FUN(SetStaticDoubleField);
      BIND_JNI_FUN(NewString);
      BIND_JNI_FUN(GetStringLength);
      BIND_JNI_FUN(GetStringChars);
      BIND_JNI_FUN(ReleaseStringChars);
      BIND_JNI_FUN(NewStringUTF);
      BIND_JNI_FUN(GetStringUTFLength);
      BIND_JNI_FUN(GetStringUTFChars);
      BIND_JNI_FUN(ReleaseStringUTFChars);
      BIND_JNI_FUN(GetArrayLength);
      BIND_JNI_FUN(NewObjectArray);
      BIND_JNI_FUN(GetObjectArrayElement);
      BIND_JNI_FUN(SetObjectArrayElement);
      BIND_JNI_FUN(NewBooleanArray);
      BIND_JNI_FUN(NewByteArray);
      BIND_JNI_FUN(NewCharArray);
      BIND_JNI_FUN(NewShortArray);
      BIND_JNI_FUN(NewIntArray);
      BIND_JNI_FUN(NewLongArray);
      BIND_JNI_FUN(NewFloatArray);
      BIND_JNI_FUN(NewDoubleArray);
      BIND_JNI_FUN(GetBooleanArrayElements);
      BIND_JNI_FUN(GetByteArrayElements);
      BIND_JNI_FUN(GetCharArrayElements);
      BIND_JNI_FUN(GetShortArrayElements);
      BIND_JNI_FUN(GetIntArrayElements);
      BIND_JNI_FUN(GetLongArrayElements);
      BIND_JNI_FUN(GetFloatArrayElements);
      BIND_JNI_FUN(GetDoubleArrayElements);
      BIND_JNI_FUN(ReleaseBooleanArrayElements);
      BIND_JNI_FUN(ReleaseByteArrayElements);
      BIND_JNI_FUN(ReleaseCharArrayElements);
      BIND_JNI_FUN(ReleaseShortArrayElements);
      BIND_JNI_FUN(ReleaseIntArrayElements);
      BIND_JNI_FUN(ReleaseLongArrayElements);
      BIND_JNI_FUN(ReleaseFloatArrayElements);
      BIND_JNI_FUN(ReleaseDoubleArrayElements);
      BIND_JNI_FUN(GetBooleanArrayRegion);
      BIND_JNI_FUN(GetByteArrayRegion);
      BIND_JNI_FUN(GetCharArrayRegion);
      BIND_JNI_FUN(GetShortArrayRegion);
      BIND_JNI_FUN(GetIntArrayRegion);
      BIND_JNI_FUN(GetLongArrayRegion);
      BIND_JNI_FUN(GetFloatArrayRegion);
      BIND_JNI_FUN(GetDoubleArrayRegion);
      BIND_JNI_FUN(SetBooleanArrayRegion);
      BIND_JNI_FUN(SetByteArrayRegion);
      BIND_JNI_FUN(SetCharArrayRegion);
      BIND_JNI_FUN(SetShortArrayRegion);
      BIND_JNI_FUN(SetIntArrayRegion);
      BIND_JNI_FUN(SetLongArrayRegion);
      BIND_JNI_FUN(SetFloatArrayRegion);
      BIND_JNI_FUN(SetDoubleArrayRegion);
      BIND_JNI_FUN(RegisterNatives);
      BIND_JNI_FUN(UnregisterNatives);
      BIND_JNI_FUN(MonitorEnter);
      BIND_JNI_FUN(MonitorExit);
      BIND_JNI_FUN(GetJavaVM);
      BIND_JNI_FUN(GetStringRegion);
      BIND_JNI_FUN(GetStringUTFRegion);
      BIND_JNI_FUN(GetPrimitiveArrayCritical);
      BIND_JNI_FUN(ReleasePrimitiveArrayCritical);
      BIND_JNI_FUN(GetStringCritical);
      BIND_JNI_FUN(ReleaseStringCritical);
      BIND_JNI_FUN(NewWeakGlobalRef);
      BIND_JNI_FUN(DeleteWeakGlobalRef);
      BIND_JNI_FUN(ExceptionCheck);
      BIND_JNI_FUN(NewDirectByteBuffer);
      BIND_JNI_FUN(GetDirectBufferAddress);
      BIND_JNI_FUN(GetDirectBufferCapacity);
      BIND_JNI_FUN(GetObjectRefType);
    }
    // endregion
#undef BIND_JNI_FUN
  }

protected:
  HookJNIEnv() = default;

private:
  // JNI Proxy
  jint GetVersion() { MONITOR_CALL(GetVersion); }

  jclass DefineClass(const char *name, jobject loader, const jbyte *buf, jsize bufLen) {
    MONITOR_CALL(DefineClass, name, loader, buf, bufLen);
  }

  jclass FindClass(const char *name) { MONITOR_CALL(FindClass, name); }

  jmethodID FromReflectedMethod(jobject method) { MONITOR_CALL(FromReflectedMethod, method); }

  jfieldID FromReflectedField(jobject field) { MONITOR_CALL(FromReflectedField, field); }

  jobject ToReflectedMethod(jclass cls, jmethodID methodID, jboolean isStatic) {
    MONITOR_CALL(ToReflectedMethod, cls, methodID, isStatic);
  }

  jclass GetSuperclass(jclass clazz) { MONITOR_CALL(GetSuperclass, clazz); }

  jboolean IsAssignableFrom(jclass from_clazz, jclass to_clazz) {
    MONITOR_CALL(IsAssignableFrom, from_clazz, to_clazz);
  }

  jobject ToReflectedField(jclass cls, jfieldID fieldID, jboolean isStatic) {
    MONITOR_CALL(ToReflectedField, cls, fieldID, isStatic);
  }

  jint Throw(jthrowable obj) { MONITOR_CALL(Throw, obj); }

  jint ThrowNew(jclass clazz, const char *message) { MONITOR_CALL(ThrowNew, clazz, message); }

  jthrowable ExceptionOccurred() { MONITOR_CALL(ExceptionOccurred); }

  void ExceptionDescribe() { MONITOR_VOID_CALL(ExceptionDescribe); }

  void ExceptionClear() { MONITOR_VOID_CALL(ExceptionClear); }

  void FatalError(const char *msg) { MONITOR_VOID_CALL(FatalError, msg); }

  jint PushLocalFrame(jint capacity) { MONITOR_CALL(PushLocalFrame, capacity); }

  jobject PopLocalFrame(jobject local) { MONITOR_CALL_INVALID_ARGS(PopLocalFrame, local); }

  jobject NewGlobalRef(jobject obj) { MONITOR_CALL(NewGlobalRef, obj); }

  void DeleteGlobalRef(jobject globalRef) { MONITOR_VOID_CALL_AFTER(DeleteGlobalRef, globalRef); }

  void DeleteLocalRef(jobject localRef) { MONITOR_VOID_CALL_AFTER(DeleteLocalRef, localRef); }

  jboolean IsSameObject(jobject ref1, jobject ref2) { MONITOR_CALL(IsSameObject, ref1, ref2); }

  jobject NewLocalRef(jobject ref) { MONITOR_CALL(NewLocalRef, ref); }

  jint EnsureLocalCapacity(jint capacity) { MONITOR_CALL(EnsureLocalCapacity, capacity); }

  jobject AllocObject(jclass clazz) { MONITOR_CALL(AllocObject, clazz); }

  jobject NewObject(jclass clazz, jmethodID methodID, ...) {
    va_list args;
    va_start(args, methodID);
    ScopedVAArgs scoped_args(&args);
    MONITOR_CALL(NewObjectV, clazz, methodID, args);
  }

  jobject NewObjectV(jclass clazz, jmethodID methodID, va_list args) {
    MONITOR_CALL_VA_LIST(NewObjectV, methodID, args, clazz, methodID);
  }

  jobject NewObjectA(jclass clazz, jmethodID methodID, const jvalue *args) {
    MONITOR_CALL_INVOKE(NewObjectA, methodID, clazz, methodID, args);
  }

  jclass GetObjectClass(jobject obj) { MONITOR_CALL(GetObjectClass, obj); }

  jboolean IsInstanceOf(jobject obj, jclass clazz) { MONITOR_CALL(IsInstanceOf, obj, clazz); }

  jmethodID GetMethodID(jclass clazz, const char *name, const char *sig) {
    MONITOR_CALL(GetMethodID, clazz, name, sig);
  }

#define PROXY_CALL_TYPE_METHOD(_jtype, _jname)                                                                         \
  _jtype Call##_jname##Method(jobject obj, jmethodID methodID, ...) {                                                  \
    va_list args;                                                                                                      \
    va_start(args, methodID);                                                                                          \
    ScopedVAArgs scoped_args(&args);                                                                                   \
    MONITOR_CALL(Call##_jname##MethodV, obj, methodID, args)                                                           \
  }

#define PROXY_CALL_TYPE_METHODV(_jtype, _jname)                                                                        \
  _jtype Call##_jname##MethodV(jobject obj, jmethodID methodID, va_list args) {                                        \
    MONITOR_CALL_VA_LIST(Call##_jname##MethodV, methodID, args, obj, methodID);                                        \
  }

#define PROXY_CALL_TYPE_METHODA(_jtype, _jname)                                                                        \
  _jtype Call##_jname##MethodA(jobject obj, jmethodID methodID, const jvalue *args) {                                  \
    MONITOR_CALL_INVOKE(Call##_jname##MethodA, methodID, obj, methodID, args);                                         \
  }

#define PROXY_CALL_TYPE(_jtype, _jname)                                                                                \
  PROXY_CALL_TYPE_METHOD(_jtype, _jname)                                                                               \
  PROXY_CALL_TYPE_METHODV(_jtype, _jname)                                                                              \
  PROXY_CALL_TYPE_METHODA(_jtype, _jname)

  PROXY_CALL_TYPE(jobject, Object)

  PROXY_CALL_TYPE(jboolean, Boolean)

  PROXY_CALL_TYPE(jbyte, Byte)

  PROXY_CALL_TYPE(jchar, Char)

  PROXY_CALL_TYPE(jshort, Short)

  PROXY_CALL_TYPE(jint, Int)

  PROXY_CALL_TYPE(jlong, Long)

  PROXY_CALL_TYPE(jfloat, Float)

  PROXY_CALL_TYPE(jdouble, Double)

  void CallVoidMethod(jobject obj, jmethodID methodID, ...) {
    va_list args;
    va_start(args, methodID);
    ScopedVAArgs scoped_args(&args);
    MONITOR_VOID_CALL(CallVoidMethodV, obj, methodID, args);
  }

  void CallVoidMethodV(jobject obj, jmethodID methodID, va_list args) {
    MONITOR_VOID_CALL_VA_LIST(CallVoidMethodV, methodID, args, obj, methodID);
  }

  void CallVoidMethodA(jobject obj, jmethodID methodID, const jvalue *args) {
    MONITOR_VOID_CALL_INVOKE(CallVoidMethodA, methodID, obj, methodID, args);
  }

#define PROXY_CALL_NONVIRT_TYPE_METHOD(_jtype, _jname)                                                                 \
  _jtype CallNonvirtual##_jname##Method(jobject obj, jclass clazz, jmethodID methodID, ...) {                          \
    va_list args;                                                                                                      \
    va_start(args, methodID);                                                                                          \
    ScopedVAArgs scoped_args(&args);                                                                                   \
    MONITOR_CALL(CallNonvirtual##_jname##MethodV, obj, clazz, methodID, args)                                          \
  }
#define PROXY_CALL_NONVIRT_TYPE_METHODV(_jtype, _jname)                                                                \
  _jtype CallNonvirtual##_jname##MethodV(jobject obj, jclass clazz, jmethodID methodID, va_list args) {                \
    MONITOR_CALL_VA_LIST(CallNonvirtual##_jname##MethodV, methodID, args, obj, clazz, methodID);                       \
  }

#define PROXY_CALL_NONVIRT_TYPE_METHODA(_jtype, _jname)                                                                \
  _jtype CallNonvirtual##_jname##MethodA(jobject obj, jclass clazz, jmethodID methodID, const jvalue *args) {          \
    MONITOR_CALL_INVOKE(CallNonvirtual##_jname##MethodA, methodID, obj, clazz, methodID, args);                        \
  }

#define PROXY_CALL_NONVIRT_TYPE(_jtype, _jname)                                                                        \
  PROXY_CALL_NONVIRT_TYPE_METHOD(_jtype, _jname)                                                                       \
  PROXY_CALL_NONVIRT_TYPE_METHODV(_jtype, _jname)                                                                      \
  PROXY_CALL_NONVIRT_TYPE_METHODA(_jtype, _jname)

  PROXY_CALL_NONVIRT_TYPE(jobject, Object)

  PROXY_CALL_NONVIRT_TYPE(jboolean, Boolean)

  PROXY_CALL_NONVIRT_TYPE(jbyte, Byte)

  PROXY_CALL_NONVIRT_TYPE(jchar, Char)

  PROXY_CALL_NONVIRT_TYPE(jshort, Short)

  PROXY_CALL_NONVIRT_TYPE(jint, Int)

  PROXY_CALL_NONVIRT_TYPE(jlong, Long)

  PROXY_CALL_NONVIRT_TYPE(jfloat, Float)

  PROXY_CALL_NONVIRT_TYPE(jdouble, Double)

  void CallNonvirtualVoidMethod(jobject obj, jclass clazz, jmethodID methodID, ...) {
    va_list args;
    va_start(args, methodID);
    ScopedVAArgs scoped_args(&args);
    MONITOR_VOID_CALL(CallNonvirtualVoidMethodV, obj, clazz, methodID, args);
  }

  void CallNonvirtualVoidMethodV(jobject obj, jclass clazz, jmethodID methodID, va_list args) {
    MONITOR_VOID_CALL_VA_LIST(CallNonvirtualVoidMethodV, methodID, args, obj, clazz, methodID);
  }

  void CallNonvirtualVoidMethodA(jobject obj, jclass clazz, jmethodID methodID, const jvalue *args) {
    MONITOR_VOID_CALL_INVOKE(CallNonvirtualVoidMethodA, methodID, obj, clazz, methodID, args);
  }

  jfieldID GetFieldID(jclass clazz, const char *name, const char *sig) { MONITOR_CALL(GetFieldID, clazz, name, sig); }

  jobject GetObjectField(jobject obj, jfieldID fieldID) { MONITOR_CALL(GetObjectField, obj, fieldID); }

  jboolean GetBooleanField(jobject obj, jfieldID fieldID) { MONITOR_CALL(GetBooleanField, obj, fieldID); }

  jbyte GetByteField(jobject obj, jfieldID fieldID) { MONITOR_CALL(GetByteField, obj, fieldID); }

  jchar GetCharField(jobject obj, jfieldID fieldID) { MONITOR_CALL(GetCharField, obj, fieldID); }

  jshort GetShortField(jobject obj, jfieldID fieldID) { MONITOR_CALL(GetShortField, obj, fieldID); }

  jint GetIntField(jobject obj, jfieldID fieldID) { MONITOR_CALL(GetIntField, obj, fieldID); }

  jlong GetLongField(jobject obj, jfieldID fieldID) { MONITOR_CALL(GetLongField, obj, fieldID); }

  jfloat GetFloatField(jobject obj, jfieldID fieldID) { MONITOR_CALL(GetFloatField, obj, fieldID); }

  jdouble GetDoubleField(jobject obj, jfieldID fieldID) { MONITOR_CALL(GetDoubleField, obj, fieldID); }

  void SetObjectField(jobject obj, jfieldID fieldID, jobject value) {
    MONITOR_VOID_CALL(SetObjectField, obj, fieldID, value);
  }

  void SetBooleanField(jobject obj, jfieldID fieldID, jboolean value) {
    MONITOR_VOID_CALL(SetBooleanField, obj, fieldID, value);
  }

  void SetByteField(jobject obj, jfieldID fieldID, jbyte value) {
    MONITOR_VOID_CALL(SetByteField, obj, fieldID, value);
  }

  void SetCharField(jobject obj, jfieldID fieldID, jchar value) {
    MONITOR_VOID_CALL(SetCharField, obj, fieldID, value);
  }

  void SetShortField(jobject obj, jfieldID fieldID, jshort value) {
    MONITOR_VOID_CALL(SetShortField, obj, fieldID, value);
  }

  void SetIntField(jobject obj, jfieldID fieldID, jint value) { MONITOR_VOID_CALL(SetIntField, obj, fieldID, value); }

  void SetLongField(jobject obj, jfieldID fieldID, jlong value) {
    MONITOR_VOID_CALL(SetLongField, obj, fieldID, value);
  }

  void SetFloatField(jobject obj, jfieldID fieldID, jfloat value) {
    MONITOR_VOID_CALL(SetFloatField, obj, fieldID, value);
  }

  void SetDoubleField(jobject obj, jfieldID fieldID, jdouble value) {
    MONITOR_VOID_CALL(SetDoubleField, obj, fieldID, value);
  }

  jmethodID GetStaticMethodID(jclass clazz, const char *name, const char *sig) {
    MONITOR_CALL(GetStaticMethodID, clazz, name, sig);
  }

#define PROXY_CALL_STATIC_TYPE_METHOD(_jtype, _jname)                                                                  \
  _jtype CallStatic##_jname##Method(jclass clazz, jmethodID methodID, ...) {                                           \
    va_list args;                                                                                                      \
    va_start(args, methodID);                                                                                          \
    ScopedVAArgs scoped_args(&args);                                                                                   \
    MONITOR_CALL(CallStatic##_jname##MethodV, clazz, methodID, args);                                                  \
  }
#define PROXY_CALL_STATIC_TYPE_METHODV(_jtype, _jname)                                                                 \
  _jtype CallStatic##_jname##MethodV(jclass clazz, jmethodID methodID, va_list args) {                                 \
    MONITOR_CALL_VA_LIST(CallStatic##_jname##MethodV, methodID, args, clazz, methodID);                                \
  }
#define PROXY_CALL_STATIC_TYPE_METHODA(_jtype, _jname)                                                                 \
  _jtype CallStatic##_jname##MethodA(jclass clazz, jmethodID methodID, const jvalue *args) {                           \
    MONITOR_CALL_INVOKE(CallStatic##_jname##MethodA, methodID, clazz, methodID, args);                                 \
  }

#define PROXY_CALL_STATIC_TYPE(_jtype, _jname)                                                                         \
  PROXY_CALL_STATIC_TYPE_METHOD(_jtype, _jname)                                                                        \
  PROXY_CALL_STATIC_TYPE_METHODV(_jtype, _jname)                                                                       \
  PROXY_CALL_STATIC_TYPE_METHODA(_jtype, _jname)

  PROXY_CALL_STATIC_TYPE(jobject, Object)

  PROXY_CALL_STATIC_TYPE(jboolean, Boolean)

  PROXY_CALL_STATIC_TYPE(jbyte, Byte)

  PROXY_CALL_STATIC_TYPE(jchar, Char)

  PROXY_CALL_STATIC_TYPE(jshort, Short)

  PROXY_CALL_STATIC_TYPE(jint, Int)

  PROXY_CALL_STATIC_TYPE(jlong, Long)

  PROXY_CALL_STATIC_TYPE(jfloat, Float)

  PROXY_CALL_STATIC_TYPE(jdouble, Double)

  void CallStaticVoidMethod(jclass clazz, jmethodID methodID, ...) {
    va_list args;
    va_start(args, methodID);
    ScopedVAArgs scoped_args(&args);
    MONITOR_VOID_CALL(CallStaticVoidMethodV, clazz, methodID, args);
  }

  void CallStaticVoidMethodV(jclass clazz, jmethodID methodID, va_list args) {
    MONITOR_VOID_CALL_VA_LIST(CallStaticVoidMethodV, methodID, args, clazz, methodID);
  }

  void CallStaticVoidMethodA(jclass clazz, jmethodID methodID, const jvalue *args) {
    MONITOR_VOID_CALL_INVOKE(CallStaticVoidMethodA, methodID, clazz, methodID, args);
  }

  jfieldID GetStaticFieldID(jclass clazz, const char *name, const char *sig) {
    MONITOR_CALL(GetStaticFieldID, clazz, name, sig);
  }

  jobject GetStaticObjectField(jclass clazz, jfieldID fieldID) { MONITOR_CALL(GetStaticObjectField, clazz, fieldID); }

  jboolean GetStaticBooleanField(jclass clazz, jfieldID fieldID) {
    MONITOR_CALL(GetStaticBooleanField, clazz, fieldID);
  }

  jbyte GetStaticByteField(jclass clazz, jfieldID fieldID) { MONITOR_CALL(GetStaticByteField, clazz, fieldID); }

  jchar GetStaticCharField(jclass clazz, jfieldID fieldID) { MONITOR_CALL(GetStaticCharField, clazz, fieldID); }

  jshort GetStaticShortField(jclass clazz, jfieldID fieldID) { MONITOR_CALL(GetStaticShortField, clazz, fieldID); }

  jint GetStaticIntField(jclass clazz, jfieldID fieldID) { MONITOR_CALL(GetStaticIntField, clazz, fieldID); }

  jlong GetStaticLongField(jclass clazz, jfieldID fieldID) { MONITOR_CALL(GetStaticLongField, clazz, fieldID); }

  jfloat GetStaticFloatField(jclass clazz, jfieldID fieldID) { MONITOR_CALL(GetStaticFloatField, clazz, fieldID); }

  jdouble GetStaticDoubleField(jclass clazz, jfieldID fieldID) { MONITOR_CALL(GetStaticDoubleField, clazz, fieldID); }

  void SetStaticObjectField(jclass clazz, jfieldID fieldID, jobject value) {
    MONITOR_VOID_CALL(SetStaticObjectField, clazz, fieldID, value);
  }

  void SetStaticBooleanField(jclass clazz, jfieldID fieldID, jboolean value) {
    MONITOR_VOID_CALL(SetStaticBooleanField, clazz, fieldID, value);
  }

  void SetStaticByteField(jclass clazz, jfieldID fieldID, jbyte value) {
    MONITOR_VOID_CALL(SetStaticByteField, clazz, fieldID, value);
  }

  void SetStaticCharField(jclass clazz, jfieldID fieldID, jchar value) {
    MONITOR_VOID_CALL(SetStaticCharField, clazz, fieldID, value);
  }

  void SetStaticShortField(jclass clazz, jfieldID fieldID, jshort value) {
    MONITOR_VOID_CALL(SetStaticShortField, clazz, fieldID, value);
  }

  void SetStaticIntField(jclass clazz, jfieldID fieldID, jint value) {
    MONITOR_VOID_CALL(SetStaticIntField, clazz, fieldID, value);
  }

  void SetStaticLongField(jclass clazz, jfieldID fieldID, jlong value) {
    MONITOR_VOID_CALL(SetStaticLongField, clazz, fieldID, value);
  }

  void SetStaticFloatField(jclass clazz, jfieldID fieldID, jfloat value) {
    MONITOR_VOID_CALL(SetStaticFloatField, clazz, fieldID, value);
  }

  void SetStaticDoubleField(jclass clazz, jfieldID fieldID, jdouble value) {
    MONITOR_VOID_CALL(SetStaticDoubleField, clazz, fieldID, value);
  }

  jstring NewString(const jchar *unicodeChars, jsize len) { MONITOR_CALL(NewString, unicodeChars, len); }

  jsize GetStringLength(jstring string) { MONITOR_CALL(GetStringLength, string); }

  const jchar *GetStringChars(jstring string, jboolean *isCopy) { MONITOR_CALL(GetStringChars, string, isCopy); }

  void ReleaseStringChars(jstring string, const jchar *chars) {
    MONITOR_VOID_CALL_AFTER(ReleaseStringChars, string, chars);
  }

  jstring NewStringUTF(const char *bytes) { MONITOR_CALL(NewStringUTF, bytes); }

  jsize GetStringUTFLength(jstring string) { MONITOR_CALL(GetStringUTFLength, string); }

  const char *GetStringUTFChars(jstring string, jboolean *isCopy) { MONITOR_CALL(GetStringUTFChars, string, isCopy); }

  void ReleaseStringUTFChars(jstring string, const char *utf) {
    MONITOR_VOID_CALL_AFTER(ReleaseStringUTFChars, string, utf);
  }

  jsize GetArrayLength(jarray array) { MONITOR_CALL(GetArrayLength, array); }

  jobjectArray NewObjectArray(jsize length, jclass elementClass, jobject initialElement) {
    MONITOR_CALL(NewObjectArray, length, elementClass, initialElement);
  }

  jobject GetObjectArrayElement(jobjectArray array, jsize index) { MONITOR_CALL(GetObjectArrayElement, array, index); }

  void SetObjectArrayElement(jobjectArray array, jsize index, jobject value) {
    MONITOR_VOID_CALL(SetObjectArrayElement, array, index, value);
  }

  jbooleanArray NewBooleanArray(jsize length) { MONITOR_CALL(NewBooleanArray, length); }

  jbyteArray NewByteArray(jsize length) { MONITOR_CALL(NewByteArray, length); }

  jcharArray NewCharArray(jsize length) { MONITOR_CALL(NewCharArray, length); }

  jshortArray NewShortArray(jsize length) { MONITOR_CALL(NewShortArray, length); }

  jintArray NewIntArray(jsize length) { MONITOR_CALL(NewIntArray, length); }

  jlongArray NewLongArray(jsize length) { MONITOR_CALL(NewLongArray, length); }

  jfloatArray NewFloatArray(jsize length) { MONITOR_CALL(NewFloatArray, length); }

  jdoubleArray NewDoubleArray(jsize length) { MONITOR_CALL(NewDoubleArray, length); }

  jboolean *GetBooleanArrayElements(jbooleanArray array, jboolean *isCopy) {
    MONITOR_CALL(GetBooleanArrayElements, array, isCopy);
  }

  jbyte *GetByteArrayElements(jbyteArray array, jboolean *isCopy) { MONITOR_CALL(GetByteArrayElements, array, isCopy); }

  jchar *GetCharArrayElements(jcharArray array, jboolean *isCopy) { MONITOR_CALL(GetCharArrayElements, array, isCopy); }

  jshort *GetShortArrayElements(jshortArray array, jboolean *isCopy) {
    MONITOR_CALL(GetShortArrayElements, array, isCopy);
  }

  jint *GetIntArrayElements(jintArray array, jboolean *isCopy) { MONITOR_CALL(GetIntArrayElements, array, isCopy); }

  jlong *GetLongArrayElements(jlongArray array, jboolean *isCopy) { MONITOR_CALL(GetLongArrayElements, array, isCopy); }

  jfloat *GetFloatArrayElements(jfloatArray array, jboolean *isCopy) {
    MONITOR_CALL(GetFloatArrayElements, array, isCopy);
  }

  jdouble *GetDoubleArrayElements(jdoubleArray array, jboolean *isCopy) {
    MONITOR_CALL(GetDoubleArrayElements, array, isCopy);
  }

  void ReleaseBooleanArrayElements(jbooleanArray array, jboolean *elems, jint mode) {
    MONITOR_VOID_CALL_AFTER(ReleaseBooleanArrayElements, array, elems, mode);
  }

  void ReleaseByteArrayElements(jbyteArray array, jbyte *elems, jint mode) {
    MONITOR_VOID_CALL_AFTER(ReleaseByteArrayElements, array, elems, mode);
  }

  void ReleaseCharArrayElements(jcharArray array, jchar *elems, jint mode) {
    MONITOR_VOID_CALL_AFTER(ReleaseCharArrayElements, array, elems, mode);
  }

  void ReleaseShortArrayElements(jshortArray array, jshort *elems, jint mode) {
    MONITOR_VOID_CALL_AFTER(ReleaseShortArrayElements, array, elems, mode);
  }

  void ReleaseIntArrayElements(jintArray array, jint *elems, jint mode) {
    MONITOR_VOID_CALL_AFTER(ReleaseIntArrayElements, array, elems, mode);
  }

  void ReleaseLongArrayElements(jlongArray array, jlong *elems, jint mode) {
    MONITOR_VOID_CALL_AFTER(ReleaseLongArrayElements, array, elems, mode);
  }

  void ReleaseFloatArrayElements(jfloatArray array, jfloat *elems, jint mode) {
    MONITOR_VOID_CALL_AFTER(ReleaseFloatArrayElements, array, elems, mode);
  }

  void ReleaseDoubleArrayElements(jdoubleArray array, jdouble *elems, jint mode) {
    MONITOR_VOID_CALL_AFTER(ReleaseDoubleArrayElements, array, elems, mode);
  }

  void GetBooleanArrayRegion(jbooleanArray array, jsize start, jsize len, jboolean *buf) {
    MONITOR_VOID_CALL_AFTER(GetBooleanArrayRegion, array, start, len, buf);
  }

  void GetByteArrayRegion(jbyteArray array, jsize start, jsize len, jbyte *buf) {
    MONITOR_VOID_CALL_AFTER(GetByteArrayRegion, array, start, len, buf);
  }

  void GetCharArrayRegion(jcharArray array, jsize start, jsize len, jchar *buf) {
    MONITOR_VOID_CALL_AFTER(GetCharArrayRegion, array, start, len, buf);
  }

  void GetShortArrayRegion(jshortArray array, jsize start, jsize len, jshort *buf) {
    MONITOR_VOID_CALL_AFTER(GetShortArrayRegion, array, start, len, buf);
  }

  void GetIntArrayRegion(jintArray array, jsize start, jsize len, jint *buf) {
    MONITOR_VOID_CALL_AFTER(GetIntArrayRegion, array, start, len, buf);
  }

  void GetLongArrayRegion(jlongArray array, jsize start, jsize len, jlong *buf) {
    MONITOR_VOID_CALL_AFTER(GetLongArrayRegion, array, start, len, buf);
  }

  void GetFloatArrayRegion(jfloatArray array, jsize start, jsize len, jfloat *buf) {
    MONITOR_VOID_CALL_AFTER(GetFloatArrayRegion, array, start, len, buf);
  }

  void GetDoubleArrayRegion(jdoubleArray array, jsize start, jsize len, jdouble *buf) {
    MONITOR_VOID_CALL_AFTER(GetDoubleArrayRegion, array, start, len, buf);
  }

  void SetBooleanArrayRegion(jbooleanArray array, jsize start, jsize len, const jboolean *buf) {
    MONITOR_VOID_CALL_AFTER(SetBooleanArrayRegion, array, start, len, buf);
  }

  void SetByteArrayRegion(jbyteArray array, jsize start, jsize len, const jbyte *buf) {
    MONITOR_VOID_CALL(SetByteArrayRegion, array, start, len, buf);
  }

  void SetCharArrayRegion(jcharArray array, jsize start, jsize len, const jchar *buf) {
    MONITOR_VOID_CALL(SetCharArrayRegion, array, start, len, buf);
  }

  void SetShortArrayRegion(jshortArray array, jsize start, jsize len, const jshort *buf) {
    MONITOR_VOID_CALL(SetShortArrayRegion, array, start, len, buf);
  }

  void SetIntArrayRegion(jintArray array, jsize start, jsize len, const jint *buf) {
    MONITOR_VOID_CALL(SetIntArrayRegion, array, start, len, buf);
  }

  void SetLongArrayRegion(jlongArray array, jsize start, jsize len, const jlong *buf) {
    MONITOR_VOID_CALL(SetLongArrayRegion, array, start, len, buf);
  }

  void SetFloatArrayRegion(jfloatArray array, jsize start, jsize len, const jfloat *buf) {
    MONITOR_VOID_CALL(SetFloatArrayRegion, array, start, len, buf);
  }

  void SetDoubleArrayRegion(jdoubleArray array, jsize start, jsize len, const jdouble *buf) {
    MONITOR_VOID_CALL(SetDoubleArrayRegion, array, start, len, buf);
  }

  jint RegisterNatives(jclass clazz, const JNINativeMethod *methods, jint nMethods) {
    MONITOR_CALL(RegisterNatives, clazz, methods, nMethods);
  }

  jint UnregisterNatives(jclass clazz) { MONITOR_CALL(UnregisterNatives, clazz); }

  jint MonitorEnter(jobject obj) { MONITOR_CALL(MonitorEnter, obj); }

  jint MonitorExit(jobject obj) { MONITOR_CALL(MonitorExit, obj); }

  jint GetJavaVM(JavaVM **vm) { MONITOR_CALL(GetJavaVM, vm); }

  void GetStringRegion(jstring str, jsize start, jsize len, jchar *buf) {
    MONITOR_VOID_CALL_AFTER(GetStringRegion, str, start, len, buf);
  }

  void GetStringUTFRegion(jstring str, jsize start, jsize len, char *buf) {
    MONITOR_VOID_CALL_AFTER(GetStringUTFRegion, str, start, len, buf);
  }

  void *GetPrimitiveArrayCritical(jarray array, jboolean *isCopy) {
    MONITOR_CALL(GetPrimitiveArrayCritical, array, isCopy);
  }

  void ReleasePrimitiveArrayCritical(jarray array, void *carray, jint mode) {
    MONITOR_VOID_CALL_AFTER(ReleasePrimitiveArrayCritical, array, carray, mode);
  }

  const jchar *GetStringCritical(jstring string, jboolean *isCopy) { MONITOR_CALL(GetStringCritical, string, isCopy); }

  void ReleaseStringCritical(jstring string, const jchar *carray) {
    MONITOR_VOID_CALL_AFTER(ReleaseStringCritical, string, carray);
  }

  jweak NewWeakGlobalRef(jobject obj) { MONITOR_CALL(NewWeakGlobalRef, obj); }

  void DeleteWeakGlobalRef(jweak obj) { MONITOR_VOID_CALL_AFTER(DeleteWeakGlobalRef, obj); }

  jboolean ExceptionCheck() { MONITOR_CALL(ExceptionCheck); }

  jobject NewDirectByteBuffer(void *address, jlong capacity) { MONITOR_CALL(NewDirectByteBuffer, address, capacity); }

  void *GetDirectBufferAddress(jobject buf) { MONITOR_CALL(GetDirectBufferAddress, buf); }

  jlong GetDirectBufferCapacity(jobject buf) { MONITOR_CALL(GetDirectBufferCapacity, buf); }

  /* added in JNI 1.6 */
  jobjectRefType GetObjectRefType(jobject obj) { MONITOR_CALL(GetObjectRefType, obj); }
};

#undef MONITOR_CALL
#undef MONITOR_CALL_VA_LIST
#undef MONITOR_CALL_INVOKE
#undef MONITOR_CALL_INVALID_ARGS
#undef MONITOR_VOID_CALL
#undef MONITOR_VOID_CALL_INVOKE
#undef MONITOR_VOID_CALL_VA_LIST
#undef MONITOR_VOID_CALL_AFTER

template <typename Derived>
class JNIMonitor {
public:
  JNIMonitor() = default;

  static bool InitHookJNI(JNIEnv *env) {
    JNIHelper::Init(env);
    memcpy(&jni_trace::backup_jni, env->functions, sizeof(JNINativeInterface));
    ProxyJNIEnv::SetBackupFunctions(&jni_trace::backup_jni);
    jni_trace::org_jni = &jni_trace::backup_jni;
    return true;
  }

  template <typename T>
  bool AddTraceFunctions(const T &elem) {
    bool res = true;
    if constexpr (_is_valid_container<T>::value) {
      for (const auto &item : elem) {
        res &= AddTraceFunction(item);
      }
    } else {
      static_assert(std::is_same_v<T, size_t>, "required size_t type");
      res = AddTraceFunction(elem);
    }
    return res;
  }

  // caller ensures that no duplicate additions are made
  bool AddTraceFunction(size_t offset) {
    if (offset < offsetof(JNINativeInterface, GetVersion) || offset > offsetof(JNINativeInterface, GetObjectRefType)) {
      LOGE("invalid jni function offset: 0x%zx", offset);
      return false;
    }
    trace_offsets_.push_back(offset);
    return true;
  }

  bool StartTrace(Derived *trace_callback) {
    if (trace_offsets_.empty()) {
      LOGE("Trace jni items is empty");
      return false;
    }
    jni_trace::monitor = this;
    trace_callback->SetOriginalEnv(&jni_trace::org_env);
    jni_trace::callback = trace_callback;
    std::vector<HookJniUnit> hooks;
    for (auto offset : trace_offsets_) {
      hooks.push_back(HookJniUnit{.offset = static_cast<int>(offset),
                                  .hook_method = reinterpret_cast<void *>(
                                    *reinterpret_cast<uintptr_t *>(((char *)&jni_trace::hook_jni + offset))),
                                  .backup_method = nullptr});
    }
    int num = get_fakelinker()->hook_jni_native_functions(&hooks[0], static_cast<int>(hooks.size()));
    trace_offsets_.clear();
    return num == hooks.size();
  }

  bool AddMonitorLibrary(std::string_view name, bool exclude) {
    SoinfoPtr target = get_fakelinker()->soinfo_find(SoinfoFindType::kSTName, name.data(), nullptr);
    if (!target) {
      LOGE("find soinfo failed: %s", name.data());
      return false;
    }
    SoinfoAttributes attr;
    if (auto code = get_fakelinker()->soinfo_get_attribute(target, &attr)) {
      LOGE("get soinfo attribute failed: %s, error: %d", name.data(), code);
      return false;
    }
    return AddMonitorAddress(static_cast<uintptr_t>(attr.base), static_cast<uintptr_t>(attr.base + attr.size), exclude);
  }

  bool RemoveMonitorLibrary(std::string_view name) {
    SoinfoPtr target = get_fakelinker()->soinfo_find(SoinfoFindType::kSTName, name.data(), nullptr);
    if (!target) {
      LOGE("find soinfo failed: %s", name.data());
      return false;
    }
    SoinfoAttributes attr;
    if (auto code = get_fakelinker()->soinfo_get_attribute(target, &attr)) {
      LOGE("get soinfo attribute failed: %s, error: %d", name.data(), code);
      return false;
    }
    return RemoveMonitorAddress(static_cast<uintptr_t>(attr.base), static_cast<uintptr_t>(attr.base + attr.size));
  }

  bool AddMonitorAddress(uintptr_t start, uintptr_t end, bool exclude) {
    if (end < start) {
      return false;
    }
    if (exclude != exclude_) {
      exclude_ = exclude;
      monitors_.clear();
    }
    LOGD("add jni monitor address start: 0x%" PRIxPTR " end: 0x%" PRIxPTR " exclude: %d", start, end, exclude);
    monitors_[start] = end;
    return true;
  }

  bool RemoveMonitorAddress(uintptr_t start, uintptr_t end) {
    auto itr = monitors_.find(start);
    if (itr != monitors_.end() && itr->second == end) {
      monitors_.erase(itr);
      return true;
    }
    return false;
  }

  void Clear() { monitors_.clear(); }

  bool IsMonitoring(uintptr_t addr) {
    auto itr = monitors_.upper_bound(addr);
    if (itr == monitors_.begin()) {
      return false;
    }
    --itr;
    return addr <= itr->second && !exclude_;
  }

  bool IsMonitoring(void *addr) { return IsMonitoring(reinterpret_cast<uintptr_t>(addr)); }

private:
  std::vector<size_t> trace_offsets_;
  std::map<uintptr_t, uintptr_t> monitors_;
  bool exclude_ = true;
  uintptr_t last_start_ = 0;
  uintptr_t last_end_ = 0;
  bool initialized_ = false;
  DISALLOW_COPY_AND_ASSIGN(JNIMonitor);
};

/**
 * @class BaseTraceJNICallback
 * @brief A base class for tracing and logging JNI calls in a custom JNIEnv wrapper.
 *
 * This template class provides a comprehensive framework for intercepting, tracing, and logging
 * JNI function calls. It is designed to be inherited by user-defined callback classes, such as
 * DefaultTraceJNICallback, to customize tracing behavior for JNI environments.
 *
 * @tparam Derived The derived callback class implementing custom trace logic.
 *
 * ## Features
 * - Intercepts all major JNI function calls, including method/field access, object creation, array operations, etc.
 * - Formats and logs method/field IDs, arguments, return values, and JNI object types.
 * - Supports strict mode for enhanced safety and validation.
 * - Maintains caches for method and field descriptions to improve performance.
 * - Provides utilities for formatting JNI types, classes, strings, and objects.
 * - Integrates with Android logging via __android_log_print.
 * - Supports registration of trace hooks for a wide range of JNI functions.
 * - Allows derived classes to override trace formatting and logging behavior.
 *
 * ## Usage
 * 1. Inherit from BaseTraceJNICallback in your callback class:
 *    @code
 *    class DefaultTraceJNICallback : public BaseTraceJNICallback<DefaultTraceJNICallback> {
 *    public:
 *      explicit DefaultTraceJNICallback(bool strict) : BaseTraceJNICallback(strict) {}
 *    };
 *    @endcode
 *
 * 2. Instantiate your callback and initialize tracing:
 *    @code
 *    DefaultTraceJNICallback tracer(true);
 *    tracer.InitTrace(env); // env is a pointer to JNIEnv
 *    tracer->AddMonitorAddress(0x1000, 0x2000, false); // Add address to monitor
 *    tracer->AddMonitorLibrary("libexample.so", false); // Add library to monitor
 *    tracer.DefaultRegister(); // Registers default JNI functions and start tracing
 *    @endcode
 *
 * 3. Optionally, use SetStrictMode, SetOriginalEnv, or ClearCache as needed.
 *
 * ## Important Methods
 * - InitTrace(JNIEnv* env): Initializes tracing and hooks JNI functions.
 * - DefaultRegister(): Registers a default set of JNI functions and start tracing.
 * - FormatMethodID/FormatFieldID: Returns human-readable descriptions for method/field IDs.
 * - FormatJNIArguments/FormatReturnValue: Formats and logs JNI call arguments and return values.
 * - TraceLog/TraceLine: Logging utilities for trace output.
 *
 * ## Thread Safety
 * JNI callbacks use independent Context instances to achieve thread safety, but
 * JNIMonitor element addition is not thread-safe. It is recommended to complete
 * adding monitoring addresses before starting the trace.
 *
 * ## Example
 * See DefaultTraceJNICallback in default_trace_jni.h for a minimal implementation.
 *
 * @see DefaultTraceJNICallback
 */
template <typename Derived>
class BaseTraceJNICallback : public HookJNIEnv<Derived> {
  friend class HookJNIEnv<Derived>;
  friend Derived;

public:
  BaseTraceJNICallback(bool strict = true) : strict_mode_(strict) {}

  bool InitTrace(JNIEnv *env) {
    this->BindMethod();
    return JNIMonitor<Derived>::InitHookJNI(env);
  }

  const char *FormatMethodID(JNIEnv *env, jmethodID method, bool check = true) {
    if (check && !method) {
      return "(null method)";
    }
    if (method == last_method_) {
      return last_method_desc_;
    }
    auto itr = cache_methods_.find(method);
    if (itr != cache_methods_.end()) {
      return itr->second.c_str();
    }
    GuardJNIEnv guard(env, original_env);
    auto result = cache_methods_.emplace(method, JNIHelper::PrettyMethod(env, method, check)).first->second.c_str();
    last_method_ = method;
    last_method_desc_ = result;
    return result;
  }

  const char *FormatFieldID(JNIEnv *env, jfieldID field, bool check = true) {
    if (check && !field) {
      return "(null field)";
    }
    if (field == last_field_) {
      return last_field_desc_;
    }
    auto itr = cache_fields_.find(field);
    if (itr != cache_fields_.end()) {
      return itr->second.c_str();
    }
    GuardJNIEnv guard(env, original_env);
    auto result = cache_fields_.emplace(field, JNIHelper::PrettyField(env, field, check)).first->second.c_str();
    last_field_ = field;
    last_field_desc_ = result;
    return result;
  }

  std::string GetMethodShorty(JNIEnv *env, jmethodID method, bool check = true) {
    if (check && !method) {
      return "(null method)";
    }
    return JNIHelper::GetMethodShorty(env, method, check);
  }

  std::string FormatClass(JNIEnv *env, jclass clazz, bool check = true) {
    if (check && !clazz) {
      return "(null class)";
    }
    GuardJNIEnv guard(env, original_env);
    auto res = JNIHelper::GetClassName(env, clazz, check);
    return res;
  }

  std::string FormatString(JNIEnv *env, jstring string, bool check = true) {
    if (check && !string) {
      return "(null string)";
    }
    GuardJNIEnv guard(env, original_env);
    return JNIHelper::ToString(env, string, check);
  }

  std::string FormatObjectClass(JNIEnv *env, jobject obj, bool check = true) {
    if (check && !obj) {
      return "(null object)";
    }
    GuardJNIEnv guard(env, original_env);
    std::string type = JNIHelper::GetObjectClassName(env, obj, check);
    if (type == "java.lang.String") {
      return FormatString(env, (jstring)obj, check);
    }
    return type;
  }

  std::string FormatObject(JNIEnv *env, jobject obj, bool check = true) {
    if (check && !obj) {
      return "(null object)";
    }
    GuardJNIEnv guard(env, original_env);
    return JNIHelper::ToString(env, obj, check);
  }

  void ClearCache(bool clear_methods = true, bool clear_fields = true) {
    if (clear_methods) {
      cache_methods_.clear();
    }
    if (clear_fields) {
      cache_fields_.clear();
    }
  }

  void SetStrictMode(bool strict) { strict_mode_ = strict; }

  void SetOriginalEnv(JNIEnv *original_env_) { original_env = original_env_; }

  JNIMonitor<Derived> *operator->() { return &jni_monitor_; }

  JNIMonitor<Derived> *GetMonitor() { return &jni_monitor_; }

  SymbolResolver &GetSymbolResolver() { return symbol_resolver_; }


  bool DefaultRegister() {
#define REGISTER_METHOD(name) offsetof(JNINativeInterface, name)
    std::vector<size_t> offsets = {
      REGISTER_METHOD(FindClass),
      REGISTER_METHOD(GetSuperclass),
      REGISTER_METHOD(NewGlobalRef),
      REGISTER_METHOD(DeleteGlobalRef),
      REGISTER_METHOD(NewObjectV),
      REGISTER_METHOD(NewObjectA),
      REGISTER_METHOD(GetObjectClass),
      REGISTER_METHOD(GetMethodID),
      REGISTER_METHOD(CallObjectMethodV),
      REGISTER_METHOD(CallObjectMethodA),
      REGISTER_METHOD(CallBooleanMethodV),
      REGISTER_METHOD(CallBooleanMethodA),
      REGISTER_METHOD(CallByteMethodV),
      REGISTER_METHOD(CallByteMethodA),
      REGISTER_METHOD(CallCharMethodV),
      REGISTER_METHOD(CallCharMethodA),
      REGISTER_METHOD(CallShortMethodV),
      REGISTER_METHOD(CallShortMethodA),
      REGISTER_METHOD(CallIntMethodV),
      REGISTER_METHOD(CallIntMethodA),
      REGISTER_METHOD(CallLongMethodV),
      REGISTER_METHOD(CallLongMethodA),
      REGISTER_METHOD(CallFloatMethodV),
      REGISTER_METHOD(CallFloatMethodA),
      REGISTER_METHOD(CallDoubleMethodV),
      REGISTER_METHOD(CallDoubleMethodA),
      REGISTER_METHOD(CallVoidMethodV),
      REGISTER_METHOD(CallVoidMethodA),
      REGISTER_METHOD(CallNonvirtualObjectMethodV),
      REGISTER_METHOD(CallNonvirtualObjectMethodA),
      REGISTER_METHOD(CallNonvirtualBooleanMethodV),
      REGISTER_METHOD(CallNonvirtualBooleanMethodA),
      REGISTER_METHOD(CallNonvirtualByteMethodV),
      REGISTER_METHOD(CallNonvirtualByteMethodA),
      REGISTER_METHOD(CallNonvirtualCharMethodV),
      REGISTER_METHOD(CallNonvirtualCharMethodA),
      REGISTER_METHOD(CallNonvirtualShortMethodV),
      REGISTER_METHOD(CallNonvirtualShortMethodA),
      REGISTER_METHOD(CallNonvirtualIntMethodV),
      REGISTER_METHOD(CallNonvirtualIntMethodA),
      REGISTER_METHOD(CallNonvirtualLongMethodV),
      REGISTER_METHOD(CallNonvirtualLongMethodA),
      REGISTER_METHOD(CallNonvirtualFloatMethodV),
      REGISTER_METHOD(CallNonvirtualFloatMethodA),
      REGISTER_METHOD(CallNonvirtualDoubleMethodV),
      REGISTER_METHOD(CallNonvirtualDoubleMethodA),
      REGISTER_METHOD(CallNonvirtualVoidMethodV),
      REGISTER_METHOD(CallNonvirtualVoidMethodA),
      REGISTER_METHOD(GetFieldID),
      REGISTER_METHOD(GetObjectField),
      REGISTER_METHOD(GetBooleanField),
      REGISTER_METHOD(GetByteField),
      REGISTER_METHOD(GetCharField),
      REGISTER_METHOD(GetIntField),
      REGISTER_METHOD(GetLongField),
      REGISTER_METHOD(GetFloatField),
      REGISTER_METHOD(GetDoubleField),
      REGISTER_METHOD(SetObjectField),
      REGISTER_METHOD(SetBooleanField),
      REGISTER_METHOD(SetByteField),
      REGISTER_METHOD(SetCharField),
      REGISTER_METHOD(SetShortField),
      REGISTER_METHOD(SetIntField),
      REGISTER_METHOD(SetLongField),
      REGISTER_METHOD(SetFloatField),
      REGISTER_METHOD(SetDoubleField),
      REGISTER_METHOD(GetStaticMethodID),
      REGISTER_METHOD(CallStaticObjectMethodV),
      REGISTER_METHOD(CallStaticObjectMethodA),
      REGISTER_METHOD(CallStaticBooleanMethodV),
      REGISTER_METHOD(CallStaticBooleanMethodA),
      REGISTER_METHOD(CallStaticByteMethodV),
      REGISTER_METHOD(CallStaticByteMethodA),
      REGISTER_METHOD(CallStaticCharMethodV),
      REGISTER_METHOD(CallStaticCharMethodA),
      REGISTER_METHOD(CallStaticShortMethodV),
      REGISTER_METHOD(CallStaticShortMethodA),
      REGISTER_METHOD(CallStaticIntMethodV),
      REGISTER_METHOD(CallStaticIntMethodA),
      REGISTER_METHOD(CallStaticLongMethodV),
      REGISTER_METHOD(CallStaticLongMethodA),
      REGISTER_METHOD(CallStaticFloatMethodV),
      REGISTER_METHOD(CallStaticFloatMethodA),
      REGISTER_METHOD(CallStaticDoubleMethodV),
      REGISTER_METHOD(CallStaticDoubleMethodA),
      REGISTER_METHOD(CallStaticVoidMethodV),
      REGISTER_METHOD(CallStaticVoidMethodA),
      REGISTER_METHOD(GetStaticFieldID),
      REGISTER_METHOD(GetStaticObjectField),
      REGISTER_METHOD(GetStaticBooleanField),
      REGISTER_METHOD(GetStaticByteField),
      REGISTER_METHOD(GetStaticCharField),
      REGISTER_METHOD(GetStaticShortField),
      REGISTER_METHOD(GetStaticIntField),
      REGISTER_METHOD(GetStaticLongField),
      REGISTER_METHOD(GetStaticFloatField),
      REGISTER_METHOD(GetStaticDoubleField),
      REGISTER_METHOD(SetStaticObjectField),
      REGISTER_METHOD(SetStaticBooleanField),
      REGISTER_METHOD(SetStaticByteField),
      REGISTER_METHOD(SetStaticCharField),
      REGISTER_METHOD(SetStaticShortField),
      REGISTER_METHOD(SetStaticIntField),
      REGISTER_METHOD(SetStaticLongField),
      REGISTER_METHOD(SetStaticFloatField),
      REGISTER_METHOD(SetStaticDoubleField),
      REGISTER_METHOD(NewString),
      REGISTER_METHOD(GetStringLength),
      REGISTER_METHOD(GetStringChars),
      REGISTER_METHOD(NewStringUTF),
      REGISTER_METHOD(GetStringUTFLength),
      REGISTER_METHOD(GetStringUTFChars),
      REGISTER_METHOD(GetArrayLength),
      REGISTER_METHOD(NewObjectArray),
      REGISTER_METHOD(GetObjectArrayElement),
      REGISTER_METHOD(SetObjectArrayElement),
      REGISTER_METHOD(NewBooleanArray),
      REGISTER_METHOD(NewByteArray),
      REGISTER_METHOD(NewCharArray),
      REGISTER_METHOD(NewShortArray),
      REGISTER_METHOD(NewIntArray),
      REGISTER_METHOD(NewLongArray),
      REGISTER_METHOD(NewFloatArray),
      REGISTER_METHOD(NewDoubleArray),
      REGISTER_METHOD(GetBooleanArrayElements),
      REGISTER_METHOD(GetByteArrayElements),
      REGISTER_METHOD(GetCharArrayElements),
      REGISTER_METHOD(GetShortArrayElements),
      REGISTER_METHOD(GetIntArrayElements),
      REGISTER_METHOD(GetLongArrayElements),
      REGISTER_METHOD(GetFloatArrayElements),
      REGISTER_METHOD(GetDoubleArrayElements),
      REGISTER_METHOD(GetBooleanArrayRegion),
      REGISTER_METHOD(GetByteArrayRegion),
      REGISTER_METHOD(GetCharArrayRegion),
      REGISTER_METHOD(GetShortArrayRegion),
      REGISTER_METHOD(GetIntArrayRegion),
      REGISTER_METHOD(GetLongArrayRegion),
      REGISTER_METHOD(GetFloatArrayRegion),
      REGISTER_METHOD(GetDoubleArrayRegion),
      REGISTER_METHOD(SetBooleanArrayRegion),
      REGISTER_METHOD(SetByteArrayRegion),
      REGISTER_METHOD(SetCharArrayRegion),
      REGISTER_METHOD(SetShortArrayRegion),
      REGISTER_METHOD(SetIntArrayRegion),
      REGISTER_METHOD(SetLongArrayRegion),
      REGISTER_METHOD(SetFloatArrayRegion),
      REGISTER_METHOD(SetDoubleArrayRegion),
      REGISTER_METHOD(RegisterNatives),
      REGISTER_METHOD(UnregisterNatives),
      REGISTER_METHOD(GetJavaVM),
      REGISTER_METHOD(GetStringRegion),
      REGISTER_METHOD(GetStringUTFRegion),
      REGISTER_METHOD(NewWeakGlobalRef),
      REGISTER_METHOD(DeleteWeakGlobalRef),
    };
#undef REGISTER_METHOD
    return Registers(offsets);
  }

  bool Registers(const std::vector<size_t> &offsets) {
    this->jni_monitor_.AddTraceFunctions(offsets);
    return this->jni_monitor_.StartTrace(static_cast<Derived *>(this));
  }


  void TraceLog(std::string_view message) { __android_log_print(ANDROID_LOG_WARN, "JNITrace", "%s", message.data()); }

  template <typename ReturnType, bool AllowAccessArgs>
  void TraceStart(TraceInvokeContext<ReturnType, AllowAccessArgs> &context, const char *name) {
    static_cast<Derived *>(this)->TraceLine(context, "----------- %s: %s -----------", name,
                                            symbol_resolver_.FormatAddress(context.caller, true).c_str());
  }

  template <typename ReturnType, bool AllowAccessArgs>
  void TraceEnd(TraceInvokeContext<ReturnType, AllowAccessArgs> &context) {
    static_cast<Derived *>(this)->TraceLine(context, "----------------- End -----------------");
    constexpr size_t max_message_length = sizeof(TraceInvokeContext<ReturnType, AllowAccessArgs>::message);
    if (context.message_length < max_message_length) {
      context.message[context.message_length] = '\0';
    } else {
      context.message[max_message_length - 1] = '\0';
      context.message_length = max_message_length - 1;
    }
    static_cast<Derived *>(this)->TraceLog(std::string_view(context.message, context.message_length));
  }

  template <typename ReturnType, bool AllowAccessArgs>
  void TraceLine(TraceInvokeContext<ReturnType, AllowAccessArgs> &context, std::string_view line, ...) {
    constexpr size_t max_message_length = sizeof(TraceInvokeContext<ReturnType, AllowAccessArgs>::message);
    va_list args;
    va_start(args, line);
    if (context.message_length < max_message_length) {
      int len = vsnprintf(context.message + context.message_length, max_message_length - context.message_length,
                          line.data(), args);
      if (len > 0) {
        context.message_length += len;
      }
      if (context.message_length < max_message_length) {
        context.message[context.message_length++] = '\n';
      }
    }
    va_end(args);
  }

  template <typename T, bool AllowAccessArgs>
  void FormatReturnValue(TraceInvokeContext<T, AllowAccessArgs> &context) {
    if constexpr (std::is_same_v<T, const char *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  const char*   %s", context.result);
    } else if constexpr (std::is_same_v<T, jboolean>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jboolean      %s", context.result ? "true" : "false");
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jboolean *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jboolean*     %12p", context.result);
    } else if constexpr (std::is_same_v<T, jbyte>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jbyte         %d", context.result);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jbyte *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jbyte*        %12p", context.result);
    } else if constexpr (std::is_same_v<T, jchar>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jchar         %d", context.result);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jchar *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jchar*        %12p", context.result);
    } else if constexpr (std::is_same_v<T, jshort>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jshort        %d", context.result);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jshort *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jshort*       %12p", context.result);
    } else if constexpr (std::is_same_v<T, jint>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jint          %d", context.result);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jint *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jint*         %12p", context.result);
    } else if constexpr (std::is_same_v<T, jlong>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jlong         %ld", context.result);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jlong *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jlong*        %12p", context.result);
    } else if constexpr (std::is_same_v<T, jfloat>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jfloat        %f", context.result);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jfloat *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jfloat*       %12p", context.result);
    } else if constexpr (std::is_same_v<T, jdouble>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jdouble       %f", context.result);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jdouble *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jdouble*      %12p", context.result);
    } else if constexpr (std::is_same_v<T, jobject>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jobject       %12p : %s", context.result,
                                              FormatObjectClass(context.env, context.result, true).c_str());
    } else if constexpr (std::is_same_v<T, jclass>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jclass        %12p : %s", context.result,
                                              FormatClass(context.env, context.result, true).c_str());
    } else if constexpr (std::is_same_v<T, jstring>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jstring       %12p : %s", context.result,
                                              FormatString(context.env, context.result, true).c_str());
    } else if constexpr (std::is_same_v<T, jmethodID>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jmethodID     %12p : %s", context.result,
                                              FormatMethodID(context.env, context.result, true));
    } else if constexpr (std::is_same_v<T, jfieldID>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jfieldID      %12p : %s", context.result,
                                              FormatFieldID(context.env, context.result, true));
    } else if constexpr (std::is_same_v<T, jarray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jarray        %12p", context.result);
    } else if constexpr (std::is_same_v<T, jobjectArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jobjectArray  %12p", context.result);
    } else if constexpr (std::is_same_v<T, jbooleanArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jbooleanArray %12p", context.result);
    } else if constexpr (std::is_same_v<T, jbyteArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jbyteArray    %12p", context.result);
    } else if constexpr (std::is_same_v<T, jcharArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jcharArray    %12p", context.result);
    } else if constexpr (std::is_same_v<T, jshortArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jshortArray   %12p", context.result);
    } else if constexpr (std::is_same_v<T, jintArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jintArray     %12p", context.result);
    } else if constexpr (std::is_same_v<T, jlongArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jlongArray    %12p", context.result);
    } else if constexpr (std::is_same_v<T, jfloatArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jfloatArray   %12p", context.result);
    } else if constexpr (std::is_same_v<T, jdoubleArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jdoubleArray  %12p", context.result);
    } else if constexpr (std::is_same_v<T, jthrowable>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jthrowable    %12p", context.result);
    } else if constexpr (std::is_same_v<T, jweak>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  jweak         %12p", context.result);
    } else if constexpr (std::is_same_v<T, va_list>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  va_list");
    } else if constexpr (std::is_same_v<T, const jvalue *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  const jvalue* %12p", context.result);
    } else if constexpr (std::is_void_v<T>) {
      static_cast<Derived *>(this)->TraceLine(context, "  return  void");
    } else {
      static_cast<Derived *>(this)->TraceLine(context, "  --> unknown return type");
    }
  }

  template <typename ReturnType, typename T>
  void FormatJNIArgument(TraceInvokeContext<ReturnType, true> &context, int index, T value) {
    if constexpr (std::is_same_v<T, const char *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] const char*   %s", index, value);
    } else if constexpr (std::is_same_v<T, jboolean>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jboolean      %s", index, value ? "true" : "false");
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jboolean *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jboolean*     %12p", index, value);
    } else if constexpr (std::is_same_v<T, jbyte>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jbyte         %d", index, value);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jbyte *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jbyte*        %12p", index, value);
    } else if constexpr (std::is_same_v<T, jchar>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jchar         %d", index, value);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jchar *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jchar*        %12p", index, value);
    } else if constexpr (std::is_same_v<T, jshort>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jshort        %d", index, value);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jshort *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jshort*       %12p", index, value);
    } else if constexpr (std::is_same_v<T, jint>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jint          %d", index, value);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jint *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jint*         %12p", index, value);
    } else if constexpr (std::is_same_v<T, jlong>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jlong         %ld", index, value);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jlong *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jlong*        %12p", index, value);
    } else if constexpr (std::is_same_v<T, jfloat>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jfloat        %f", index, value);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jfloat *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jfloat*       %12p", index, value);
    } else if constexpr (std::is_same_v<T, jdouble>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jdouble       %f", index, value);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jdouble *>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jdouble*      %12p", index, value);
    } else if constexpr (std::is_same_v<T, jobject>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jobject       %12p : %s", index, value,
                                              FormatObjectClass(context.env, value, this->strict_mode_).c_str());
    } else if constexpr (std::is_same_v<T, jclass>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jclass        %12p : %s", index, value,
                                              FormatClass(context.env, value, this->strict_mode_).c_str());
    } else if constexpr (std::is_same_v<T, jstring>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jstring       %12p : %s", index, value,
                                              FormatString(context.env, value, this->strict_mode_).c_str());
    } else if constexpr (std::is_same_v<T, jmethodID>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jmethodID     %12p : %s", index, value,
                                              FormatMethodID(context.env, value, this->strict_mode_));
    } else if constexpr (std::is_same_v<T, jfieldID>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jfieldID      %12p : %s", index, value,
                                              FormatFieldID(context.env, value, this->strict_mode_));
    } else if constexpr (std::is_same_v<T, jarray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jarray        %12p", index, value);
    } else if constexpr (std::is_same_v<T, jobjectArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jobjectArray  %12p", index, value);
    } else if constexpr (std::is_same_v<T, jbooleanArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jbooleanArray %12p", index, value);
    } else if constexpr (std::is_same_v<T, jbyteArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jbyteArray    %12p", index, value);
    } else if constexpr (std::is_same_v<T, jcharArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jcharArray    %12p", index, value);
    } else if constexpr (std::is_same_v<T, jshortArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jshortArray   %12p", index, value);
    } else if constexpr (std::is_same_v<T, jintArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jintArray     %12p", index, value);
    } else if constexpr (std::is_same_v<T, jlongArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jlongArray    %12p", index, value);
    } else if constexpr (std::is_same_v<T, jfloatArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jfloatArray   %12p", index, value);
    } else if constexpr (std::is_same_v<T, jdoubleArray>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jdoubleArray  %12p", index, value);
    } else if constexpr (std::is_same_v<T, jthrowable>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jthrowable    %12p", index, value);
    } else if constexpr (std::is_same_v<T, jweak>) {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jweak         %12p", index, value);
    } else if constexpr (std::is_same_v<T, va_list>) {
      std::string shorty = GetMethodShorty(context.env, context.method, true);
      if (shorty.size() > 1) {
        static_cast<Derived *>(this)->TraceLine(context, "  args[%d] va_list shorty: %s", index, shorty.c_str() + 1);
        // shorty[0] is return type, so skip it
        for (size_t i = 1; i < shorty.size(); ++i) {
          bool skip = false;
          switch (shorty[i]) {
          case 'Z': {
            jint z = va_arg(value, jint);
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jboolean    %s", index++,
                                                    z == 0 ? "false" : "true");
          } break;
          case 'B':
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jbyte       %d", index++,
                                                    va_arg(value, jint));
            break;
          case 'C':
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jchar       %d", index++,
                                                    va_arg(value, jint));
            break;
          case 'S':
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jshort      %d", index++,
                                                    va_arg(value, jint));
            break;
          case 'I':
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jint        %d", index++,
                                                    va_arg(value, jint));
            break;
          case 'F':
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jfloat      %f", index++,
                                                    va_arg(value, jdouble));
            break;
          case 'L': {
            jobject arg = va_arg(value, jobject);
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jobject     %12p : %s", index++, arg,
                                                    FormatObjectClass(context.env, arg, this->strict_mode_).c_str());
          } break;
          case 'D':
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jdouble     %f", index++,
                                                    va_arg(value, jdouble));
            break;
          case 'J':
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jlong       %ld", index++,
                                                    va_arg(value, jlong));
            break;
          default:
            static_cast<Derived *>(this)->TraceLine(
              context,
              "    args[%d] unknown type, possible reading error of short method or there is an unhandled exception",
              index++);
            skip = true;
            break;
          }
          if (skip) {
            break;
          }
        }
      }
    } else if constexpr (std::is_same_v<T, const jvalue *>) {
      std::string shorty = GetMethodShorty(context.env, context.method, true);
      if (shorty.size() > 1) {
        static_cast<Derived *>(this)->TraceLine(context, "  args[%d] jvalue* shorty : %s", index, shorty.c_str() + 1);
        // shorty[0] is return type, so skip it
        for (size_t i = 1; i < shorty.size(); ++i) {
          bool skip = false;
          switch (shorty[i]) {
          case 'Z':
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jboolean    %s", index++,
                                                    value[i - 1].z ? "true" : "false");
            break;
          case 'B':
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jbyte       %d", index++, value[i - 1].b);
            break;
          case 'C':
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jchar       %d", index++, value[i - 1].c);
            break;
          case 'S':
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jshort      %d", index++, value[i - 1].s);
            break;
          case 'I':
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jint        %d", index++, value[i - 1].i);
            break;
          case 'F':
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jfloat      %d", index++, value[i - 1].i);
            break;
          case 'L': {
            jobject arg = value[i - 1].l;
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jobject     %12p : %s", index++, arg,
                                                    FormatObjectClass(context.env, arg, this->strict_mode_).c_str());
          } break;
          case 'D':
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jdouble     %ld", index++, value[i - 1].j);
            break;
          case 'J':
            static_cast<Derived *>(this)->TraceLine(context, "    args[%d] jlong       %ld", index++, value[i - 1].j);
            break;
          default:
            static_cast<Derived *>(this)->TraceLine(
              context,
              "    args[%d] unknown type, possible reading error of short method or there is an unhandled exception",
              index++);
            skip = true;
            break;
          }
          if (skip) {
            break;
          }
        }
      }
    } else {
      static_cast<Derived *>(this)->TraceLine(context, "  args[%d] unknown type", index);
    }
  }

  template <typename ReturnType, typename... Args>
  void FormatJNIArguments(const char *name, TraceInvokeContext<ReturnType, true> &context, Args... args) {
    static_cast<Derived *>(this)->TraceStart(context, name);
    int index = 0;
    (static_cast<Derived *>(this)->FormatJNIArgument(context, index++, std::forward<Args>(args)), ...);
    static_cast<Derived *>(this)->FormatReturnValue(context);
    static_cast<Derived *>(this)->TraceEnd(context);
  }

  template <typename ReturnType, typename... Args>
  void FormatJNIArguments(const char *name, TraceInvokeContext<ReturnType, false> &context, Args... args) {
    static_cast<Derived *>(this)->TraceStart(context, name);
    static_cast<Derived *>(this)->FormatReturnValue(context);
    static_cast<Derived *>(this)->TraceEnd(context);
  }


private:
  void GetVersion(TraceInvokeContext<jint> &context) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetVersion", context);
  }

  void DefineClass(TraceInvokeContext<jclass> &context, const char *name, jobject loader, const jbyte *buf,
                   jsize bufLen) {
    static_cast<Derived *>(this)->FormatJNIArguments("DefineClass", context, name, loader, buf, bufLen);
  }

  void FindClass(TraceInvokeContext<jclass> &context, const char *name) {
    static_cast<Derived *>(this)->FormatJNIArguments("FindClass", context, name);
  }

  void FromReflectedMethod(TraceInvokeContext<jmethodID> &context, jobject method) {
    static_cast<Derived *>(this)->FormatJNIArguments("FromReflectedMethod", context, method);
  }

  void FromReflectedField(TraceInvokeContext<jfieldID> &context, jobject field) {
    static_cast<Derived *>(this)->FormatJNIArguments("FromReflectedField", context, field);
  }

  void ToReflectedMethod(TraceInvokeContext<jobject> &context, jclass cls, jmethodID methodID, jboolean isStatic) {
    static_cast<Derived *>(this)->FormatJNIArguments("ToReflectedMethod", context, cls, methodID, isStatic);
  }

  void GetSuperclass(TraceInvokeContext<jclass> &context, jclass clazz) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetSuperclass", context, clazz);
  }

  void IsAssignableFrom(TraceInvokeContext<jboolean> &context, jclass from_clazz, jclass to_clazz) {
    static_cast<Derived *>(this)->FormatJNIArguments("IsAssignableFrom", context, from_clazz, to_clazz);
  }

  void ToReflectedField(TraceInvokeContext<jobject> &context, jclass cls, jfieldID fieldID, jboolean isStatic) {
    static_cast<Derived *>(this)->FormatJNIArguments("ToReflectedField", context, cls, fieldID, isStatic);
  }

  void Throw(TraceInvokeContext<jint> &context, jthrowable obj) {
    static_cast<Derived *>(this)->FormatJNIArguments("Throw", context, obj);
  }

  void ThrowNew(TraceInvokeContext<jint> &context, jclass clazz, const char *message) {
    static_cast<Derived *>(this)->FormatJNIArguments("ThrowNew", context, clazz, message);
  }

  void ExceptionOccurred(TraceInvokeContext<jthrowable> &context) {
    static_cast<Derived *>(this)->FormatJNIArguments("ExceptionOccurred", context);
  }

  void ExceptionDescribe(TraceInvokeContext<void> &context) {
    static_cast<Derived *>(this)->FormatJNIArguments("ExceptionDescribe", context);
  }

  void ExceptionClear(TraceInvokeContext<void> &context) {
    static_cast<Derived *>(this)->FormatJNIArguments("ExceptionClear", context);
  }

  void FatalError(TraceInvokeContext<void> &context, const char *msg) {
    static_cast<Derived *>(this)->FormatJNIArguments("FatalError", context, msg);
  }

  void PushLocalFrame(TraceInvokeContext<jint> &context, jint capacity) {
    static_cast<Derived *>(this)->FormatJNIArguments("PushLocalFrame", context, capacity);
  }

  void PopLocalFrame(TraceInvokeContext<jobject, false> &context, jobject local) {
    static_cast<Derived *>(this)->FormatJNIArguments("PopLocalFrame", context, local);
  }

  void NewGlobalRef(TraceInvokeContext<jobject> &context, jobject obj) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewGlobalRef", context, obj);
  }

  void DeleteGlobalRef(TraceInvokeContext<void> &context, jobject globalRef) {
    static_cast<Derived *>(this)->FormatJNIArguments("DeleteGlobalRef", context, globalRef);
  }

  void DeleteLocalRef(TraceInvokeContext<void> &context, jobject localRef) {
    static_cast<Derived *>(this)->FormatJNIArguments("DeleteLocalRef", context, localRef);
  }

  void IsSameObject(TraceInvokeContext<jboolean> &context, jobject ref1, jobject ref2) {
    static_cast<Derived *>(this)->FormatJNIArguments("IsSameObject", context, ref1, ref2);
  }

  void NewLocalRef(TraceInvokeContext<jobject> &context, jobject ref) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewLocalRef", context, ref);
  }

  void EnsureLocalCapacity(TraceInvokeContext<jint> &context, jint capacity) {
    static_cast<Derived *>(this)->FormatJNIArguments("EnsureLocalCapacity", context, capacity);
  }

  void AllocObject(TraceInvokeContext<jobject> &context, jclass clazz) {
    static_cast<Derived *>(this)->FormatJNIArguments("AllocObject", context, clazz);
  }

  void NewObjectV(TraceInvokeContext<jobject> &context, jclass clazz, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewObjectV", context, clazz, methodID, args);
  }

  void NewObjectA(TraceInvokeContext<jobject> &context, jclass clazz, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewObjectA", context, clazz, methodID, args);
  }

  void GetObjectClass(TraceInvokeContext<jclass> &context, jobject obj) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetObjectClass", context, obj);
  }

  void IsInstanceOf(TraceInvokeContext<jboolean> &context, jobject obj, jclass clazz) {
    static_cast<Derived *>(this)->FormatJNIArguments("IsInstanceOf", context, obj, clazz);
  }

  void GetMethodID(TraceInvokeContext<jmethodID> &context, jclass clazz, const char *name, const char *sig) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetMethodID", context, clazz, name, sig);
  }

  void CallObjectMethodV(TraceInvokeContext<jobject> &context, jobject obj, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallObjectMethodV", context, obj, methodID, args);
  }

  void CallObjectMethodA(TraceInvokeContext<jobject> &context, jobject obj, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallObjectMethodA", context, obj, methodID, args);
  }

  void CallBooleanMethodV(TraceInvokeContext<jboolean> &context, jobject obj, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallBooleanMethodV", context, obj, methodID, args);
  }

  void CallBooleanMethodA(TraceInvokeContext<jboolean> &context, jobject obj, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallBooleanMethodA", context, obj, methodID, args);
  }

  void CallByteMethodV(TraceInvokeContext<jbyte> &context, jobject obj, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallByteMethodV", context, obj, methodID, args);
  }

  void CallByteMethodA(TraceInvokeContext<jbyte> &context, jobject obj, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallByteMethodA", context, obj, methodID, args);
  }

  void CallCharMethodV(TraceInvokeContext<jchar> &context, jobject obj, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallCharMethodV", context, obj, methodID, args);
  }

  void CallCharMethodA(TraceInvokeContext<jchar> &context, jobject obj, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallCharMethodA", context, obj, methodID, args);
  }

  void CallShortMethodV(TraceInvokeContext<jshort> &context, jobject obj, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallShortMethodV", context, obj, methodID, args);
  }

  void CallShortMethodA(TraceInvokeContext<jshort> &context, jobject obj, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallShortMethodA", context, obj, methodID, args);
  }

  void CallIntMethodV(TraceInvokeContext<jint> &context, jobject obj, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallIntMethodV", context, obj, methodID, args);
  }

  void CallIntMethodA(TraceInvokeContext<jint> &context, jobject obj, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallIntMethodA", context, obj, methodID, args);
  }

  void CallLongMethodV(TraceInvokeContext<jlong> &context, jobject obj, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallLongMethodV", context, obj, methodID, args);
  }

  void CallLongMethodA(TraceInvokeContext<jlong> &context, jobject obj, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallLongMethodA", context, obj, methodID, args);
  }

  void CallFloatMethodV(TraceInvokeContext<jfloat> &context, jobject obj, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallFloatMethodV", context, obj, methodID, args);
  }

  void CallFloatMethodA(TraceInvokeContext<jfloat> &context, jobject obj, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallFloatMethodA", context, obj, methodID, args);
  }

  void CallDoubleMethodV(TraceInvokeContext<jdouble> &context, jobject obj, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallDoubleMethodV", context, obj, methodID, args);
  }

  void CallDoubleMethodA(TraceInvokeContext<jdouble> &context, jobject obj, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallDoubleMethodA", context, obj, methodID, args);
  }

  void CallVoidMethodV(TraceInvokeContext<void> &context, jobject obj, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallVoidMethodV", context, obj, methodID, args);
  }

  void CallVoidMethodA(TraceInvokeContext<void> &context, jobject obj, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallVoidMethodA", context, obj, methodID, args);
  }

  void CallNonvirtualObjectMethodV(TraceInvokeContext<jobject> &context, jobject obj, jclass clazz, jmethodID methodID,
                                   va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualObjectMethodV", context, obj, clazz, methodID,
                                                     args);
  }

  void CallNonvirtualObjectMethodA(TraceInvokeContext<jobject> &context, jobject obj, jclass clazz, jmethodID methodID,
                                   const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualObjectMethodA", context, obj, clazz, methodID,
                                                     args);
  }

  void CallNonvirtualBooleanMethodV(TraceInvokeContext<jboolean> &context, jobject obj, jclass clazz,
                                    jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualBooleanMethodV", context, obj, clazz, methodID,
                                                     args);
  }

  void CallNonvirtualBooleanMethodA(TraceInvokeContext<jboolean> &context, jobject obj, jclass clazz,
                                    jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualBooleanMethodA", context, obj, clazz, methodID,
                                                     args);
  }

  void CallNonvirtualByteMethodV(TraceInvokeContext<jbyte> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualByteMethodV", context, obj, clazz, methodID, args);
  }

  void CallNonvirtualByteMethodA(TraceInvokeContext<jbyte> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualByteMethodA", context, obj, clazz, methodID, args);
  }

  void CallNonvirtualCharMethodV(TraceInvokeContext<jchar> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualCharMethodV", context, obj, clazz, methodID, args);
  }

  void CallNonvirtualCharMethodA(TraceInvokeContext<jchar> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualCharMethodA", context, obj, clazz, methodID, args);
  }

  void CallNonvirtualShortMethodV(TraceInvokeContext<jshort> &context, jobject obj, jclass clazz, jmethodID methodID,
                                  va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualShortMethodV", context, obj, clazz, methodID, args);
  }

  void CallNonvirtualShortMethodA(TraceInvokeContext<jshort> &context, jobject obj, jclass clazz, jmethodID methodID,
                                  const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualShortMethodA", context, obj, clazz, methodID, args);
  }

  void CallNonvirtualIntMethodV(TraceInvokeContext<jint> &context, jobject obj, jclass clazz, jmethodID methodID,
                                va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualIntMethodV", context, obj, clazz, methodID, args);
  }

  void CallNonvirtualIntMethodA(TraceInvokeContext<jint> &context, jobject obj, jclass clazz, jmethodID methodID,
                                const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualIntMethodA", context, obj, clazz, methodID, args);
  }

  void CallNonvirtualLongMethodV(TraceInvokeContext<jlong> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualLongMethodV", context, obj, clazz, methodID, args);
  }

  void CallNonvirtualLongMethodA(TraceInvokeContext<jlong> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualLongMethodA", context, obj, clazz, methodID, args);
  }

  void CallNonvirtualFloatMethodV(TraceInvokeContext<jfloat> &context, jobject obj, jclass clazz, jmethodID methodID,
                                  va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualFloatMethodV", context, obj, clazz, methodID, args);
  }

  void CallNonvirtualFloatMethodA(TraceInvokeContext<jfloat> &context, jobject obj, jclass clazz, jmethodID methodID,
                                  const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualFloatMethodA", context, obj, clazz, methodID, args);
  }

  void CallNonvirtualDoubleMethodV(TraceInvokeContext<jdouble> &context, jobject obj, jclass clazz, jmethodID methodID,
                                   va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualDoubleMethodV", context, obj, clazz, methodID,
                                                     args);
  }

  void CallNonvirtualDoubleMethodA(TraceInvokeContext<jdouble> &context, jobject obj, jclass clazz, jmethodID methodID,
                                   const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualDoubleMethodA", context, obj, clazz, methodID,
                                                     args);
  }

  void CallNonvirtualVoidMethodV(TraceInvokeContext<void> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualVoidMethodV", context, obj, clazz, methodID, args);
  }

  void CallNonvirtualVoidMethodA(TraceInvokeContext<void> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallNonvirtualVoidMethodA", context, obj, clazz, methodID, args);
  }

  void GetFieldID(TraceInvokeContext<jfieldID> &context, jclass clazz, const char *name, const char *sig) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetFieldID", context, clazz, name, sig);
  }

  void GetObjectField(TraceInvokeContext<jobject> &context, jobject obj, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetObjectField", context, obj, fieldID);
  }

  void GetBooleanField(TraceInvokeContext<jboolean> &context, jobject obj, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetBooleanField", context, obj, fieldID);
  }

  void GetByteField(TraceInvokeContext<jbyte> &context, jobject obj, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetByteField", context, obj, fieldID);
  }

  void GetCharField(TraceInvokeContext<jchar> &context, jobject obj, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetCharField", context, obj, fieldID);
  }

  void GetShortField(TraceInvokeContext<jshort> &context, jobject obj, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetShortField", context, obj, fieldID);
  }

  void GetIntField(TraceInvokeContext<jint> &context, jobject obj, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetIntField", context, obj, fieldID);
  }

  void GetLongField(TraceInvokeContext<jlong> &context, jobject obj, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetLongField", context, obj, fieldID);
  }

  void GetFloatField(TraceInvokeContext<jfloat> &context, jobject obj, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetFloatField", context, obj, fieldID);
  }

  void GetDoubleField(TraceInvokeContext<jdouble> &context, jobject obj, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetDoubleField", context, obj, fieldID);
  }

  void SetObjectField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jobject value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetObjectField", context, obj, fieldID, value);
  }

  void SetBooleanField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jboolean value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetBooleanField", context, obj, fieldID, value);
  }

  void SetByteField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jbyte value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetByteField", context, obj, fieldID, value);
  }

  void SetCharField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jchar value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetCharField", context, obj, fieldID, value);
  }

  void SetShortField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jshort value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetShortField", context, obj, fieldID, value);
  }

  void SetIntField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jint value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetIntField", context, obj, fieldID, value);
  }

  void SetLongField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jlong value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetLongField", context, obj, fieldID, value);
  }

  void SetFloatField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jfloat value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetFloatField", context, obj, fieldID, value);
  }

  void SetDoubleField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jdouble value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetDoubleField", context, obj, fieldID, value);
  }

  void GetStaticMethodID(TraceInvokeContext<jmethodID> &context, jclass clazz, const char *name, const char *sig) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStaticMethodID", context, clazz, name, sig);
  }

  void CallStaticObjectMethodV(TraceInvokeContext<jobject> &context, jclass clazz, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticObjectMethodV", context, clazz, methodID, args);
  }

  void CallStaticObjectMethodA(TraceInvokeContext<jobject> &context, jclass clazz, jmethodID methodID,
                               const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticObjectMethodA", context, clazz, methodID, args);
  }

  void CallStaticBooleanMethodV(TraceInvokeContext<jboolean> &context, jclass clazz, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticBooleanMethodV", context, clazz, methodID, args);
  }

  void CallStaticBooleanMethodA(TraceInvokeContext<jboolean> &context, jclass clazz, jmethodID methodID,
                                const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticBooleanMethodA", context, clazz, methodID, args);
  }

  void CallStaticByteMethodV(TraceInvokeContext<jbyte> &context, jclass clazz, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticByteMethodV", context, clazz, methodID, args);
  }

  void CallStaticByteMethodA(TraceInvokeContext<jbyte> &context, jclass clazz, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticByteMethodA", context, clazz, methodID, args);
  }

  void CallStaticCharMethodV(TraceInvokeContext<jchar> &context, jclass clazz, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticCharMethodV", context, clazz, methodID, args);
  }

  void CallStaticCharMethodA(TraceInvokeContext<jchar> &context, jclass clazz, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticCharMethodA", context, clazz, methodID, args);
  }

  void CallStaticShortMethodV(TraceInvokeContext<jshort> &context, jclass clazz, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticShortMethodV", context, clazz, methodID, args);
  }

  void CallStaticShortMethodA(TraceInvokeContext<jshort> &context, jclass clazz, jmethodID methodID,
                              const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticShortMethodA", context, clazz, methodID, args);
  }

  void CallStaticIntMethodV(TraceInvokeContext<jint> &context, jclass clazz, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticIntMethodV", context, clazz, methodID, args);
  }

  void CallStaticIntMethodA(TraceInvokeContext<jint> &context, jclass clazz, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticIntMethodA", context, clazz, methodID, args);
  }

  void CallStaticLongMethodV(TraceInvokeContext<jlong> &context, jclass clazz, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticLongMethodV", context, clazz, methodID, args);
  }

  void CallStaticLongMethodA(TraceInvokeContext<jlong> &context, jclass clazz, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticLongMethodA", context, clazz, methodID, args);
  }

  void CallStaticFloatMethodV(TraceInvokeContext<jfloat> &context, jclass clazz, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticFloatMethodV", context, clazz, methodID, args);
  }

  void CallStaticFloatMethodA(TraceInvokeContext<jfloat> &context, jclass clazz, jmethodID methodID,
                              const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticFloatMethodA", context, clazz, methodID, args);
  }

  void CallStaticDoubleMethodV(TraceInvokeContext<jdouble> &context, jclass clazz, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticDoubleMethodV", context, clazz, methodID, args);
  }

  void CallStaticDoubleMethodA(TraceInvokeContext<jdouble> &context, jclass clazz, jmethodID methodID,
                               const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticDoubleMethodA", context, clazz, methodID, args);
  }

  void CallStaticVoidMethodV(TraceInvokeContext<void> &context, jclass clazz, jmethodID methodID, va_list args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticVoidMethodV", context, clazz, methodID, args);
  }

  void CallStaticVoidMethodA(TraceInvokeContext<void> &context, jclass clazz, jmethodID methodID, const jvalue *args) {
    static_cast<Derived *>(this)->FormatJNIArguments("CallStaticVoidMethodA", context, clazz, methodID, args);
  }

  void GetStaticFieldID(TraceInvokeContext<jfieldID> &context, jclass clazz, const char *name, const char *sig) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStaticFieldID", context, clazz, name, sig);
  }

  void GetStaticObjectField(TraceInvokeContext<jobject> &context, jclass clazz, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStaticObjectField", context, clazz, fieldID);
  }

  void GetStaticBooleanField(TraceInvokeContext<jboolean> &context, jclass clazz, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStaticBooleanField", context, clazz, fieldID);
  }

  void GetStaticByteField(TraceInvokeContext<jbyte> &context, jclass clazz, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStaticByteField", context, clazz, fieldID);
  }

  void GetStaticCharField(TraceInvokeContext<jchar> &context, jclass clazz, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStaticCharField", context, clazz, fieldID);
  }

  void GetStaticShortField(TraceInvokeContext<jshort> &context, jclass clazz, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStaticShortField", context, clazz, fieldID);
  }

  void GetStaticIntField(TraceInvokeContext<jint> &context, jclass clazz, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStaticIntField", context, clazz, fieldID);
  }

  void GetStaticLongField(TraceInvokeContext<jlong> &context, jclass clazz, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStaticLongField", context, clazz, fieldID);
  }

  void GetStaticFloatField(TraceInvokeContext<jfloat> &context, jclass clazz, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStaticFloatField", context, clazz, fieldID);
  }

  void GetStaticDoubleField(TraceInvokeContext<jdouble> &context, jclass clazz, jfieldID fieldID) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStaticDoubleField", context, clazz, fieldID);
  }

  void SetStaticObjectField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jobject value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetStaticObjectField", context, clazz, fieldID, value);
  }

  void SetStaticBooleanField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jboolean value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetStaticBooleanField", context, clazz, fieldID, value);
  }

  void SetStaticByteField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jbyte value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetStaticByteField", context, clazz, fieldID, value);
  }

  void SetStaticCharField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jchar value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetStaticCharField", context, clazz, fieldID, value);
  }

  void SetStaticShortField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jshort value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetStaticShortField", context, clazz, fieldID, value);
  }

  void SetStaticIntField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jint value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetStaticIntField", context, clazz, fieldID, value);
  }

  void SetStaticLongField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jlong value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetStaticLongField", context, clazz, fieldID, value);
  }

  void SetStaticFloatField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jfloat value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetStaticFloatField", context, clazz, fieldID, value);
  }

  void SetStaticDoubleField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jdouble value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetStaticDoubleField", context, clazz, fieldID, value);
  }

  void NewString(TraceInvokeContext<jstring> &context, const jchar *unicodeChars, jsize len) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewString", context, unicodeChars, len);
  }

  void GetStringLength(TraceInvokeContext<jsize> &context, jstring string) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStringLength", context, string);
  }

  void GetStringChars(TraceInvokeContext<const jchar *> &context, jstring string, jboolean *isCopy) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStringChars", context, string, isCopy);
  }

  void ReleaseStringChars(TraceInvokeContext<void> &context, jstring string, const jchar *chars) {
    static_cast<Derived *>(this)->FormatJNIArguments("ReleaseStringChars", context, string, chars);
  }

  void NewStringUTF(TraceInvokeContext<jstring> &context, const char *bytes) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewStringUTF", context, bytes);
  }

  void GetStringUTFLength(TraceInvokeContext<jsize> &context, jstring string) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStringUTFLength", context, string);
  }

  void GetStringUTFChars(TraceInvokeContext<const char *> &context, jstring string, jboolean *isCopy) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStringUTFChars", context, string, isCopy);
  }

  void ReleaseStringUTFChars(TraceInvokeContext<void> &context, jstring string, const char *utf) {
    static_cast<Derived *>(this)->FormatJNIArguments("ReleaseStringUTFChars", context, string, utf);
  }

  void GetArrayLength(TraceInvokeContext<jsize> &context, jarray array) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetArrayLength", context, array);
  }

  void NewObjectArray(TraceInvokeContext<jobjectArray> &context, jsize length, jclass elementClass,
                      jobject initialElement) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewObjectArray", context, length, elementClass, initialElement);
  }

  void GetObjectArrayElement(TraceInvokeContext<jobject> &context, jobjectArray array, jsize index) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetObjectArrayElement", context, array, index);
  }

  void SetObjectArrayElement(TraceInvokeContext<void> &context, jobjectArray array, jsize index, jobject value) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetObjectArrayElement", context, array, index, value);
  }

  void NewBooleanArray(TraceInvokeContext<jbooleanArray> &context, jsize length) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewBooleanArray", context, length);
  }

  void NewByteArray(TraceInvokeContext<jbyteArray> &context, jsize length) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewByteArray", context, length);
  }

  void NewCharArray(TraceInvokeContext<jcharArray> &context, jsize length) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewCharArray", context, length);
  }

  void NewShortArray(TraceInvokeContext<jshortArray> &context, jsize length) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewShortArray", context, length);
  }

  void NewIntArray(TraceInvokeContext<jintArray> &context, jsize length) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewIntArray", context, length);
  }

  void NewLongArray(TraceInvokeContext<jlongArray> &context, jsize length) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewLongArray", context, length);
  }

  void NewFloatArray(TraceInvokeContext<jfloatArray> &context, jsize length) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewFloatArray", context, length);
  }

  void NewDoubleArray(TraceInvokeContext<jdoubleArray> &context, jsize length) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewDoubleArray", context, length);
  }

  void GetBooleanArrayElements(TraceInvokeContext<jboolean *> &context, jbooleanArray array, jboolean *isCopy) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetBooleanArrayElements", context, array, isCopy);
  }

  void GetByteArrayElements(TraceInvokeContext<jbyte *> &context, jbyteArray array, jboolean *isCopy) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetByteArrayElements", context, array, isCopy);
  }

  void GetCharArrayElements(TraceInvokeContext<jchar *> &context, jcharArray array, jboolean *isCopy) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetCharArrayElements", context, array, isCopy);
  }

  void GetShortArrayElements(TraceInvokeContext<jshort *> &context, jshortArray array, jboolean *isCopy) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetShortArrayElements", context, array, isCopy);
  }

  void GetIntArrayElements(TraceInvokeContext<jint *> &context, jintArray array, jboolean *isCopy) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetIntArrayElements", context, array, isCopy);
  }

  void GetLongArrayElements(TraceInvokeContext<jlong *> &context, jlongArray array, jboolean *isCopy) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetLongArrayElements", context, array, isCopy);
  }

  void GetFloatArrayElements(TraceInvokeContext<jfloat *> &context, jfloatArray array, jboolean *isCopy) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetFloatArrayElements", context, array, isCopy);
  }

  void GetDoubleArrayElements(TraceInvokeContext<jdouble *> &context, jdoubleArray array, jboolean *isCopy) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetDoubleArrayElements", context, array, isCopy);
  }

  void ReleaseBooleanArrayElements(TraceInvokeContext<void> &context, jbooleanArray array, jboolean *elems, jint mode) {
    static_cast<Derived *>(this)->FormatJNIArguments("ReleaseBooleanArrayElements", context, array, elems, mode);
  }

  void ReleaseByteArrayElements(TraceInvokeContext<void> &context, jbyteArray array, jbyte *elems, jint mode) {
    static_cast<Derived *>(this)->FormatJNIArguments("ReleaseByteArrayElements", context, array, elems, mode);
  }

  void ReleaseCharArrayElements(TraceInvokeContext<void> &context, jcharArray array, jchar *elems, jint mode) {
    static_cast<Derived *>(this)->FormatJNIArguments("ReleaseCharArrayElements", context, array, elems, mode);
  }

  void ReleaseShortArrayElements(TraceInvokeContext<void> &context, jshortArray array, jshort *elems, jint mode) {
    static_cast<Derived *>(this)->FormatJNIArguments("ReleaseShortArrayElements", context, array, elems, mode);
  }

  void ReleaseIntArrayElements(TraceInvokeContext<void> &context, jintArray array, jint *elems, jint mode) {
    static_cast<Derived *>(this)->FormatJNIArguments("ReleaseIntArrayElements", context, array, elems, mode);
  }

  void ReleaseLongArrayElements(TraceInvokeContext<void> &context, jlongArray array, jlong *elems, jint mode) {
    static_cast<Derived *>(this)->FormatJNIArguments("ReleaseLongArrayElements", context, array, elems, mode);
  }

  void ReleaseFloatArrayElements(TraceInvokeContext<void> &context, jfloatArray array, jfloat *elems, jint mode) {
    static_cast<Derived *>(this)->FormatJNIArguments("ReleaseFloatArrayElements", context, array, elems, mode);
  }

  void ReleaseDoubleArrayElements(TraceInvokeContext<void> &context, jdoubleArray array, jdouble *elems, jint mode) {
    static_cast<Derived *>(this)->FormatJNIArguments("ReleaseDoubleArrayElements", context, array, elems, mode);
  }

  void GetBooleanArrayRegion(TraceInvokeContext<void> &context, jbooleanArray array, jsize start, jsize len,
                             jboolean *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetBooleanArrayRegion", context, array, start, len, buf);
  }

  void GetByteArrayRegion(TraceInvokeContext<void> &context, jbyteArray array, jsize start, jsize len, jbyte *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetByteArrayRegion", context, array, start, len, buf);
  }

  void GetCharArrayRegion(TraceInvokeContext<void> &context, jcharArray array, jsize start, jsize len, jchar *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetCharArrayRegion", context, array, start, len, buf);
  }

  void GetShortArrayRegion(TraceInvokeContext<void> &context, jshortArray array, jsize start, jsize len, jshort *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetShortArrayRegion", context, array, start, len, buf);
  }

  void GetIntArrayRegion(TraceInvokeContext<void> &context, jintArray array, jsize start, jsize len, jint *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetIntArrayRegion", context, array, start, len, buf);
  }

  void GetLongArrayRegion(TraceInvokeContext<void> &context, jlongArray array, jsize start, jsize len, jlong *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetLongArrayRegion", context, array, start, len, buf);
  }

  void GetFloatArrayRegion(TraceInvokeContext<void> &context, jfloatArray array, jsize start, jsize len, jfloat *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetFloatArrayRegion", context, array, start, len, buf);
  }

  void GetDoubleArrayRegion(TraceInvokeContext<void> &context, jdoubleArray array, jsize start, jsize len,
                            jdouble *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetDoubleArrayRegion", context, array, start, len, buf);
  }

  void SetBooleanArrayRegion(TraceInvokeContext<void> &context, jbooleanArray array, jsize start, jsize len,
                             const jboolean *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetBooleanArrayRegion", context, array, start, len, buf);
  }

  void SetByteArrayRegion(TraceInvokeContext<void> &context, jbyteArray array, jsize start, jsize len,
                          const jbyte *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetByteArrayRegion", context, array, start, len, buf);
  }

  void SetCharArrayRegion(TraceInvokeContext<void> &context, jcharArray array, jsize start, jsize len,
                          const jchar *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetCharArrayRegion", context, array, start, len, buf);
  }

  void SetShortArrayRegion(TraceInvokeContext<void> &context, jshortArray array, jsize start, jsize len,
                           const jshort *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetShortArrayRegion", context, array, start, len, buf);
  }

  void SetIntArrayRegion(TraceInvokeContext<void> &context, jintArray array, jsize start, jsize len, const jint *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetIntArrayRegion", context, array, start, len, buf);
  }

  void SetLongArrayRegion(TraceInvokeContext<void> &context, jlongArray array, jsize start, jsize len,
                          const jlong *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetLongArrayRegion", context, array, start, len, buf);
  }

  void SetFloatArrayRegion(TraceInvokeContext<void> &context, jfloatArray array, jsize start, jsize len,
                           const jfloat *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetFloatArrayRegion", context, array, start, len, buf);
  }

  void SetDoubleArrayRegion(TraceInvokeContext<void> &context, jdoubleArray array, jsize start, jsize len,
                            const jdouble *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("SetDoubleArrayRegion", context, array, start, len, buf);
  }

  void RegisterNatives(TraceInvokeContext<jint> &context, jclass clazz, const JNINativeMethod *methods, jint nMethods) {
    static_cast<Derived *>(this)->TraceStart(context, "RegisterNatives");
    static_cast<Derived *>(this)->FormatJNIArgument(context, 0, clazz);
    for (jint i = 0; i < nMethods; ++i) {
      static_cast<Derived *>(this)->TraceLine(context, "    name: %s, signature: %s, fnPtr: %p", methods[i].name,
                                              methods[i].signature, methods[i].fnPtr);
    }
    static_cast<Derived *>(this)->FormatReturnValue(context);
    static_cast<Derived *>(this)->TraceEnd(context);
  }

  void UnregisterNatives(TraceInvokeContext<jint> &context, jclass clazz) {
    static_cast<Derived *>(this)->FormatJNIArguments("UnregisterNatives", context, clazz);
  }

  void MonitorEnter(TraceInvokeContext<jint> &context, jobject obj) {
    static_cast<Derived *>(this)->FormatJNIArguments("MonitorEnter", context, obj);
  }

  void MonitorExit(TraceInvokeContext<jint> &context, jobject obj) {
    static_cast<Derived *>(this)->FormatJNIArguments("MonitorExit", context, obj);
  }

  void GetJavaVM(TraceInvokeContext<jint> &context, JavaVM **vm) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetJavaVM", context, vm);
  }

  void GetStringRegion(TraceInvokeContext<void> &context, jstring str, jsize start, jsize len, jchar *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStringRegion", context, str, start, len, buf);
  }

  void GetStringUTFRegion(TraceInvokeContext<void> &context, jstring str, jsize start, jsize len, char *buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStringUTFRegion", context, str, start, len, buf);
  }

  void GetPrimitiveArrayCritical(TraceInvokeContext<void *> &context, jarray array, jboolean *isCopy) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetPrimitiveArrayCritical", context, array, isCopy);
  }

  void ReleasePrimitiveArrayCritical(TraceInvokeContext<void> &context, jarray array, void *carray, jint mode) {
    static_cast<Derived *>(this)->FormatJNIArguments("ReleasePrimitiveArrayCritical", context, array, carray, mode);
  }

  void GetStringCritical(TraceInvokeContext<const jchar *> &context, jstring string, jboolean *isCopy) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetStringCritical", context, string, isCopy);
  }

  void ReleaseStringCritical(TraceInvokeContext<void> &context, jstring string, const jchar *carray) {
    static_cast<Derived *>(this)->FormatJNIArguments("ReleaseStringCritical", context, string, carray);
  }

  void NewWeakGlobalRef(TraceInvokeContext<jweak> &context, jobject obj) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewWeakGlobalRef", context, obj);
  }

  void DeleteWeakGlobalRef(TraceInvokeContext<void> &context, jweak obj) {
    static_cast<Derived *>(this)->FormatJNIArguments("DeleteWeakGlobalRef", context, obj);
  }

  void ExceptionCheck(TraceInvokeContext<jboolean> &context) {
    static_cast<Derived *>(this)->FormatJNIArguments("ExceptionCheck", context);
  }

  void NewDirectByteBuffer(TraceInvokeContext<jobject> &context, void *address, jlong capacity) {
    static_cast<Derived *>(this)->FormatJNIArguments("NewDirectByteBuffer", context, address, capacity);
  }

  void GetDirectBufferAddress(TraceInvokeContext<void *> &context, jobject buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetDirectBufferAddress", context, buf);
  }

  void GetDirectBufferCapacity(TraceInvokeContext<jlong> &context, jobject buf) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetDirectBufferCapacity", context, buf);
  }

  /* added in JNI 1.6 */
  void GetObjectRefType(TraceInvokeContext<jobjectRefType> &context, jobject obj) {
    static_cast<Derived *>(this)->FormatJNIArguments("GetObjectRefType", context, obj);
  }

public:
  JNIEnv *original_env = nullptr;

protected:
  bool strict_mode_;
  JNIMonitor<Derived> jni_monitor_;

private:
  SymbolResolver symbol_resolver_;
  std::map<jmethodID, std::string> cache_methods_;
  std::map<jfieldID, std::string> cache_fields_;
  jmethodID last_method_ = nullptr;
  const char *last_method_desc_ = nullptr;
  jfieldID last_field_ = nullptr;
  const char *last_field_desc_ = nullptr;
};

} // namespace fakelinker
