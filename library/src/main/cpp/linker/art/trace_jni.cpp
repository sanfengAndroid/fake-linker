//
// Created by beichen on 2025/4/25.
//

#include <fakelinker/trace_jni.h>

#include <cinttypes>

#include <fakelinker/jni_helper.h>
#include <fakelinker/proxy_jni.h>
#include <fakelinker/type.h>

#include "../linker/linker_globals.h"
#include "hook_jni_native_interface_impl.h"


namespace fakelinker {

static JNINativeInterface backup_jni;
static JNINativeInterface *org_jni = &backup_jni;
static JNINativeInterface hook_jni;
static JNIMonitor *monitor = nullptr;
static BaseTraceJNICallback *callback = nullptr;
static JNIEnv org_env{.functions = &backup_jni};

struct ScopedVAArgs {
  explicit ScopedVAArgs(va_list *args) : args(args) {}
  ScopedVAArgs(const ScopedVAArgs &) = delete;
  ScopedVAArgs(ScopedVAArgs &&) = delete;
  ~ScopedVAArgs() { va_end(*args); }

private:
  va_list *args;
};

#define MONITOR_CALL(name, ...)                                                                                        \
  auto result = org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__);                                        \
  if (monitor->IsMonitoring(__builtin_return_address(0))) {                                                            \
    TraceInvokeContext<return_type_object_trait_t<decltype(&JNIEnv::name)>> context(                                   \
      result, reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                                          \
    callback->name(context, ##__VA_ARGS__);                                                                            \
    return context.result;                                                                                             \
  }                                                                                                                    \
  return result;

#define MONITOR_CALL_VA_LIST(name, methodID, args, ...)                                                                \
  va_list args_copy;                                                                                                   \
  va_copy(args_copy, args);                                                                                            \
  ScopedVAArgs scoped_args(&args_copy);                                                                                \
  auto result = org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__, args);                                  \
  if (monitor->IsMonitoring(__builtin_return_address(0))) {                                                            \
    TraceInvokeContext<return_type_object_trait_t<decltype(&JNIEnv::name)>> context(                                   \
      result, reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                                          \
    context.method = methodID;                                                                                         \
    callback->name(context, ##__VA_ARGS__, args_copy);                                                                 \
    return context.result;                                                                                             \
  }                                                                                                                    \
  return result;

#define MONITOR_CALL_INVOKE(name, methodID, ...)                                                                       \
  auto result = org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__);                                        \
  if (monitor->IsMonitoring(__builtin_return_address(0))) {                                                            \
    TraceInvokeContext<return_type_object_trait_t<decltype(&JNIEnv::name)>> context(                                   \
      result, reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                                          \
    context.method = methodID;                                                                                         \
    callback->name(context, ##__VA_ARGS__);                                                                            \
    return context.result;                                                                                             \
  }                                                                                                                    \
  return result;


#define MONITOR_CALL_INVALID_ARGS(name, ...)                                                                           \
  auto result = org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__);                                        \
  if (monitor->IsMonitoring(__builtin_return_address(0))) {                                                            \
    TraceInvokeContext<return_type_object_trait_t<decltype(&JNIEnv::name)>, false> context(                            \
      result, reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                                          \
    callback->name(context, ##__VA_ARGS__);                                                                            \
    return context.result;                                                                                             \
  }                                                                                                                    \
  return result;


#define MONITOR_VOID_CALL(name, ...)                                                                                   \
  org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__);                                                      \
  if (monitor->IsMonitoring(__builtin_return_address(0))) {                                                            \
    TraceInvokeContext<void> context(reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                   \
    callback->name(context, ##__VA_ARGS__);                                                                            \
  }

#define MONITOR_VOID_CALL_INVOKE(name, methodID, ...)                                                                  \
  org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__);                                                      \
  if (monitor->IsMonitoring(__builtin_return_address(0))) {                                                            \
    TraceInvokeContext<void> context(reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                   \
    context.method = methodID;                                                                                         \
    callback->name(context, ##__VA_ARGS__);                                                                            \
  }

#define MONITOR_VOID_CALL_VA_LIST(name, methodID, args, ...)                                                           \
  va_list args_copy;                                                                                                   \
  va_copy(args_copy, args);                                                                                            \
  ScopedVAArgs scoped_args(&args_copy);                                                                                \
  org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__, args);                                                \
  if (monitor->IsMonitoring(__builtin_return_address(0))) {                                                            \
    TraceInvokeContext<void> context(reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                   \
    context.method = methodID;                                                                                         \
    callback->name(context, ##__VA_ARGS__, args_copy);                                                                 \
  }

#define MONITOR_VOID_CALL_AFTER(name, ...)                                                                             \
  if (monitor->IsMonitoring(__builtin_return_address(0))) {                                                            \
    TraceInvokeContext<void> context(reinterpret_cast<JNIEnv *>(this), __builtin_return_address(0));                   \
    callback->name(context, ##__VA_ARGS__);                                                                            \
  }                                                                                                                    \
  org_jni->name(reinterpret_cast<JNIEnv *>(this), ##__VA_ARGS__);


class HookJNIEnv {
public:
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

private:
  HookJNIEnv() = default;
};

#undef MONITOR_CALL
#undef MONITOR_VOID_CALL


const char *BaseTraceJNICallback::FormatMethodID(JNIEnv *env, jmethodID method, bool check) {
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

const char *BaseTraceJNICallback::FormatFieldID(JNIEnv *env, jfieldID field, bool check) {
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

std::string BaseTraceJNICallback::GetMethodShorty(JNIEnv *env, jmethodID method, bool check) {
  if (check && !method) {
    return "(null method)";
  }
  return JNIHelper::GetMethodShorty(env, method, check);
}

JNIMonitor &JNIMonitor::Get() {
  static JNIMonitor singleton;
  return singleton;
}

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

bool JNIMonitor::InitHookJNI(JNIEnv *env) {
#define BIND_JNI_FUN(name) hook_jni.name = ForceConvertFuncPtr<decltype(JNINativeInterface::name)>(&HookJNIEnv::name)
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
  JNIHelper::Init(env);
  memcpy(&backup_jni, env->functions, sizeof(JNINativeInterface));
  ProxyJNIEnv::SetBackupFunctions(&backup_jni);
  Get().initialized_ = true;
  org_jni = &backup_jni;
  return true;
}

bool JNIMonitor::AddTraceFunction(size_t offset) {
  if (offset < offsetof(JNINativeInterface, GetVersion) || offset > offsetof(JNINativeInterface, GetObjectRefType)) {
    return false;
  }
  trace_offsets_.push_back(offset);
  return true;
}

bool JNIMonitor::StartTrace(BaseTraceJNICallback *trace_callback) {
  if (trace_offsets_.empty()) {
    return false;
  }
  monitor = this;
  trace_callback->SetOriginalEnv(&org_env);
  callback = trace_callback;

  std::vector<HookJniUnit> hooks;
  for (auto offset : trace_offsets_) {
    hooks.push_back(
      HookJniUnit{.offset = static_cast<int>(offset),
                  .hook_method = reinterpret_cast<void *>(*reinterpret_cast<uintptr_t *>(((char *)&hook_jni + offset))),
                  .backup_method = nullptr});
  }
  bool res = HookJniNativeInterfaces(&hooks[0], static_cast<int>(hooks.size()));
  trace_offsets_.clear();
  return res;
}

bool JNIMonitor::AddLibraryMonitor(std::string_view name, bool exclude) {
  soinfo *target = ProxyLinker::Get().FindSoinfoByName(name.data());
  if (!target) {
    LOGE("find soinfo failed: %s", name.data());
    return false;
  }
  return AddAddressMonitor(static_cast<uintptr_t>(target->base()),
                           static_cast<uintptr_t>(target->base() + target->size()), exclude);
}

bool JNIMonitor::RemoveLibraryMonitor(std::string_view name) {
  soinfo *target = ProxyLinker::Get().FindSoinfoByName(name.data());
  if (!target) {
    LOGE("find soinfo failed: %s", name.data());
    return false;
  }
  return RemoveAddressMonitor(static_cast<uintptr_t>(target->base()),
                              static_cast<uintptr_t>(target->base() + target->size()));
}

bool JNIMonitor::AddAddressMonitor(uintptr_t start, uintptr_t end, bool exclude) {
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

bool JNIMonitor::RemoveAddressMonitor(uintptr_t start, uintptr_t end) {
  auto itr = monitors_.find(start);
  if (itr != monitors_.end() && itr->second == end) {
    monitors_.erase(itr);
    return true;
  }
  return false;
}

bool JNIMonitor::IsMonitoring(uintptr_t addr) {
  auto itr = monitors_.upper_bound(addr);
  if (itr == monitors_.begin()) {
    return false;
  }
  --itr;
  return addr <= itr->second && !exclude_;
}

} // namespace fakelinker