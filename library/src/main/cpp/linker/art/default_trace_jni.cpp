#include <default_trace_jni.h>

#include <android/log.h>

#include <alog.h>

namespace fakelinker {

void DefaultTraceJNICallback::TraceLog(std::string_view message) {
  __android_log_print(ANDROID_LOG_WARN, "JNITrace", "%s", message.data());
}

bool DefaultTraceJNICallback::Register() {
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
  JNIMonitor::Get().AddTraceFunctions(offsets);
  return JNIMonitor::Get().StartTrace(this);
}

void DefaultTraceJNICallback::FindClass(TraceInvokeContext<jclass> &context, const char *name) {
  FormatJNIArguments("FindClass", context, name);
}
void DefaultTraceJNICallback::GetSuperclass(TraceInvokeContext<jclass> &context, jclass clazz) {
  FormatJNIArguments("GetSuperclass", context, clazz);
}
void DefaultTraceJNICallback::NewGlobalRef(TraceInvokeContext<jobject> &context, jobject obj) {
  FormatJNIArguments("NewGlobalRef", context, obj);
}
void DefaultTraceJNICallback::DeleteGlobalRef(TraceInvokeContext<void> &context, jobject globalRef) {
  FormatJNIArguments("DeleteGlobalRef", context, globalRef);
}
void DefaultTraceJNICallback::NewObjectV(TraceInvokeContext<jobject> &context, jclass clazz, jmethodID methodID,
                                         va_list args) {
  FormatJNIArguments("NewObjectV", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::NewObjectA(TraceInvokeContext<jobject> &context, jclass clazz, jmethodID methodID,
                                         const jvalue *args) {
  FormatJNIArguments("NewObjectA", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::GetObjectClass(TraceInvokeContext<jclass> &context, jobject obj) {
  FormatJNIArguments("GetObjectClass", context, obj);
}
void DefaultTraceJNICallback::GetMethodID(TraceInvokeContext<jmethodID> &context, jclass clazz, const char *name,
                                          const char *sig) {
  FormatJNIArguments("GetMethodID", context, clazz, name, sig);
}
void DefaultTraceJNICallback::CallObjectMethodV(TraceInvokeContext<jobject> &context, jobject obj, jmethodID methodID,
                                                va_list args) {
  FormatJNIArguments("CallObjectMethodV", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallObjectMethodA(TraceInvokeContext<jobject> &context, jobject obj, jmethodID methodID,
                                                const jvalue *args) {
  FormatJNIArguments("CallObjectMethodA", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallBooleanMethodV(TraceInvokeContext<jboolean> &context, jobject obj, jmethodID methodID,
                                                 va_list args) {
  FormatJNIArguments("CallBooleanMethodV", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallBooleanMethodA(TraceInvokeContext<jboolean> &context, jobject obj, jmethodID methodID,
                                                 const jvalue *args) {
  FormatJNIArguments("CallBooleanMethodA", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallByteMethodV(TraceInvokeContext<jbyte> &context, jobject obj, jmethodID methodID,
                                              va_list args) {
  FormatJNIArguments("CallByteMethodV", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallByteMethodA(TraceInvokeContext<jbyte> &context, jobject obj, jmethodID methodID,
                                              const jvalue *args) {
  FormatJNIArguments("CallByteMethodA", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallCharMethodV(TraceInvokeContext<jchar> &context, jobject obj, jmethodID methodID,
                                              va_list args) {
  FormatJNIArguments("CallCharMethodV", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallCharMethodA(TraceInvokeContext<jchar> &context, jobject obj, jmethodID methodID,
                                              const jvalue *args) {
  FormatJNIArguments("CallCharMethodA", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallShortMethodV(TraceInvokeContext<jshort> &context, jobject obj, jmethodID methodID,
                                               va_list args) {
  FormatJNIArguments("CallShortMethodV", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallShortMethodA(TraceInvokeContext<jshort> &context, jobject obj, jmethodID methodID,
                                               const jvalue *args) {
  FormatJNIArguments("CallShortMethodA", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallIntMethodV(TraceInvokeContext<jint> &context, jobject obj, jmethodID methodID,
                                             va_list args) {
  FormatJNIArguments("CallIntMethodV", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallIntMethodA(TraceInvokeContext<jint> &context, jobject obj, jmethodID methodID,
                                             const jvalue *args) {
  FormatJNIArguments("CallIntMethodA", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallLongMethodV(TraceInvokeContext<jlong> &context, jobject obj, jmethodID methodID,
                                              va_list args) {
  FormatJNIArguments("CallLongMethodV", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallLongMethodA(TraceInvokeContext<jlong> &context, jobject obj, jmethodID methodID,
                                              const jvalue *args) {
  FormatJNIArguments("CallLongMethodA", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallFloatMethodV(TraceInvokeContext<jfloat> &context, jobject obj, jmethodID methodID,
                                               va_list args) {
  FormatJNIArguments("CallFloatMethodV", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallFloatMethodA(TraceInvokeContext<jfloat> &context, jobject obj, jmethodID methodID,
                                               const jvalue *args) {
  FormatJNIArguments("CallFloatMethodA", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallDoubleMethodV(TraceInvokeContext<jdouble> &context, jobject obj, jmethodID methodID,
                                                va_list args) {
  FormatJNIArguments("CallDoubleMethodV", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallDoubleMethodA(TraceInvokeContext<jdouble> &context, jobject obj, jmethodID methodID,
                                                const jvalue *args) {
  FormatJNIArguments("CallDoubleMethodA", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallVoidMethodV(TraceInvokeContext<void> &context, jobject obj, jmethodID methodID,
                                              va_list args) {
  FormatJNIArguments("CallVoidMethodV", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallVoidMethodA(TraceInvokeContext<void> &context, jobject obj, jmethodID methodID,
                                              const jvalue *args) {
  FormatJNIArguments("CallVoidMethodA", context, obj, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualObjectMethodV(TraceInvokeContext<jobject> &context, jobject obj,
                                                          jclass clazz, jmethodID methodID, va_list args) {
  FormatJNIArguments("CallNonvirtualObjectMethodV", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualObjectMethodA(TraceInvokeContext<jobject> &context, jobject obj,
                                                          jclass clazz, jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallNonvirtualObjectMethodA", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualBooleanMethodV(TraceInvokeContext<jboolean> &context, jobject obj,
                                                           jclass clazz, jmethodID methodID, va_list args) {
  FormatJNIArguments("CallNonvirtualBooleanMethodV", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualBooleanMethodA(TraceInvokeContext<jboolean> &context, jobject obj,
                                                           jclass clazz, jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallNonvirtualBooleanMethodA", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualByteMethodV(TraceInvokeContext<jbyte> &context, jobject obj, jclass clazz,
                                                        jmethodID methodID, va_list args) {
  FormatJNIArguments("CallNonvirtualByteMethodV", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualByteMethodA(TraceInvokeContext<jbyte> &context, jobject obj, jclass clazz,
                                                        jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallNonvirtualByteMethodA", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualCharMethodV(TraceInvokeContext<jchar> &context, jobject obj, jclass clazz,
                                                        jmethodID methodID, va_list args) {
  FormatJNIArguments("CallNonvirtualCharMethodV", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualCharMethodA(TraceInvokeContext<jchar> &context, jobject obj, jclass clazz,
                                                        jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallNonvirtualCharMethodA", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualShortMethodV(TraceInvokeContext<jshort> &context, jobject obj, jclass clazz,
                                                         jmethodID methodID, va_list args) {
  FormatJNIArguments("CallNonvirtualShortMethodV", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualShortMethodA(TraceInvokeContext<jshort> &context, jobject obj, jclass clazz,
                                                         jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallNonvirtualShortMethodA", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualIntMethodV(TraceInvokeContext<jint> &context, jobject obj, jclass clazz,
                                                       jmethodID methodID, va_list args) {
  FormatJNIArguments("CallNonvirtualIntMethodV", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualIntMethodA(TraceInvokeContext<jint> &context, jobject obj, jclass clazz,
                                                       jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallNonvirtualIntMethodA", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualLongMethodV(TraceInvokeContext<jlong> &context, jobject obj, jclass clazz,
                                                        jmethodID methodID, va_list args) {
  FormatJNIArguments("CallNonvirtualLongMethodV", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualLongMethodA(TraceInvokeContext<jlong> &context, jobject obj, jclass clazz,
                                                        jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallNonvirtualLongMethodA", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualFloatMethodV(TraceInvokeContext<jfloat> &context, jobject obj, jclass clazz,
                                                         jmethodID methodID, va_list args) {
  FormatJNIArguments("CallNonvirtualFloatMethodV", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualFloatMethodA(TraceInvokeContext<jfloat> &context, jobject obj, jclass clazz,
                                                         jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallNonvirtualFloatMethodA", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualDoubleMethodV(TraceInvokeContext<jdouble> &context, jobject obj,
                                                          jclass clazz, jmethodID methodID, va_list args) {
  FormatJNIArguments("CallNonvirtualDoubleMethodV", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualDoubleMethodA(TraceInvokeContext<jdouble> &context, jobject obj,
                                                          jclass clazz, jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallNonvirtualDoubleMethodA", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualVoidMethodV(TraceInvokeContext<void> &context, jobject obj, jclass clazz,
                                                        jmethodID methodID, va_list args) {
  FormatJNIArguments("CallNonvirtualVoidMethodV", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallNonvirtualVoidMethodA(TraceInvokeContext<void> &context, jobject obj, jclass clazz,
                                                        jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallNonvirtualVoidMethodA", context, obj, clazz, methodID, args);
}
void DefaultTraceJNICallback::GetFieldID(TraceInvokeContext<jfieldID> &context, jclass clazz, const char *name,
                                         const char *sig) {
  FormatJNIArguments("GetFieldID", context, clazz, name, sig);
}
void DefaultTraceJNICallback::GetObjectField(TraceInvokeContext<jobject> &context, jobject obj, jfieldID fieldID) {
  FormatJNIArguments("GetObjectField", context, obj, fieldID);
}
void DefaultTraceJNICallback::GetBooleanField(TraceInvokeContext<jboolean> &context, jobject obj, jfieldID fieldID) {
  FormatJNIArguments("GetBooleanField", context, obj, fieldID);
}
void DefaultTraceJNICallback::GetByteField(TraceInvokeContext<jbyte> &context, jobject obj, jfieldID fieldID) {
  FormatJNIArguments("GetByteField", context, obj, fieldID);
}
void DefaultTraceJNICallback::GetCharField(TraceInvokeContext<jchar> &context, jobject obj, jfieldID fieldID) {
  FormatJNIArguments("GetCharField", context, obj, fieldID);
}
void DefaultTraceJNICallback::GetIntField(TraceInvokeContext<jint> &context, jobject obj, jfieldID fieldID) {
  FormatJNIArguments("GetIntField", context, obj, fieldID);
}
void DefaultTraceJNICallback::GetLongField(TraceInvokeContext<jlong> &context, jobject obj, jfieldID fieldID) {
  FormatJNIArguments("GetLongField", context, obj, fieldID);
}
void DefaultTraceJNICallback::GetFloatField(TraceInvokeContext<jfloat> &context, jobject obj, jfieldID fieldID) {
  FormatJNIArguments("GetFloatField", context, obj, fieldID);
}
void DefaultTraceJNICallback::GetDoubleField(TraceInvokeContext<jdouble> &context, jobject obj, jfieldID fieldID) {
  FormatJNIArguments("GetDoubleField", context, obj, fieldID);
}
void DefaultTraceJNICallback::SetObjectField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID,
                                             jobject value) {
  FormatJNIArguments("SetObjectField", context, obj, fieldID, value);
}
void DefaultTraceJNICallback::SetBooleanField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID,
                                              jboolean value) {
  FormatJNIArguments("SetBooleanField", context, obj, fieldID, value);
}
void DefaultTraceJNICallback::SetByteField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID,
                                           jbyte value) {
  FormatJNIArguments("SetByteField", context, obj, fieldID, value);
}
void DefaultTraceJNICallback::SetCharField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID,
                                           jchar value) {
  FormatJNIArguments("SetCharField", context, obj, fieldID, value);
}
void DefaultTraceJNICallback::SetShortField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID,
                                            jshort value) {
  FormatJNIArguments("SetShortField", context, obj, fieldID, value);
}
void DefaultTraceJNICallback::SetIntField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID,
                                          jint value) {
  FormatJNIArguments("SetIntField", context, obj, fieldID, value);
}
void DefaultTraceJNICallback::SetLongField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID,
                                           jlong value) {
  FormatJNIArguments("SetLongField", context, obj, fieldID, value);
}
void DefaultTraceJNICallback::SetFloatField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID,
                                            jfloat value) {
  FormatJNIArguments("SetFloatField", context, obj, fieldID, value);
}
void DefaultTraceJNICallback::SetDoubleField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID,
                                             jdouble value) {
  FormatJNIArguments("SetDoubleField", context, obj, fieldID, value);
}
void DefaultTraceJNICallback::GetStaticMethodID(TraceInvokeContext<jmethodID> &context, jclass clazz, const char *name,
                                                const char *sig) {
  FormatJNIArguments("GetStaticMethodID", context, clazz, name, sig);
}
void DefaultTraceJNICallback::CallStaticObjectMethodV(TraceInvokeContext<jobject> &context, jclass clazz,
                                                      jmethodID methodID, va_list args) {
  FormatJNIArguments("CallStaticObjectMethodV", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticObjectMethodA(TraceInvokeContext<jobject> &context, jclass clazz,
                                                      jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallStaticObjectMethodA", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticBooleanMethodV(TraceInvokeContext<jboolean> &context, jclass clazz,
                                                       jmethodID methodID, va_list args) {
  FormatJNIArguments("CallStaticBooleanMethodV", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticBooleanMethodA(TraceInvokeContext<jboolean> &context, jclass clazz,
                                                       jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallStaticBooleanMethodA", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticByteMethodV(TraceInvokeContext<jbyte> &context, jclass clazz,
                                                    jmethodID methodID, va_list args) {
  FormatJNIArguments("CallStaticByteMethodV", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticByteMethodA(TraceInvokeContext<jbyte> &context, jclass clazz,
                                                    jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallStaticByteMethodA", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticCharMethodV(TraceInvokeContext<jchar> &context, jclass clazz,
                                                    jmethodID methodID, va_list args) {
  FormatJNIArguments("CallStaticCharMethodV", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticCharMethodA(TraceInvokeContext<jchar> &context, jclass clazz,
                                                    jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallStaticCharMethodA", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticShortMethodV(TraceInvokeContext<jshort> &context, jclass clazz,
                                                     jmethodID methodID, va_list args) {
  FormatJNIArguments("CallStaticShortMethodV", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticShortMethodA(TraceInvokeContext<jshort> &context, jclass clazz,
                                                     jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallStaticShortMethodA", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticIntMethodV(TraceInvokeContext<jint> &context, jclass clazz, jmethodID methodID,
                                                   va_list args) {
  FormatJNIArguments("CallStaticIntMethodV", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticIntMethodA(TraceInvokeContext<jint> &context, jclass clazz, jmethodID methodID,
                                                   const jvalue *args) {
  FormatJNIArguments("CallStaticIntMethodA", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticLongMethodV(TraceInvokeContext<jlong> &context, jclass clazz,
                                                    jmethodID methodID, va_list args) {
  FormatJNIArguments("CallStaticLongMethodV", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticLongMethodA(TraceInvokeContext<jlong> &context, jclass clazz,
                                                    jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallStaticLongMethodA", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticFloatMethodV(TraceInvokeContext<jfloat> &context, jclass clazz,
                                                     jmethodID methodID, va_list args) {
  FormatJNIArguments("CallStaticFloatMethodV", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticFloatMethodA(TraceInvokeContext<jfloat> &context, jclass clazz,
                                                     jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallStaticFloatMethodA", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticDoubleMethodV(TraceInvokeContext<jdouble> &context, jclass clazz,
                                                      jmethodID methodID, va_list args) {
  FormatJNIArguments("CallStaticDoubleMethodV", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticDoubleMethodA(TraceInvokeContext<jdouble> &context, jclass clazz,
                                                      jmethodID methodID, const jvalue *args) {
  FormatJNIArguments("CallStaticDoubleMethodA", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticVoidMethodV(TraceInvokeContext<void> &context, jclass clazz, jmethodID methodID,
                                                    va_list args) {
  FormatJNIArguments("CallStaticVoidMethodV", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::CallStaticVoidMethodA(TraceInvokeContext<void> &context, jclass clazz, jmethodID methodID,
                                                    const jvalue *args) {
  FormatJNIArguments("CallStaticVoidMethodA", context, clazz, methodID, args);
}
void DefaultTraceJNICallback::GetStaticFieldID(TraceInvokeContext<jfieldID> &context, jclass clazz, const char *name,
                                               const char *sig) {
  FormatJNIArguments("GetStaticFieldID", context, clazz, name, sig);
}
void DefaultTraceJNICallback::GetStaticObjectField(TraceInvokeContext<jobject> &context, jclass clazz,
                                                   jfieldID fieldID) {
  FormatJNIArguments("GetStaticObjectField", context, clazz, fieldID);
}
void DefaultTraceJNICallback::GetStaticBooleanField(TraceInvokeContext<jboolean> &context, jclass clazz,
                                                    jfieldID fieldID) {
  FormatJNIArguments("GetStaticBooleanField", context, clazz, fieldID);
}
void DefaultTraceJNICallback::GetStaticByteField(TraceInvokeContext<jbyte> &context, jclass clazz, jfieldID fieldID) {
  FormatJNIArguments("GetStaticByteField", context, clazz, fieldID);
}
void DefaultTraceJNICallback::GetStaticCharField(TraceInvokeContext<jchar> &context, jclass clazz, jfieldID fieldID) {
  FormatJNIArguments("GetStaticCharField", context, clazz, fieldID);
}
void DefaultTraceJNICallback::GetStaticShortField(TraceInvokeContext<jshort> &context, jclass clazz, jfieldID fieldID) {
  FormatJNIArguments("GetStaticShortField", context, clazz, fieldID);
}
void DefaultTraceJNICallback::GetStaticIntField(TraceInvokeContext<jint> &context, jclass clazz, jfieldID fieldID) {
  FormatJNIArguments("GetStaticIntField", context, clazz, fieldID);
}
void DefaultTraceJNICallback::GetStaticLongField(TraceInvokeContext<jlong> &context, jclass clazz, jfieldID fieldID) {
  FormatJNIArguments("GetStaticLongField", context, clazz, fieldID);
}
void DefaultTraceJNICallback::GetStaticFloatField(TraceInvokeContext<jfloat> &context, jclass clazz, jfieldID fieldID) {
  FormatJNIArguments("GetStaticFloatField", context, clazz, fieldID);
}
void DefaultTraceJNICallback::GetStaticDoubleField(TraceInvokeContext<jdouble> &context, jclass clazz,
                                                   jfieldID fieldID) {
  FormatJNIArguments("GetStaticDoubleField", context, clazz, fieldID);
}
void DefaultTraceJNICallback::SetStaticObjectField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID,
                                                   jobject value) {
  FormatJNIArguments("SetStaticObjectField", context, clazz, fieldID, value);
}
void DefaultTraceJNICallback::SetStaticBooleanField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID,
                                                    jboolean value) {
  FormatJNIArguments("SetStaticBooleanField", context, clazz, fieldID, value);
}
void DefaultTraceJNICallback::SetStaticByteField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID,
                                                 jbyte value) {
  FormatJNIArguments("SetStaticByteField", context, clazz, fieldID, value);
}
void DefaultTraceJNICallback::SetStaticCharField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID,
                                                 jchar value) {
  FormatJNIArguments("SetStaticCharField", context, clazz, fieldID, value);
}
void DefaultTraceJNICallback::SetStaticShortField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID,
                                                  jshort value) {
  FormatJNIArguments("SetStaticShortField", context, clazz, fieldID, value);
}
void DefaultTraceJNICallback::SetStaticIntField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID,
                                                jint value) {
  FormatJNIArguments("SetStaticIntField", context, clazz, fieldID, value);
}
void DefaultTraceJNICallback::SetStaticLongField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID,
                                                 jlong value) {
  FormatJNIArguments("SetStaticLongField", context, clazz, fieldID, value);
}
void DefaultTraceJNICallback::SetStaticFloatField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID,
                                                  jfloat value) {
  FormatJNIArguments("SetStaticFloatField", context, clazz, fieldID, value);
}
void DefaultTraceJNICallback::SetStaticDoubleField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID,
                                                   jdouble value) {
  FormatJNIArguments("SetStaticDoubleField", context, clazz, fieldID, value);
}
void DefaultTraceJNICallback::NewString(TraceInvokeContext<jstring> &context, const jchar *unicodeChars, jsize len) {
  FormatJNIArguments("NewString", context, unicodeChars, len);
}
void DefaultTraceJNICallback::GetStringLength(TraceInvokeContext<jsize> &context, jstring string) {
  FormatJNIArguments("GetStringLength", context, string);
}
void DefaultTraceJNICallback::GetStringChars(TraceInvokeContext<const jchar *> &context, jstring string,
                                             jboolean *isCopy) {
  FormatJNIArguments("GetStringChars", context, string, isCopy);
}
void DefaultTraceJNICallback::NewStringUTF(TraceInvokeContext<jstring> &context, const char *bytes) {
  FormatJNIArguments("NewStringUTF", context, bytes);
}
void DefaultTraceJNICallback::GetStringUTFLength(TraceInvokeContext<jsize> &context, jstring string) {
  FormatJNIArguments("GetStringUTFLength", context, string);
}
void DefaultTraceJNICallback::GetStringUTFChars(TraceInvokeContext<const char *> &context, jstring string,
                                                jboolean *isCopy) {
  FormatJNIArguments("GetStringUTFChars", context, string, isCopy);
}
void DefaultTraceJNICallback::GetArrayLength(TraceInvokeContext<jsize> &context, jarray array) {
  FormatJNIArguments("GetArrayLength", context, array);
}
void DefaultTraceJNICallback::NewObjectArray(TraceInvokeContext<jobjectArray> &context, jsize length,
                                             jclass elementClass, jobject initialElement) {
  FormatJNIArguments("NewObjectArray", context, length, elementClass, initialElement);
}
void DefaultTraceJNICallback::GetObjectArrayElement(TraceInvokeContext<jobject> &context, jobjectArray array,
                                                    jsize index) {
  FormatJNIArguments("GetObjectArrayElement", context, array, index);
}
void DefaultTraceJNICallback::SetObjectArrayElement(TraceInvokeContext<void> &context, jobjectArray array, jsize index,
                                                    jobject value) {
  FormatJNIArguments("SetObjectArrayElement", context, array, index, value);
}
void DefaultTraceJNICallback::NewBooleanArray(TraceInvokeContext<jbooleanArray> &context, jsize length) {
  FormatJNIArguments("NewBooleanArray", context, length);
}
void DefaultTraceJNICallback::NewByteArray(TraceInvokeContext<jbyteArray> &context, jsize length) {
  FormatJNIArguments("NewByteArray", context, length);
}
void DefaultTraceJNICallback::NewCharArray(TraceInvokeContext<jcharArray> &context, jsize length) {
  FormatJNIArguments("NewCharArray", context, length);
}
void DefaultTraceJNICallback::NewShortArray(TraceInvokeContext<jshortArray> &context, jsize length) {
  FormatJNIArguments("NewShortArray", context, length);
}
void DefaultTraceJNICallback::NewIntArray(TraceInvokeContext<jintArray> &context, jsize length) {
  FormatJNIArguments("NewIntArray", context, length);
}
void DefaultTraceJNICallback::NewLongArray(TraceInvokeContext<jlongArray> &context, jsize length) {
  FormatJNIArguments("NewLongArray", context, length);
}
void DefaultTraceJNICallback::NewFloatArray(TraceInvokeContext<jfloatArray> &context, jsize length) {
  FormatJNIArguments("NewFloatArray", context, length);
}
void DefaultTraceJNICallback::NewDoubleArray(TraceInvokeContext<jdoubleArray> &context, jsize length) {
  FormatJNIArguments("NewDoubleArray", context, length);
}
void DefaultTraceJNICallback::GetBooleanArrayElements(TraceInvokeContext<jboolean *> &context, jbooleanArray array,
                                                      jboolean *isCopy) {
  FormatJNIArguments("GetBooleanArrayElements", context, array, isCopy);
}
void DefaultTraceJNICallback::GetByteArrayElements(TraceInvokeContext<jbyte *> &context, jbyteArray array,
                                                   jboolean *isCopy) {
  FormatJNIArguments("GetByteArrayElements", context, array, isCopy);
}
void DefaultTraceJNICallback::GetCharArrayElements(TraceInvokeContext<jchar *> &context, jcharArray array,
                                                   jboolean *isCopy) {
  FormatJNIArguments("GetCharArrayElements", context, array, isCopy);
}
void DefaultTraceJNICallback::GetShortArrayElements(TraceInvokeContext<jshort *> &context, jshortArray array,
                                                    jboolean *isCopy) {
  FormatJNIArguments("GetShortArrayElements", context, array, isCopy);
}
void DefaultTraceJNICallback::GetIntArrayElements(TraceInvokeContext<jint *> &context, jintArray array,
                                                  jboolean *isCopy) {
  FormatJNIArguments("GetIntArrayElements", context, array, isCopy);
}
void DefaultTraceJNICallback::GetLongArrayElements(TraceInvokeContext<jlong *> &context, jlongArray array,
                                                   jboolean *isCopy) {
  FormatJNIArguments("GetLongArrayElements", context, array, isCopy);
}
void DefaultTraceJNICallback::GetFloatArrayElements(TraceInvokeContext<jfloat *> &context, jfloatArray array,
                                                    jboolean *isCopy) {
  FormatJNIArguments("GetFloatArrayElements", context, array, isCopy);
}
void DefaultTraceJNICallback::GetDoubleArrayElements(TraceInvokeContext<jdouble *> &context, jdoubleArray array,
                                                     jboolean *isCopy) {
  FormatJNIArguments("GetDoubleArrayElements", context, array, isCopy);
}
void DefaultTraceJNICallback::GetBooleanArrayRegion(TraceInvokeContext<void> &context, jbooleanArray array, jsize start,
                                                    jsize len, jboolean *buf) {
  FormatJNIArguments("GetBooleanArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::GetByteArrayRegion(TraceInvokeContext<void> &context, jbyteArray array, jsize start,
                                                 jsize len, jbyte *buf) {
  FormatJNIArguments("GetByteArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::GetCharArrayRegion(TraceInvokeContext<void> &context, jcharArray array, jsize start,
                                                 jsize len, jchar *buf) {
  FormatJNIArguments("GetCharArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::GetShortArrayRegion(TraceInvokeContext<void> &context, jshortArray array, jsize start,
                                                  jsize len, jshort *buf) {
  FormatJNIArguments("GetShortArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::GetIntArrayRegion(TraceInvokeContext<void> &context, jintArray array, jsize start,
                                                jsize len, jint *buf) {
  FormatJNIArguments("GetIntArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::GetLongArrayRegion(TraceInvokeContext<void> &context, jlongArray array, jsize start,
                                                 jsize len, jlong *buf) {
  FormatJNIArguments("GetLongArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::GetFloatArrayRegion(TraceInvokeContext<void> &context, jfloatArray array, jsize start,
                                                  jsize len, jfloat *buf) {
  FormatJNIArguments("GetFloatArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::GetDoubleArrayRegion(TraceInvokeContext<void> &context, jdoubleArray array, jsize start,
                                                   jsize len, jdouble *buf) {
  FormatJNIArguments("GetDoubleArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::SetBooleanArrayRegion(TraceInvokeContext<void> &context, jbooleanArray array, jsize start,
                                                    jsize len, const jboolean *buf) {
  FormatJNIArguments("SetBooleanArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::SetByteArrayRegion(TraceInvokeContext<void> &context, jbyteArray array, jsize start,
                                                 jsize len, const jbyte *buf) {
  FormatJNIArguments("SetByteArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::SetCharArrayRegion(TraceInvokeContext<void> &context, jcharArray array, jsize start,
                                                 jsize len, const jchar *buf) {
  FormatJNIArguments("SetCharArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::SetShortArrayRegion(TraceInvokeContext<void> &context, jshortArray array, jsize start,
                                                  jsize len, const jshort *buf) {
  FormatJNIArguments("SetShortArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::SetIntArrayRegion(TraceInvokeContext<void> &context, jintArray array, jsize start,
                                                jsize len, const jint *buf) {
  FormatJNIArguments("SetIntArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::SetLongArrayRegion(TraceInvokeContext<void> &context, jlongArray array, jsize start,
                                                 jsize len, const jlong *buf) {
  FormatJNIArguments("SetLongArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::SetFloatArrayRegion(TraceInvokeContext<void> &context, jfloatArray array, jsize start,
                                                  jsize len, const jfloat *buf) {
  FormatJNIArguments("SetFloatArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::SetDoubleArrayRegion(TraceInvokeContext<void> &context, jdoubleArray array, jsize start,
                                                   jsize len, const jdouble *buf) {
  FormatJNIArguments("SetDoubleArrayRegion", context, array, start, len, buf);
}
void DefaultTraceJNICallback::RegisterNatives(TraceInvokeContext<jint> &context, jclass clazz,
                                              const JNINativeMethod *methods, jint nMethods) {
  TraceStart(context, "RegisterNatives");
  FormatJNIArgument(context, 0, clazz);
  for (jint i = 0; i < nMethods; ++i) {
    TraceLine(context, "    name: %s, signature: %s, fnPtr: %p", methods[i].name, methods[i].signature,
              methods[i].fnPtr);
  }
  FormatReturnValue(context);
  TraceEnd(context);
}
void DefaultTraceJNICallback::UnregisterNatives(TraceInvokeContext<jint> &context, jclass clazz) {
  FormatJNIArguments("UnregisterNatives", context, clazz);
}
void DefaultTraceJNICallback::GetJavaVM(TraceInvokeContext<jint> &context, JavaVM **vm) {
  FormatJNIArguments("GetJavaVM", context, vm);
}
void DefaultTraceJNICallback::GetStringRegion(TraceInvokeContext<void> &context, jstring str, jsize start, jsize len,
                                              jchar *buf) {
  FormatJNIArguments("GetStringRegion", context, str, start, len, buf);
}
void DefaultTraceJNICallback::GetStringUTFRegion(TraceInvokeContext<void> &context, jstring str, jsize start, jsize len,
                                                 char *buf) {
  FormatJNIArguments("GetStringUTFRegion", context, str, start, len, buf);
}
void DefaultTraceJNICallback::NewWeakGlobalRef(TraceInvokeContext<jweak> &context, jobject obj) {
  FormatJNIArguments("NewWeakGlobalRef", context, obj);
}
void DefaultTraceJNICallback::DeleteWeakGlobalRef(TraceInvokeContext<void> &context, jweak obj) {
  FormatJNIArguments("DeleteWeakGlobalRef", context, obj);
}


} // namespace fakelinker