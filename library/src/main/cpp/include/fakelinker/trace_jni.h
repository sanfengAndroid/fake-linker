//
// Created by beichen on 2025/4/25.
//
#pragma once

#include <jni.h>

#include <map>
#include <string_view>
#include <type_traits>
#include <vector>

#include "jni_helper.h"
#include "macros.h"


namespace fakelinker {

template <typename T, typename = void>
struct _is_valid_container : std::false_type {};

template <typename T>
struct _is_valid_container<
  T, std::void_t<decltype(std::declval<T>().begin()), decltype(std::declval<T>().end()), typename T::value_type>>
    : std::is_same<typename T::value_type, size_t> {};


template <typename jtype, bool AllowAccessArgs = true>
struct TraceInvokeContext {
  // JNIEnv 对象
  JNIEnv *env;
  // 调用者的地址
  void *caller;
  // 如果有返回值,则是
  jtype result;
  // 当有可变参数 va_list 时则为具体的方法
  jmethodID method;
  // 保存消息
  char message[4096];
  uint32_t message_length = 0;
  TraceInvokeContext(jtype result, JNIEnv *env, void *caller) :
      env(env), caller(caller), result(result), method(nullptr) {}
};

template <>
struct TraceInvokeContext<void, true> {
  // JNIEnv 对象
  JNIEnv *env;
  // 调用者的地址
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

class BaseTraceJNICallback {
public:
  BaseTraceJNICallback(bool strict = true) : strict_mode_(strict) {}
  virtual ~BaseTraceJNICallback() = default;

  virtual void TraceLog(std::string_view message) = 0;

  const char *FormatMethodID(JNIEnv *env, jmethodID method, bool check = true);

  const char *FormatFieldID(JNIEnv *env, jfieldID field, bool check = true);

  std::string GetMethodShorty(JNIEnv *env, jmethodID method, bool check = true);

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

  // JNI Trace

  virtual void GetVersion(TraceInvokeContext<jint> &context) {}

  virtual void DefineClass(TraceInvokeContext<jclass> &context, const char *name, jobject loader, const jbyte *buf,
                           jsize bufLen) {}

  virtual void FindClass(TraceInvokeContext<jclass> &context, const char *name) {}

  virtual void FromReflectedMethod(TraceInvokeContext<jmethodID> &context, jobject method) {}

  virtual void FromReflectedField(TraceInvokeContext<jfieldID> &context, jobject field) {}

  virtual void ToReflectedMethod(TraceInvokeContext<jobject> &context, jclass cls, jmethodID methodID,
                                 jboolean isStatic) {}

  virtual void GetSuperclass(TraceInvokeContext<jclass> &context, jclass clazz) {}

  virtual void IsAssignableFrom(TraceInvokeContext<jboolean> &context, jclass from_clazz, jclass to_clazz) {}

  virtual void ToReflectedField(TraceInvokeContext<jobject> &context, jclass cls, jfieldID fieldID, jboolean isStatic) {
  }

  virtual void Throw(TraceInvokeContext<jint> &context, jthrowable obj) {}

  virtual void ThrowNew(TraceInvokeContext<jint> &context, jclass clazz, const char *message) {}

  virtual void ExceptionOccurred(TraceInvokeContext<jthrowable> &context) {}

  virtual void ExceptionDescribe(TraceInvokeContext<void> &context) {}

  virtual void ExceptionClear(TraceInvokeContext<void> &context) {}

  virtual void FatalError(TraceInvokeContext<void> &context, const char *msg) {}

  virtual void PushLocalFrame(TraceInvokeContext<jint> &context, jint capacity) {}

  virtual void PopLocalFrame(TraceInvokeContext<jobject, false> &context, jobject local) {}

  virtual void NewGlobalRef(TraceInvokeContext<jobject> &context, jobject obj) {}

  virtual void DeleteGlobalRef(TraceInvokeContext<void> &context, jobject globalRef) {}

  virtual void DeleteLocalRef(TraceInvokeContext<void> &context, jobject localRef) {}

  virtual void IsSameObject(TraceInvokeContext<jboolean> &context, jobject ref1, jobject ref2) {}

  virtual void NewLocalRef(TraceInvokeContext<jobject> &context, jobject ref) {}

