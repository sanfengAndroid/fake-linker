//
// Created by beich on 2020/12/20.
//

#pragma once

#include <jni.h>

extern "C" JNINativeInterface *original_functions;

#define PROXY_CALL(MN, ...) functions->MN != nullptr ? functions->MN(env_, ##__VA_ARGS__) : env_->MN(__VA_ARGS__)

namespace fakelinker {

class ProxyJNIEnv {
public:
  // JNI Proxy
  jint GetVersion() { return functions->GetVersion(env_); }

  jclass DefineClass(const char *name, jobject loader, const jbyte *buf, jsize bufLen) {
    return PROXY_CALL(DefineClass, name, loader, buf, bufLen);
  }

  jclass FindClass(const char *name) { return PROXY_CALL(FindClass, name); }

  jmethodID FromReflectedMethod(jobject method) { return PROXY_CALL(FromReflectedMethod, method); }

  jfieldID FromReflectedField(jobject field) { return PROXY_CALL(FromReflectedField, field); }

  jobject ToReflectedMethod(jclass cls, jmethodID methodID, jboolean isStatic) {
    return PROXY_CALL(ToReflectedMethod, cls, methodID, isStatic);
  }

  jclass GetSuperclass(jclass clazz) { return PROXY_CALL(GetSuperclass, clazz); }

  jboolean IsAssignableFrom(jclass from_clazz, jclass to_clazz) {
    return PROXY_CALL(IsAssignableFrom, from_clazz, to_clazz);
  }

  jobject ToReflectedField(jclass cls, jfieldID fieldID, jboolean isStatic) {
    return PROXY_CALL(ToReflectedField, cls, fieldID, isStatic);
  }

  jint Throw(jthrowable obj) { return PROXY_CALL(Throw, obj); }

  jint ThrowNew(jclass clazz, const char *message) { return PROXY_CALL(ThrowNew, clazz, message); }

  jthrowable ExceptionOccurred() { return PROXY_CALL(ExceptionOccurred); }

  void ExceptionDescribe() { return PROXY_CALL(ExceptionDescribe); }

  void ExceptionClear() { return PROXY_CALL(ExceptionClear); }

  void FatalError(const char *msg) { PROXY_CALL(FatalError, msg); }

  jint PushLocalFrame(jint capacity) { return PROXY_CALL(PushLocalFrame, capacity); }

  jobject PopLocalFrame(jobject result) { return PROXY_CALL(PopLocalFrame, result); }

  jobject NewGlobalRef(jobject obj) { return PROXY_CALL(NewGlobalRef, obj); }

  void DeleteGlobalRef(jobject globalRef) { return PROXY_CALL(DeleteGlobalRef, globalRef); }

  void DeleteLocalRef(jobject localRef) { return PROXY_CALL(DeleteLocalRef, localRef); }

  jboolean IsSameObject(jobject ref1, jobject ref2) { return PROXY_CALL(IsSameObject, ref1, ref2); }

  jobject NewLocalRef(jobject ref) { return PROXY_CALL(NewLocalRef, ref); }

  jint EnsureLocalCapacity(jint capacity) { return PROXY_CALL(EnsureLocalCapacity, capacity); }

  jobject AllocObject(jclass clazz) { return PROXY_CALL(AllocObject, clazz); }

  jobject NewObject(jclass clazz, jmethodID methodID, ...) {
    va_list args;
    va_start(args, methodID);
    jobject result = PROXY_CALL(NewObjectV, clazz, methodID, args);
    va_end(args);
    return result;
  }

  jobject NewObjectV(jclass clazz, jmethodID methodID, va_list args) {
    return PROXY_CALL(NewObjectV, clazz, methodID, args);
  }

  jobject NewObjectA(jclass clazz, jmethodID methodID, const jvalue *args) {
    return PROXY_CALL(NewObjectA, clazz, methodID, args);
  }

  jclass GetObjectClass(jobject obj) { return PROXY_CALL(GetObjectClass, obj); }

