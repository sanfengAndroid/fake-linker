//
// Created by beich on 2020/12/18.
//

#include "jni_helper.h"
#include "proxy_jni.h"
#include "scoped_utf_chars.h"

#define STR(x) #x
#define CHECK_EXECEPTION_BEFORE(env)                                                                                   \
  if (__predict_false(env.ExceptionCheck())) {                                                                         \
    return __FILE__ ":" STR(__LINE__) ",There is an unhandled exception, "                                             \
                                      "this operation will be ignored";                                                \
  }

#define CLEAR_EXCEPTION_RETURN(env, Tips)                                                                              \
  if (__predict_false(env.ExceptionCheck())) {                                                                         \
    env.ExceptionClear();                                                                                              \
    return Tips;                                                                                                       \
  }

#define CHECK_JNI_NULL_OBJECT_RETURN(obj, Tips)                                                                        \
  if (__predict_false(obj == nullptr)) {                                                                               \
    return Tips;                                                                                                       \
  }

std::string JNIHelper::GetClassName(JNIEnv *env, jclass clazz) {
  fakelinker::ProxyJNIEnv proxy(env);

  CHECK_JNI_NULL_OBJECT_RETURN(clazz, "call GetClassName jclass is nullptr");
  CHECK_EXECEPTION_BEFORE(proxy);
  auto name = reinterpret_cast<jstring>(proxy.CallObjectMethod(clazz, JNIHelper::Get().java_lang_Class_getName));
  CLEAR_EXCEPTION_RETURN(proxy, "call Class.getName() exception occurs");
  ScopedUtfChars sof(env, name);
  return sof.c_str();
}

std::string JNIHelper::GetObjectClassName(JNIEnv *env, jobject obj) {
  fakelinker::ProxyJNIEnv proxy(env);

  CHECK_JNI_NULL_OBJECT_RETURN(obj, "call GetObjectClassName jobject is nullptr");
  CHECK_EXECEPTION_BEFORE(proxy);

  ScopedLocalRef<jclass> clazz(&proxy, (jclass)proxy.CallObjectMethod(obj, JNIHelper::Get().java_lang_Object_getClass));
  CLEAR_EXCEPTION_RETURN(proxy, "call GetObjectClassName exception occurs");
  return GetClassName(env, clazz.get());
}

std::string JNIHelper::ToString(JNIEnv *env, jobject l) {
  fakelinker::ProxyJNIEnv proxy(env);

  CHECK_JNI_NULL_OBJECT_RETURN(l, "call ToString jobejct is nullptr");
  CHECK_EXECEPTION_BEFORE(proxy);

  auto name = reinterpret_cast<jstring>(proxy.CallObjectMethod(l, JNIHelper::Get().java_lang_Object_toString));

  CLEAR_EXCEPTION_RETURN(proxy, "call ToString exception occurs");
  ScopedUtfChars sof(env, name);
  return sof.c_str();
}

std::string JNIHelper::ToString(JNIEnv *env, jmethodID methodID) {
  fakelinker::ProxyJNIEnv proxy(env);

  CHECK_JNI_NULL_OBJECT_RETURN(methodID, "call ToString jmethodID is nullptr");
  CHECK_EXECEPTION_BEFORE(proxy);
  ScopedLocalRef<jobject> method(&proxy, proxy.ToReflectedMethod(JNIHelper::Get().java_lang_Object, methodID, false));
  CLEAR_EXCEPTION_RETURN(proxy, "call Method.toString exception occurs");
  return ToString(env, method.get());
}

std::string JNIHelper::ToString(JNIEnv *env, jfieldID fieldID) {
  fakelinker::ProxyJNIEnv proxy(env);
  CHECK_JNI_NULL_OBJECT_RETURN(fieldID, "call ToString jfieldID is nullptr");
  CHECK_EXECEPTION_BEFORE(proxy);

  ScopedLocalRef<jobject> filed(&proxy, proxy.ToReflectedField(JNIHelper::Get().java_lang_Object, fieldID, false));

  CLEAR_EXCEPTION_RETURN(proxy, "call Field.toString exception occurs");
  return ToString(env, filed.get());
}

void JNIHelper::PrintAndClearException(JNIEnv *env) {
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
  }
}

void JNIHelper::ReleaseByteArray(JNIEnv *env, jbyteArray arr, jbyte *bytes) {
  if (bytes) {
    env->ReleaseByteArrayElements(arr, bytes, JNI_ABORT);
  }
}

void JNIHelper::ReleaseIntArray(JNIEnv *env, jintArray arr, jint *parr) {
  if (parr) {
    env->ReleaseIntArrayElements(arr, parr, 0);
  }
}

std::string JNIHelper::GetMethodName(JNIEnv *env, jmethodID mid) {
  fakelinker::ProxyJNIEnv proxy(env);

  CHECK_JNI_NULL_OBJECT_RETURN(mid, "call GetMethodName jmethodID is nullptr");
  CHECK_EXECEPTION_BEFORE(proxy);
  jobject obj = proxy.ToReflectedMethod(JNIHelper::Get().java_lang_reflect_Method, mid, false);
  ScopedLocalRef<jobject> method_obj(&proxy, obj);
  auto name = reinterpret_cast<jstring>(
    proxy.CallObjectMethod(method_obj.get(), JNIHelper::Get().java_lang_reflect_Method_getName));
  CLEAR_EXCEPTION_RETURN(proxy, "call Method.getName exception occurs");
  ScopedUtfChars sof(env, name);
  return sof.c_str();
}