  virtual void EnsureLocalCapacity(TraceInvokeContext<jint> &context, jint capacity) {}

  virtual void AllocObject(TraceInvokeContext<jobject> &context, jclass clazz) {}

  virtual void NewObjectV(TraceInvokeContext<jobject> &context, jclass clazz, jmethodID methodID, va_list args) {}

  virtual void NewObjectA(TraceInvokeContext<jobject> &context, jclass clazz, jmethodID methodID, const jvalue *args) {}

  virtual void GetObjectClass(TraceInvokeContext<jclass> &context, jobject obj) {}

  virtual void IsInstanceOf(TraceInvokeContext<jboolean> &context, jobject obj, jclass clazz) {}

  virtual void GetMethodID(TraceInvokeContext<jmethodID> &context, jclass clazz, const char *name, const char *sig) {}


#define TRACE_CALL_TYPE_METHODV(_jtype, _jname)                                                                        \
  virtual void Call##_jname##MethodV(TraceInvokeContext<_jtype> &context, jobject obj, jmethodID methodID,             \
                                     va_list args) {}

#define TRACE_CALL_TYPE_METHODA(_jtype, _jname)                                                                        \
  virtual void Call##_jname##MethodA(TraceInvokeContext<_jtype> &context, jobject obj, jmethodID methodID,             \
                                     const jvalue *args) {}

#define TRACE_CALL_TYPE(_jtype, _jname)                                                                                \
  TRACE_CALL_TYPE_METHODV(_jtype, _jname)                                                                              \
  TRACE_CALL_TYPE_METHODA(_jtype, _jname)

  TRACE_CALL_TYPE(jobject, Object)

  TRACE_CALL_TYPE(jboolean, Boolean)

  TRACE_CALL_TYPE(jbyte, Byte)

  TRACE_CALL_TYPE(jchar, Char)

  TRACE_CALL_TYPE(jshort, Short)

  TRACE_CALL_TYPE(jint, Int)

  TRACE_CALL_TYPE(jlong, Long)

  TRACE_CALL_TYPE(jfloat, Float)

  TRACE_CALL_TYPE(jdouble, Double)

#undef TRACE_CALL_TYPE
#undef TRACE_CALL_TYPE_METHODV
#undef TRACE_CALL_TYPE_METHODA

  virtual void CallVoidMethodV(TraceInvokeContext<void> &context, jobject obj, jmethodID methodID, va_list args) {}

  virtual void CallVoidMethodA(TraceInvokeContext<void> &context, jobject obj, jmethodID methodID, const jvalue *args) {
  }


#define TRACE_CALL_NONVIRT_TYPE_METHODV(_jtype, _jname)                                                                \
  virtual void CallNonvirtual##_jname##MethodV(TraceInvokeContext<_jtype> &context, jobject obj, jclass clazz,         \
                                               jmethodID methodID, va_list args) {}

#define TRACE_CALL_NONVIRT_TYPE_METHODA(_jtype, _jname)                                                                \
  virtual void CallNonvirtual##_jname##MethodA(TraceInvokeContext<_jtype> &context, jobject obj, jclass clazz,         \
                                               jmethodID methodID, const jvalue *args) {}

#define TRACE_CALL_NONVIRT_TYPE(_jtype, _jname)                                                                        \
  TRACE_CALL_NONVIRT_TYPE_METHODV(_jtype, _jname)                                                                      \
  TRACE_CALL_NONVIRT_TYPE_METHODA(_jtype, _jname)

  TRACE_CALL_NONVIRT_TYPE(jobject, Object)

  TRACE_CALL_NONVIRT_TYPE(jboolean, Boolean)

  TRACE_CALL_NONVIRT_TYPE(jbyte, Byte)

  TRACE_CALL_NONVIRT_TYPE(jchar, Char)

  TRACE_CALL_NONVIRT_TYPE(jshort, Short)

  TRACE_CALL_NONVIRT_TYPE(jint, Int)

  TRACE_CALL_NONVIRT_TYPE(jlong, Long)

  TRACE_CALL_NONVIRT_TYPE(jfloat, Float)

  TRACE_CALL_NONVIRT_TYPE(jdouble, Double)