  jboolean IsInstanceOf(jobject obj, jclass clazz) { return PROXY_CALL(IsInstanceOf, obj, clazz); }

  jmethodID GetMethodID(jclass clazz, const char *name, const char *sig) {
    return PROXY_CALL(GetMethodID, clazz, name, sig);
  }

#define PROXY_CALL_TYPE_METHOD(_jtype, _jname)                                                                         \
  _jtype Call##_jname##Method(jobject obj, jmethodID methodID, ...) {                                                  \
    _jtype result;                                                                                                     \
    va_list args;                                                                                                      \
    va_start(args, methodID);                                                                                          \
    result = PROXY_CALL(Call##_jname##MethodV, obj, methodID, args);                                                   \
    va_end(args);                                                                                                      \
    return result;                                                                                                     \
  }

#define PROXY_CALL_TYPE_METHODV(_jtype, _jname)                                                                        \
  _jtype Call##_jname##MethodV(jobject obj, jmethodID methodID, va_list args) {                                        \
    return PROXY_CALL(Call##_jname##MethodV, obj, methodID, args);                                                     \
  }

#define PROXY_CALL_TYPE_METHODA(_jtype, _jname)                                                                        \
  _jtype Call##_jname##MethodA(jobject obj, jmethodID methodID, const jvalue *args) {                                  \
    return PROXY_CALL(Call##_jname##MethodA, obj, methodID, args);                                                     \
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

#undef PROXY_CALL_TYPE
#undef PROXY_CALL_TYPE_METHOD
#undef PROXY_CALL_TYPE_METHODV
#undef PROXY_CALL_TYPE_METHODA

  void CallVoidMethod(jobject obj, jmethodID methodID, ...) {
    va_list args;
    va_start(args, methodID);
    PROXY_CALL(CallVoidMethod, obj, methodID, args);
    va_end(args);
  }

  void CallVoidMethodV(jobject obj, jmethodID methodID, va_list args) {
    PROXY_CALL(CallVoidMethodV, obj, methodID, args);
  }

  void CallVoidMethodA(jobject obj, jmethodID methodID, const jvalue *args) {
    PROXY_CALL(CallVoidMethodA, obj, methodID, args);
  }

#define PROXY_CALL_NONVIRT_TYPE_METHOD(_jtype, _jname)                                                                 \
  _jtype CallNonvirtual##_jname##Method(jobject obj, jclass clazz, jmethodID methodID, ...) {                          \
    _jtype result;                                                                                                     \
    va_list args;                                                                                                      \
    va_start(args, methodID);                                                                                          \
    result = PROXY_CALL(CallNonvirtual##_jname##MethodV, obj, clazz, methodID, args);                                  \
    va_end(args);                                                                                                      \
    return result;                                                                                                     \
  }
#define PROXY_CALL_NONVIRT_TYPE_METHODV(_jtype, _jname)                                                                \
  _jtype CallNonvirtual##_jname##MethodV(jobject obj, jclass clazz, jmethodID methodID, va_list args) {                \
    return PROXY_CALL(CallNonvirtual##_jname##MethodV, obj, clazz, methodID, args);                                    \
  }

#define PROXY_CALL_NONVIRT_TYPE_METHODA(_jtype, _jname)                                                                \
  _jtype CallNonvirtual##_jname##MethodA(jobject obj, jclass clazz, jmethodID methodID, const jvalue *args) {          \
    return PROXY_CALL(CallNonvirtual##_jname##MethodA, obj, clazz, methodID, args);                                    \
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

#undef PROXY_CALL_NONVIRT_TYPE
#undef PROXY_CALL_NONVIRT_TYPE_METHOD
#undef PROXY_CALL_NONVIRT_TYPE_METHODV
#undef PROXY_CALL_NONVIRT_TYPE_METHODA

  void CallNonvirtualVoidMethod(jobject obj, jclass clazz, jmethodID methodID, ...) {
    va_list args;
    va_start(args, methodID);
    PROXY_CALL(CallNonvirtualVoidMethodV, obj, clazz, methodID, args);
    va_end(args);
  }

  void CallNonvirtualVoidMethodV(jobject obj, jclass clazz, jmethodID methodID, va_list args) {
    PROXY_CALL(CallNonvirtualVoidMethodV, obj, clazz, methodID, args);
  }

  void CallNonvirtualVoidMethodA(jobject obj, jclass clazz, jmethodID methodID, const jvalue *args) {
    PROXY_CALL(CallNonvirtualVoidMethodA, obj, clazz, methodID, args);
  }

  jfieldID GetFieldID(jclass clazz, const char *name, const char *sig) {
    return PROXY_CALL(GetFieldID, clazz, name, sig);
  }

  jobject GetObjectField(jobject obj, jfieldID fieldID) { return PROXY_CALL(GetObjectField, obj, fieldID); }

  jboolean GetBooleanField(jobject obj, jfieldID fieldID) { return PROXY_CALL(GetBooleanField, obj, fieldID); }

  jbyte GetByteField(jobject obj, jfieldID fieldID) { return PROXY_CALL(GetByteField, obj, fieldID); }

  jchar GetCharField(jobject obj, jfieldID fieldID) { return PROXY_CALL(GetCharField, obj, fieldID); }

  jshort GetShortField(jobject obj, jfieldID fieldID) { return PROXY_CALL(GetShortField, obj, fieldID); }

  jint GetIntField(jobject obj, jfieldID fieldID) { return PROXY_CALL(GetIntField, obj, fieldID); }

  jlong GetLongField(jobject obj, jfieldID fieldID) { return PROXY_CALL(GetLongField, obj, fieldID); }

  jfloat GetFloatField(jobject obj, jfieldID fieldID) { return PROXY_CALL(GetFloatField, obj, fieldID); }

  jdouble GetDoubleField(jobject obj, jfieldID fieldID) { return PROXY_CALL(GetDoubleField, obj, fieldID); }

  void SetObjectField(jobject obj, jfieldID fieldID, jobject value) { PROXY_CALL(SetObjectField, obj, fieldID, value); }

  void SetBooleanField(jobject obj, jfieldID fieldID, jboolean value) {
    PROXY_CALL(SetBooleanField, obj, fieldID, value);
  }

  void SetByteField(jobject obj, jfieldID fieldID, jbyte value) { PROXY_CALL(SetByteField, obj, fieldID, value); }

  void SetCharField(jobject obj, jfieldID fieldID, jchar value) { PROXY_CALL(SetCharField, obj, fieldID, value); }

  void SetShortField(jobject obj, jfieldID fieldID, jshort value) { PROXY_CALL(SetShortField, obj, fieldID, value); }

  void SetIntField(jobject obj, jfieldID fieldID, jint value) { PROXY_CALL(SetIntField, obj, fieldID, value); }

  void SetLongField(jobject obj, jfieldID fieldID, jlong value) { PROXY_CALL(SetLongField, obj, fieldID, value); }

  void SetFloatField(jobject obj, jfieldID fieldID, jfloat value) { PROXY_CALL(SetFloatField, obj, fieldID, value); }

  void SetDoubleField(jobject obj, jfieldID fieldID, jdouble value) { PROXY_CALL(SetDoubleField, obj, fieldID, value); }

  jmethodID GetStaticMethodID(jclass clazz, const char *name, const char *sig) {
    return PROXY_CALL(GetStaticMethodID, clazz, name, sig);
  }

#define PROXY_CALL_STATIC_TYPE_METHOD(_jtype, _jname)                                                                  \
  _jtype CallStatic##_jname##Method(jclass clazz, jmethodID methodID, ...) {                                           \
    _jtype result;                                                                                                     \
    va_list args;                                                                                                      \
    va_start(args, methodID);                                                                                          \
    result = PROXY_CALL(CallStatic##_jname##MethodV, clazz, methodID, args);                                           \
    va_end(args);                                                                                                      \
    return result;                                                                                                     \
  }
#define PROXY_CALL_STATIC_TYPE_METHODV(_jtype, _jname)                                                                 \
  _jtype CallStatic##_jname##MethodV(jclass clazz, jmethodID methodID, va_list args) {                                 \
    return PROXY_CALL(CallStatic##_jname##MethodV, clazz, methodID, args);                                             \
  }
#define PROXY_CALL_STATIC_TYPE_METHODA(_jtype, _jname)                                                                 \
  _jtype CallStatic##_jname##MethodA(jclass clazz, jmethodID methodID, const jvalue *args) {                           \
    return PROXY_CALL(CallStatic##_jname##MethodA, clazz, methodID, args);                                             \
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

#undef PROXY_CALL_STATIC_TYPE
#undef PROXY_CALL_STATIC_TYPE_METHOD
#undef PROXY_CALL_STATIC_TYPE_METHODV
#undef PROXY_CALL_STATIC_TYPE_METHODA

  void CallStaticVoidMethod(jclass clazz, jmethodID methodID, ...) {
    va_list args;
    va_start(args, methodID);
    PROXY_CALL(CallStaticVoidMethodV, clazz, methodID, args);
    va_end(args);
  }

  void CallStaticVoidMethodV(jclass clazz, jmethodID methodID, va_list args) {
    PROXY_CALL(CallStaticVoidMethodV, clazz, methodID, args);
  }

  void CallStaticVoidMethodA(jclass clazz, jmethodID methodID, const jvalue *args) {
    PROXY_CALL(CallStaticVoidMethodA, clazz, methodID, args);
  }

  jfieldID GetStaticFieldID(jclass clazz, const char *name, const char *sig) {
    return PROXY_CALL(GetStaticFieldID, clazz, name, sig);
  }

  jobject GetStaticObjectField(jclass clazz, jfieldID fieldID) {
    return PROXY_CALL(GetStaticObjectField, clazz, fieldID);
  }

  jboolean GetStaticBooleanField(jclass clazz, jfieldID fieldID) {
    return PROXY_CALL(GetStaticBooleanField, clazz, fieldID);
  }

  jbyte GetStaticByteField(jclass clazz, jfieldID fieldID) { return PROXY_CALL(GetStaticByteField, clazz, fieldID); }

  jchar GetStaticCharField(jclass clazz, jfieldID fieldID) { return PROXY_CALL(GetStaticCharField, clazz, fieldID); }

  jshort GetStaticShortField(jclass clazz, jfieldID fieldID) { return PROXY_CALL(GetStaticShortField, clazz, fieldID); }

  jint GetStaticIntField(jclass clazz, jfieldID fieldID) { return PROXY_CALL(GetStaticIntField, clazz, fieldID); }

  jlong GetStaticLongField(jclass clazz, jfieldID fieldID) { return PROXY_CALL(GetStaticLongField, clazz, fieldID); }

  jfloat GetStaticFloatField(jclass clazz, jfieldID fieldID) { return PROXY_CALL(GetStaticFloatField, clazz, fieldID); }

  jdouble GetStaticDoubleField(jclass clazz, jfieldID fieldID) {
    return PROXY_CALL(GetStaticDoubleField, clazz, fieldID);
  }

  void SetStaticObjectField(jclass clazz, jfieldID fieldID, jobject value) {
    PROXY_CALL(SetStaticObjectField, clazz, fieldID, value);
  }

  void SetStaticBooleanField(jclass clazz, jfieldID fieldID, jboolean value) {
    PROXY_CALL(SetStaticBooleanField, clazz, fieldID, value);
  }

  void SetStaticByteField(jclass clazz, jfieldID fieldID, jbyte value) {
    PROXY_CALL(SetStaticByteField, clazz, fieldID, value);
  }

  void SetStaticCharField(jclass clazz, jfieldID fieldID, jchar value) {
    PROXY_CALL(SetStaticCharField, clazz, fieldID, value);
  }

  void SetStaticShortField(jclass clazz, jfieldID fieldID, jshort value) {
    PROXY_CALL(SetStaticShortField, clazz, fieldID, value);
  }

  void SetStaticIntField(jclass clazz, jfieldID fieldID, jint value) {
    PROXY_CALL(SetStaticIntField, clazz, fieldID, value);
  }

  void SetStaticLongField(jclass clazz, jfieldID fieldID, jlong value) {
    PROXY_CALL(SetStaticLongField, clazz, fieldID, value);
  }

  void SetStaticFloatField(jclass clazz, jfieldID fieldID, jfloat value) {
    PROXY_CALL(SetStaticFloatField, clazz, fieldID, value);
  }

  void SetStaticDoubleField(jclass clazz, jfieldID fieldID, jdouble value) {
    PROXY_CALL(SetStaticDoubleField, clazz, fieldID, value);
  }

  jstring NewString(const jchar *unicodeChars, jsize len) { return PROXY_CALL(NewString, unicodeChars, len); }

  jsize GetStringLength(jstring string) { return PROXY_CALL(GetStringLength, string); }

  const jchar *GetStringChars(jstring string, jboolean *isCopy) { return PROXY_CALL(GetStringChars, string, isCopy); }

  void ReleaseStringChars(jstring string, const jchar *chars) { PROXY_CALL(ReleaseStringChars, string, chars); }

  jstring NewStringUTF(const char *bytes) { return PROXY_CALL(NewStringUTF, bytes); }

  jsize GetStringUTFLength(jstring string) { return PROXY_CALL(GetStringUTFLength, string); }

  const char *GetStringUTFChars(jstring string, jboolean *isCopy) {
    return PROXY_CALL(GetStringUTFChars, string, isCopy);
  }

  void ReleaseStringUTFChars(jstring string, const char *utf) { PROXY_CALL(ReleaseStringUTFChars, string, utf); }

  jsize GetArrayLength(jarray array) { return PROXY_CALL(GetArrayLength, array); }

  jobjectArray NewObjectArray(jsize length, jclass elementClass, jobject initialElement) {
    return PROXY_CALL(NewObjectArray, length, elementClass, initialElement);
  }

  jobject GetObjectArrayElement(jobjectArray array, jsize index) {
    return PROXY_CALL(GetObjectArrayElement, array, index);
  }

  void SetObjectArrayElement(jobjectArray array, jsize index, jobject value) {
    PROXY_CALL(SetObjectArrayElement, array, index, value);
  }

  jbooleanArray NewBooleanArray(jsize length) { return PROXY_CALL(NewBooleanArray, length); }

  jbyteArray NewByteArray(jsize length) { return PROXY_CALL(NewByteArray, length); }

  jcharArray NewCharArray(jsize length) { return PROXY_CALL(NewCharArray, length); }

  jshortArray NewShortArray(jsize length) { return PROXY_CALL(NewShortArray, length); }

  jintArray NewIntArray(jsize length) { return PROXY_CALL(NewIntArray, length); }

  jlongArray NewLongArray(jsize length) { return PROXY_CALL(NewLongArray, length); }

  jfloatArray NewFloatArray(jsize length) { return PROXY_CALL(NewFloatArray, length); }

  jdoubleArray NewDoubleArray(jsize length) { return PROXY_CALL(NewDoubleArray, length); }

  jboolean *GetBooleanArrayElements(jbooleanArray array, jboolean *isCopy) {
    return PROXY_CALL(GetBooleanArrayElements, array, isCopy);
  }

  jbyte *GetByteArrayElements(jbyteArray array, jboolean *isCopy) {
    return PROXY_CALL(GetByteArrayElements, array, isCopy);
  }

  jchar *GetCharArrayElements(jcharArray array, jboolean *isCopy) {
    return PROXY_CALL(GetCharArrayElements, array, isCopy);
  }

  jshort *GetShortArrayElements(jshortArray array, jboolean *isCopy) {
    return PROXY_CALL(GetShortArrayElements, array, isCopy);
  }

  jint *GetIntArrayElements(jintArray array, jboolean *isCopy) {
    return PROXY_CALL(GetIntArrayElements, array, isCopy);
  }

  jlong *GetLongArrayElements(jlongArray array, jboolean *isCopy) {
    return PROXY_CALL(GetLongArrayElements, array, isCopy);
  }

  jfloat *GetFloatArrayElements(jfloatArray array, jboolean *isCopy) {
    return PROXY_CALL(GetFloatArrayElements, array, isCopy);
  }

  jdouble *GetDoubleArrayElements(jdoubleArray array, jboolean *isCopy) {
    return PROXY_CALL(GetDoubleArrayElements, array, isCopy);
  }

  void ReleaseBooleanArrayElements(jbooleanArray array, jboolean *elems, jint mode) {
    PROXY_CALL(ReleaseBooleanArrayElements, array, elems, mode);
  }

  void ReleaseByteArrayElements(jbyteArray array, jbyte *elems, jint mode) {
    PROXY_CALL(ReleaseByteArrayElements, array, elems, mode);
  }

  void ReleaseCharArrayElements(jcharArray array, jchar *elems, jint mode) {
    PROXY_CALL(ReleaseCharArrayElements, array, elems, mode);
  }

  void ReleaseShortArrayElements(jshortArray array, jshort *elems, jint mode) {
    PROXY_CALL(ReleaseShortArrayElements, array, elems, mode);
  }

  void ReleaseIntArrayElements(jintArray array, jint *elems, jint mode) {
    PROXY_CALL(ReleaseIntArrayElements, array, elems, mode);
  }

  void ReleaseLongArrayElements(jlongArray array, jlong *elems, jint mode) {
    PROXY_CALL(ReleaseLongArrayElements, array, elems, mode);
  }

  void ReleaseFloatArrayElements(jfloatArray array, jfloat *elems, jint mode) {
    PROXY_CALL(ReleaseFloatArrayElements, array, elems, mode);
  }

  void ReleaseDoubleArrayElements(jdoubleArray array, jdouble *elems, jint mode) {
    PROXY_CALL(ReleaseDoubleArrayElements, array, elems, mode);
  }

  void GetBooleanArrayRegion(jbooleanArray array, jsize start, jsize len, jboolean *buf) {
    PROXY_CALL(GetBooleanArrayRegion, array, start, len, buf);
  }

  void GetByteArrayRegion(jbyteArray array, jsize start, jsize len, jbyte *buf) {
    PROXY_CALL(GetByteArrayRegion, array, start, len, buf);
  }

  void GetCharArrayRegion(jcharArray array, jsize start, jsize len, jchar *buf) {
    PROXY_CALL(GetCharArrayRegion, array, start, len, buf);
  }

  void GetShortArrayRegion(jshortArray array, jsize start, jsize len, jshort *buf) {
    PROXY_CALL(GetShortArrayRegion, array, start, len, buf);
  }

  void GetIntArrayRegion(jintArray array, jsize start, jsize len, jint *buf) {
    PROXY_CALL(GetIntArrayRegion, array, start, len, buf);
  }

  void GetLongArrayRegion(jlongArray array, jsize start, jsize len, jlong *buf) {
    PROXY_CALL(GetLongArrayRegion, array, start, len, buf);
  }

  void GetFloatArrayRegion(jfloatArray array, jsize start, jsize len, jfloat *buf) {
    PROXY_CALL(GetFloatArrayRegion, array, start, len, buf);
  }

  void GetDoubleArrayRegion(jdoubleArray array, jsize start, jsize len, jdouble *buf) {
    PROXY_CALL(GetDoubleArrayRegion, array, start, len, buf);
  }

  void SetBooleanArrayRegion(jbooleanArray array, jsize start, jsize len, const jboolean *buf) {
    PROXY_CALL(SetBooleanArrayRegion, array, start, len, buf);
  }

  void SetByteArrayRegion(jbyteArray array, jsize start, jsize len, const jbyte *buf) {
    PROXY_CALL(SetByteArrayRegion, array, start, len, buf);
  }

  void SetCharArrayRegion(jcharArray array, jsize start, jsize len, const jchar *buf) {
    PROXY_CALL(SetCharArrayRegion, array, start, len, buf);
  }

  void SetShortArrayRegion(jshortArray array, jsize start, jsize len, const jshort *buf) {
    PROXY_CALL(SetShortArrayRegion, array, start, len, buf);
  }

  void SetIntArrayRegion(jintArray array, jsize start, jsize len, const jint *buf) {
    PROXY_CALL(SetIntArrayRegion, array, start, len, buf);
  }

  void SetLongArrayRegion(jlongArray array, jsize start, jsize len, const jlong *buf) {
    PROXY_CALL(SetLongArrayRegion, array, start, len, buf);
  }

  void SetFloatArrayRegion(jfloatArray array, jsize start, jsize len, const jfloat *buf) {
    PROXY_CALL(SetFloatArrayRegion, array, start, len, buf);
  }

  void SetDoubleArrayRegion(jdoubleArray array, jsize start, jsize len, const jdouble *buf) {
    PROXY_CALL(SetDoubleArrayRegion, array, start, len, buf);
  }

  jint RegisterNatives(jclass clazz, const JNINativeMethod *methods, jint nMethods) {
    return PROXY_CALL(RegisterNatives, clazz, methods, nMethods);
  }

  jint UnregisterNatives(jclass clazz) { return PROXY_CALL(UnregisterNatives, clazz); }

  jint MonitorEnter(jobject obj) { return PROXY_CALL(MonitorEnter, obj); }

  jint MonitorExit(jobject obj) { return PROXY_CALL(MonitorExit, obj); }

  jint GetJavaVM(JavaVM **vm) { return PROXY_CALL(GetJavaVM, vm); }

  void GetStringRegion(jstring str, jsize start, jsize len, jchar *buf) {
    PROXY_CALL(GetStringRegion, str, start, len, buf);
  }

  void GetStringUTFRegion(jstring str, jsize start, jsize len, char *buf) {
    return PROXY_CALL(GetStringUTFRegion, str, start, len, buf);
  }

  void *GetPrimitiveArrayCritical(jarray array, jboolean *isCopy) {
    return PROXY_CALL(GetPrimitiveArrayCritical, array, isCopy);
  }

  void ReleasePrimitiveArrayCritical(jarray array, void *carray, jint mode) {
    PROXY_CALL(ReleasePrimitiveArrayCritical, array, carray, mode);
  }

  const jchar *GetStringCritical(jstring string, jboolean *isCopy) {
    return PROXY_CALL(GetStringCritical, string, isCopy);
  }

  void ReleaseStringCritical(jstring string, const jchar *carray) { PROXY_CALL(ReleaseStringCritical, string, carray); }

  jweak NewWeakGlobalRef(jobject obj) { return PROXY_CALL(NewWeakGlobalRef, obj); }

  void DeleteWeakGlobalRef(jweak obj) { PROXY_CALL(DeleteWeakGlobalRef, obj); }

  jboolean ExceptionCheck() { return PROXY_CALL(ExceptionCheck); }

  jobject NewDirectByteBuffer(void *address, jlong capacity) {
    return PROXY_CALL(NewDirectByteBuffer, address, capacity);
  }

  void *GetDirectBufferAddress(jobject buf) { return PROXY_CALL(GetDirectBufferAddress, buf); }

  jlong GetDirectBufferCapacity(jobject buf) { return PROXY_CALL(GetDirectBufferCapacity, buf); }

  /* added in JNI 1.6 */
  jobjectRefType GetObjectRefType(jobject obj) { return PROXY_CALL(GetObjectRefType, obj); }

public:
  explicit ProxyJNIEnv(JNIEnv *env) : env_(env) {
    functions = backup_functions ? backup_functions : original_functions;
  }

  explicit ProxyJNIEnv(JNIEnv *env, JNINativeInterface *interface) : env_(env), functions(interface) {}

  void Env(JNIEnv *env) { env_ = env; }

  static void SetBackupFunctions(JNINativeInterface *functions) { backup_functions = functions; }

private:
  JNIEnv *env_;
  JNINativeInterface *functions;
  static JNINativeInterface *backup_functions;
};

} // namespace fakelinker

#undef PROXY_CALL