std::string JNIHelper::GetFieldName(JNIEnv *env, jfieldID fieldId) {
  fakelinker::ProxyJNIEnv proxy(env);
  CHECK_JNI_NULL_OBJECT_RETURN(fieldId, "call GetFieldName jfieldID is nullptr");
  CHECK_EXECEPTION_BEFORE(proxy);
  ScopedLocalRef<jobject> field_obj(env,
                                    proxy.ToReflectedField(JNIHelper::Get().java_lang_reflect_Field, fieldId, false));

  auto name = reinterpret_cast<jstring>(
    proxy.CallObjectMethod(field_obj.get(), JNIHelper::Get().java_lang_reflect_Field_getName));
  CLEAR_EXCEPTION_RETURN(proxy, "call Field.getName exception occurs");
  ScopedUtfChars sof(env, name);
  return sof.c_str();
}

bool JNIHelper::IsClassObject(JNIEnv *env, jobject obj) {
  fakelinker::ProxyJNIEnv proxy(env);
  CHECK_JNI_NULL_OBJECT_RETURN(obj, "call IsClassObject jobject is nullptr");
  CHECK_EXECEPTION_BEFORE(proxy);
  return proxy.IsInstanceOf(obj, JNIHelper::Get().java_lang_Class);
}

jclass JNIHelper::CacheClass(JNIEnv *env, const char *jni_class_name) {
  ScopedLocalRef<jclass> c(env, env->FindClass(jni_class_name));
  if (c.get() == nullptr) {
    LOGE("Couldn't find class: %s", jni_class_name);
    return nullptr;
  }
  return reinterpret_cast<jclass>(env->NewGlobalRef(c.get()));
}

jmethodID JNIHelper::CacheMethod(JNIEnv *env, jclass c, bool is_static, const char *name, const char *signature) {
  jmethodID mid;
  mid = is_static ? env->GetStaticMethodID(c, name, signature) : env->GetMethodID(c, name, signature);
  return mid;
}

void JNIHelper::Init(JNIEnv *env) {
  JNIHelper &helper = JNIHelper::Get();
  if (helper.init_) {
    return;
  }
  CHECK(helper.java_lang_Object = CacheClass(env, "java/lang/Object"));
  CHECK(helper.java_lang_Class = CacheClass(env, "java/lang/Class"));
  CHECK(helper.java_lang_String = CacheClass(env, "java/lang/String"));

  CHECK(helper.java_lang_reflect_Method = CacheClass(env, "java/lang/reflect/Method"));
  CHECK(helper.java_lang_reflect_Field = CacheClass(env, "java/lang/reflect/Field"));

  CHECK(helper.java_lang_Object_toString =
          CacheMethod(env, helper.java_lang_Object, false, "toString", "()Ljava/lang/String;"));
  CHECK(helper.java_lang_Object_getClass =
          CacheMethod(env, helper.java_lang_Object, false, "getClass", "()Ljava/lang/Class;"));
  CHECK(helper.java_lang_Class_getName =
          CacheMethod(env, helper.java_lang_Class, false, "getName", "()Ljava/lang/String;"));
  CHECK(helper.java_lang_reflect_Method_getName =
          CacheMethod(env, helper.java_lang_reflect_Method, false, "getName", "()Ljava/lang/String;"));
  CHECK(helper.java_lang_reflect_Field_getName =
          CacheMethod(env, helper.java_lang_reflect_Field, false, "getName", "()Ljava/lang/String;"));

  helper.init_ = true;
}

void JNIHelper::Clear(JNIEnv *env) {
  JNIHelper &helper = JNIHelper::Get();
  if (helper.java_lang_Object != nullptr) {
    env->DeleteGlobalRef(helper.java_lang_Object);
    helper.java_lang_Object = nullptr;
  }

  if (helper.java_lang_Class != nullptr) {
    env->DeleteGlobalRef(helper.java_lang_Class);
    helper.java_lang_Class = nullptr;
  }
  if (helper.java_lang_reflect_Method != nullptr) {
    env->DeleteGlobalRef(helper.java_lang_reflect_Method);
    helper.java_lang_reflect_Method = nullptr;
  }
  if (helper.java_lang_reflect_Field != nullptr) {
    env->DeleteGlobalRef(helper.java_lang_reflect_Field);
    helper.java_lang_reflect_Field = nullptr;
  }

  helper.java_lang_Object_toString = nullptr;
  helper.java_lang_Object_getClass = nullptr;
  helper.java_lang_Class_getName = nullptr;
  helper.java_lang_reflect_Method_getName = nullptr;
  helper.java_lang_reflect_Field_getName = nullptr;
  helper.init_ = false;
}

JNIHelper &JNIHelper::Get() {
  static JNIHelper helper;
  return helper;
}