#undef TRACE_CALL_NONVIRT_TYPE
#undef TRACE_CALL_NONVIRT_TYPE_METHODV
#undef TRACE_CALL_NONVIRT_TYPE_METHODA

  virtual void CallNonvirtualVoidMethodV(TraceInvokeContext<void> &context, jobject obj, jclass clazz,
                                         jmethodID methodID, va_list args) {}

  virtual void CallNonvirtualVoidMethodA(TraceInvokeContext<void> &context, jobject obj, jclass clazz,
                                         jmethodID methodID, const jvalue *args) {}

  virtual void GetFieldID(TraceInvokeContext<jfieldID> &context, jclass clazz, const char *name, const char *sig) {}

  virtual void GetObjectField(TraceInvokeContext<jobject> &context, jobject obj, jfieldID fieldID) {}

  virtual void GetBooleanField(TraceInvokeContext<jboolean> &context, jobject obj, jfieldID fieldID) {}

  virtual void GetByteField(TraceInvokeContext<jbyte> &context, jobject obj, jfieldID fieldID) {}

  virtual void GetCharField(TraceInvokeContext<jchar> &context, jobject obj, jfieldID fieldID) {}

  virtual void GetShortField(TraceInvokeContext<jshort> &context, jobject obj, jfieldID fieldID) {}

  virtual void GetIntField(TraceInvokeContext<jint> &context, jobject obj, jfieldID fieldID) {}

  virtual void GetLongField(TraceInvokeContext<jlong> &context, jobject obj, jfieldID fieldID) {}

  virtual void GetFloatField(TraceInvokeContext<jfloat> &context, jobject obj, jfieldID fieldID) {}

  virtual void GetDoubleField(TraceInvokeContext<jdouble> &context, jobject obj, jfieldID fieldID) {}

  virtual void SetObjectField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jobject value) {}

  virtual void SetBooleanField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jboolean value) {}

  virtual void SetByteField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jbyte value) {}

  virtual void SetCharField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jchar value) {}

  virtual void SetShortField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jshort value) {}

  virtual void SetIntField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jint value) {}

  virtual void SetLongField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jlong value) {}

  virtual void SetFloatField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jfloat value) {}

  virtual void SetDoubleField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jdouble value) {}

  virtual void GetStaticMethodID(TraceInvokeContext<jmethodID> &context, jclass clazz, const char *name,
                                 const char *sig) {}


#define TRACE_CALL_STATIC_TYPE_METHODV(_jtype, _jname)                                                                 \
  virtual void CallStatic##_jname##MethodV(TraceInvokeContext<_jtype> &context, jclass clazz, jmethodID methodID,      \
                                           va_list args) {}
#define TRACE_CALL_STATIC_TYPE_METHODA(_jtype, _jname)                                                                 \
  virtual void CallStatic##_jname##MethodA(TraceInvokeContext<_jtype> &context, jclass clazz, jmethodID methodID,      \
                                           const jvalue *args) {}

#define TRACE_CALL_STATIC_TYPE(_jtype, _jname)                                                                         \
  TRACE_CALL_STATIC_TYPE_METHODV(_jtype, _jname)                                                                       \
  TRACE_CALL_STATIC_TYPE_METHODA(_jtype, _jname)

  TRACE_CALL_STATIC_TYPE(jobject, Object)

  TRACE_CALL_STATIC_TYPE(jboolean, Boolean)

  TRACE_CALL_STATIC_TYPE(jbyte, Byte)

  TRACE_CALL_STATIC_TYPE(jchar, Char)

  TRACE_CALL_STATIC_TYPE(jshort, Short)

  TRACE_CALL_STATIC_TYPE(jint, Int)

  TRACE_CALL_STATIC_TYPE(jlong, Long)

  TRACE_CALL_STATIC_TYPE(jfloat, Float)

  TRACE_CALL_STATIC_TYPE(jdouble, Double)

#undef TRACE_CALL_STATIC_TYPE
#undef TRACE_CALL_STATIC_TYPE_METHODV
#undef TRACE_CALL_STATIC_TYPE_METHODA

  virtual void CallStaticVoidMethodV(TraceInvokeContext<void> &context, jclass clazz, jmethodID methodID,
                                     va_list args) {}

  virtual void CallStaticVoidMethodA(TraceInvokeContext<void> &context, jclass clazz, jmethodID methodID,
                                     const jvalue *args) {}

  virtual void GetStaticFieldID(TraceInvokeContext<jfieldID> &context, jclass clazz, const char *name,
                                const char *sig) {}

  virtual void GetStaticObjectField(TraceInvokeContext<jobject> &context, jclass clazz, jfieldID fieldID) {}

  virtual void GetStaticBooleanField(TraceInvokeContext<jboolean> &context, jclass clazz, jfieldID fieldID) {}

  virtual void GetStaticByteField(TraceInvokeContext<jbyte> &context, jclass clazz, jfieldID fieldID) {}

  virtual void GetStaticCharField(TraceInvokeContext<jchar> &context, jclass clazz, jfieldID fieldID) {}

  virtual void GetStaticShortField(TraceInvokeContext<jshort> &context, jclass clazz, jfieldID fieldID) {}

  virtual void GetStaticIntField(TraceInvokeContext<jint> &context, jclass clazz, jfieldID fieldID) {}

  virtual void GetStaticLongField(TraceInvokeContext<jlong> &context, jclass clazz, jfieldID fieldID) {}

  virtual void GetStaticFloatField(TraceInvokeContext<jfloat> &context, jclass clazz, jfieldID fieldID) {}

  virtual void GetStaticDoubleField(TraceInvokeContext<jdouble> &context, jclass clazz, jfieldID fieldID) {}

  virtual void SetStaticObjectField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jobject value) {}

  virtual void SetStaticBooleanField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID,
                                     jboolean value) {}

  virtual void SetStaticByteField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jbyte value) {}

  virtual void SetStaticCharField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jchar value) {}

  virtual void SetStaticShortField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jshort value) {}

  virtual void SetStaticIntField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jint value) {}

  virtual void SetStaticLongField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jlong value) {}

  virtual void SetStaticFloatField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jfloat value) {}

  virtual void SetStaticDoubleField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jdouble value) {}

  virtual void NewString(TraceInvokeContext<jstring> &context, const jchar *unicodeChars, jsize len) {}

  virtual void GetStringLength(TraceInvokeContext<jsize> &context, jstring string) {}

  virtual void GetStringChars(TraceInvokeContext<const jchar *> &context, jstring string, jboolean *isCopy) {}

  virtual void ReleaseStringChars(TraceInvokeContext<void> &context, jstring string, const jchar *chars) {}

  virtual void NewStringUTF(TraceInvokeContext<jstring> &context, const char *bytes) {}

  virtual void GetStringUTFLength(TraceInvokeContext<jsize> &context, jstring string) {}

  virtual void GetStringUTFChars(TraceInvokeContext<const char *> &context, jstring string, jboolean *isCopy) {}

  virtual void ReleaseStringUTFChars(TraceInvokeContext<void> &context, jstring string, const char *utf) {}

  virtual void GetArrayLength(TraceInvokeContext<jsize> &context, jarray array) {}

  virtual void NewObjectArray(TraceInvokeContext<jobjectArray> &context, jsize length, jclass elementClass,
                              jobject initialElement) {}

  virtual void GetObjectArrayElement(TraceInvokeContext<jobject> &context, jobjectArray array, jsize index) {}

  virtual void SetObjectArrayElement(TraceInvokeContext<void> &context, jobjectArray array, jsize index,
                                     jobject value) {}

  virtual void NewBooleanArray(TraceInvokeContext<jbooleanArray> &context, jsize length) {}

  virtual void NewByteArray(TraceInvokeContext<jbyteArray> &context, jsize length) {}

  virtual void NewCharArray(TraceInvokeContext<jcharArray> &context, jsize length) {}

  virtual void NewShortArray(TraceInvokeContext<jshortArray> &context, jsize length) {}

  virtual void NewIntArray(TraceInvokeContext<jintArray> &context, jsize length) {}

  virtual void NewLongArray(TraceInvokeContext<jlongArray> &context, jsize length) {}

  virtual void NewFloatArray(TraceInvokeContext<jfloatArray> &context, jsize length) {}

  virtual void NewDoubleArray(TraceInvokeContext<jdoubleArray> &context, jsize length) {}

  virtual void GetBooleanArrayElements(TraceInvokeContext<jboolean *> &context, jbooleanArray array, jboolean *isCopy) {
  }

  virtual void GetByteArrayElements(TraceInvokeContext<jbyte *> &context, jbyteArray array, jboolean *isCopy) {}

  virtual void GetCharArrayElements(TraceInvokeContext<jchar *> &context, jcharArray array, jboolean *isCopy) {}

  virtual void GetShortArrayElements(TraceInvokeContext<jshort *> &context, jshortArray array, jboolean *isCopy) {}

  virtual void GetIntArrayElements(TraceInvokeContext<jint *> &context, jintArray array, jboolean *isCopy) {}

  virtual void GetLongArrayElements(TraceInvokeContext<jlong *> &context, jlongArray array, jboolean *isCopy) {}

  virtual void GetFloatArrayElements(TraceInvokeContext<jfloat *> &context, jfloatArray array, jboolean *isCopy) {}

  virtual void GetDoubleArrayElements(TraceInvokeContext<jdouble *> &context, jdoubleArray array, jboolean *isCopy) {}

  virtual void ReleaseBooleanArrayElements(TraceInvokeContext<void> &context, jbooleanArray array, jboolean *elems,
                                           jint mode) {}

  virtual void ReleaseByteArrayElements(TraceInvokeContext<void> &context, jbyteArray array, jbyte *elems, jint mode) {}

  virtual void ReleaseCharArrayElements(TraceInvokeContext<void> &context, jcharArray array, jchar *elems, jint mode) {}

  virtual void ReleaseShortArrayElements(TraceInvokeContext<void> &context, jshortArray array, jshort *elems,
                                         jint mode) {}

  virtual void ReleaseIntArrayElements(TraceInvokeContext<void> &context, jintArray array, jint *elems, jint mode) {}

  virtual void ReleaseLongArrayElements(TraceInvokeContext<void> &context, jlongArray array, jlong *elems, jint mode) {}

  virtual void ReleaseFloatArrayElements(TraceInvokeContext<void> &context, jfloatArray array, jfloat *elems,
                                         jint mode) {}

  virtual void ReleaseDoubleArrayElements(TraceInvokeContext<void> &context, jdoubleArray array, jdouble *elems,
                                          jint mode) {}

  virtual void GetBooleanArrayRegion(TraceInvokeContext<void> &context, jbooleanArray array, jsize start, jsize len,
                                     jboolean *buf) {}

  virtual void GetByteArrayRegion(TraceInvokeContext<void> &context, jbyteArray array, jsize start, jsize len,
                                  jbyte *buf) {}

  virtual void GetCharArrayRegion(TraceInvokeContext<void> &context, jcharArray array, jsize start, jsize len,
                                  jchar *buf) {}

  virtual void GetShortArrayRegion(TraceInvokeContext<void> &context, jshortArray array, jsize start, jsize len,
                                   jshort *buf) {}

  virtual void GetIntArrayRegion(TraceInvokeContext<void> &context, jintArray array, jsize start, jsize len,
                                 jint *buf) {}

  virtual void GetLongArrayRegion(TraceInvokeContext<void> &context, jlongArray array, jsize start, jsize len,
                                  jlong *buf) {}

  virtual void GetFloatArrayRegion(TraceInvokeContext<void> &context, jfloatArray array, jsize start, jsize len,
                                   jfloat *buf) {}

  virtual void GetDoubleArrayRegion(TraceInvokeContext<void> &context, jdoubleArray array, jsize start, jsize len,
                                    jdouble *buf) {}

  virtual void SetBooleanArrayRegion(TraceInvokeContext<void> &context, jbooleanArray array, jsize start, jsize len,
                                     const jboolean *buf) {}

  virtual void SetByteArrayRegion(TraceInvokeContext<void> &context, jbyteArray array, jsize start, jsize len,
                                  const jbyte *buf) {}

  virtual void SetCharArrayRegion(TraceInvokeContext<void> &context, jcharArray array, jsize start, jsize len,
                                  const jchar *buf) {}

  virtual void SetShortArrayRegion(TraceInvokeContext<void> &context, jshortArray array, jsize start, jsize len,
                                   const jshort *buf) {}

  virtual void SetIntArrayRegion(TraceInvokeContext<void> &context, jintArray array, jsize start, jsize len,
                                 const jint *buf) {}

  virtual void SetLongArrayRegion(TraceInvokeContext<void> &context, jlongArray array, jsize start, jsize len,
                                  const jlong *buf) {}

  virtual void SetFloatArrayRegion(TraceInvokeContext<void> &context, jfloatArray array, jsize start, jsize len,
                                   const jfloat *buf) {}

  virtual void SetDoubleArrayRegion(TraceInvokeContext<void> &context, jdoubleArray array, jsize start, jsize len,
                                    const jdouble *buf) {}

  virtual void RegisterNatives(TraceInvokeContext<jint> &context, jclass clazz, const JNINativeMethod *methods,
                               jint nMethods) {}

  virtual void UnregisterNatives(TraceInvokeContext<jint> &context, jclass clazz) {}

  virtual void MonitorEnter(TraceInvokeContext<jint> &context, jobject obj) {}

  virtual void MonitorExit(TraceInvokeContext<jint> &context, jobject obj) {}

  virtual void GetJavaVM(TraceInvokeContext<jint> &context, JavaVM **vm) {}

  virtual void GetStringRegion(TraceInvokeContext<void> &context, jstring str, jsize start, jsize len, jchar *buf) {}

  virtual void GetStringUTFRegion(TraceInvokeContext<void> &context, jstring str, jsize start, jsize len, char *buf) {}

  virtual void GetPrimitiveArrayCritical(TraceInvokeContext<void *> &context, jarray array, jboolean *isCopy) {}

  virtual void ReleasePrimitiveArrayCritical(TraceInvokeContext<void> &context, jarray array, void *carray, jint mode) {
  }

  virtual void GetStringCritical(TraceInvokeContext<const jchar *> &context, jstring string, jboolean *isCopy) {}

  virtual void ReleaseStringCritical(TraceInvokeContext<void> &context, jstring string, const jchar *carray) {}

  virtual void NewWeakGlobalRef(TraceInvokeContext<jweak> &context, jobject obj) {}

  virtual void DeleteWeakGlobalRef(TraceInvokeContext<void> &context, jweak obj) {}

  virtual void ExceptionCheck(TraceInvokeContext<jboolean> &context) {}

  virtual void NewDirectByteBuffer(TraceInvokeContext<jobject> &context, void *address, jlong capacity) {}

  virtual void GetDirectBufferAddress(TraceInvokeContext<void *> &context, jobject buf) {}

  virtual void GetDirectBufferCapacity(TraceInvokeContext<jlong> &context, jobject buf) {}

  /* added in JNI 1.6 */
  virtual void GetObjectRefType(TraceInvokeContext<jobjectRefType> &context, jobject obj) {}
  JNIEnv *original_env;

protected:
  bool strict_mode_;

private:
  std::map<jmethodID, std::string> cache_methods_;
  std::map<jfieldID, std::string> cache_fields_;
  jmethodID last_method_ = nullptr;
  const char *last_method_desc_ = nullptr;
  jfieldID last_field_ = nullptr;
  const char *last_field_desc_ = nullptr;
};


class JNIMonitor {
public:
  static JNIMonitor &Get();

  static bool InitHookJNI(JNIEnv *env);

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

  // 用户确保不重复
  bool AddTraceFunction(size_t offset);

  bool StartTrace(BaseTraceJNICallback *tarce_callback);

  bool AddLibraryMonitor(std::string_view name, bool exclude);

  bool RemoveLibraryMonitor(std::string_view name);

  bool AddAddressMonitor(uintptr_t start, uintptr_t end, bool exclude);

  bool RemoveAddressMonitor(uintptr_t start, uintptr_t end);

  bool IsMonitoring(uintptr_t addr);

  bool IsMonitoring(void *addr) { return IsMonitoring(reinterpret_cast<uintptr_t>(addr)); }

private:
  JNIMonitor() = default;

  std::vector<size_t> trace_offsets_;
  std::map<uintptr_t, uintptr_t> monitors_;
  bool exclude_ = true;
  uintptr_t last_start_ = 0;
  uintptr_t last_end_ = 0;
  bool initialized_ = false;
  DISALLOW_COPY_AND_ASSIGN(JNIMonitor);
};

} // namespace fakelinker