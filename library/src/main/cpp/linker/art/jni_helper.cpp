//
// Created by beich on 2020/12/18.
//

#include "fakelinker/jni_helper.h"

#include <fakelinker/art_symbol.h>
#include <fakelinker/proxy_jni.h>
#include <fakelinker/scoped_utf_chars.h>

#include "../linker_symbol.h"


namespace fakelinker {

static struct CacheJNI {
  bool init = false;
  jclass java_lang_Object;
  jclass java_lang_Class;
  jclass java_lang_String;

  jclass java_lang_reflect_Method;
  jclass java_lang_reflect_Field;

  jmethodID java_lang_Object_toString;
  jmethodID java_lang_Object_getClass;
  jmethodID java_lang_reflect_Field_getName;
  jmethodID java_lang_Class_getName;
  jmethodID java_lang_reflect_Method_getName;
} cache_jni;


#define STR(x)         #x
#define MAKE_STRING(x) STR(x)
#define CHECK_EXCEPTION_BEFORE(env)                                                                                    \
  if (strict && __predict_false(env.ExceptionCheck())) {                                                               \
    return "jni_helper.cpp:" MAKE_STRING(__LINE__) ",There is an unhandled exception, "                                \
                                                   "this operation will be ignored";                                   \
  }

#define CLEAR_EXCEPTION_RETURN(env, tips)                                                                              \
  if (__predict_false(env.ExceptionCheck())) {                                                                         \
    env.ExceptionClear();                                                                                              \
    return tips;                                                                                                       \
  }

#define CHECK_JNI_NULL_OBJECT_RETURN(obj, tips)                                                                        \
  if (strict && __predict_false(obj == nullptr)) {                                                                     \
    return tips;                                                                                                       \
  }


std::string JNIHelper::GetClassName(JNIEnv *env, jclass clazz, bool strict) {
  fakelinker::ProxyJNIEnv proxy(env);

  CHECK_JNI_NULL_OBJECT_RETURN(clazz, "call GetClassName jclass is nullptr");
  CHECK_EXCEPTION_BEFORE(proxy);
  auto name = reinterpret_cast<jstring>(proxy.CallObjectMethod(clazz, cache_jni.java_lang_Class_getName));
  CLEAR_EXCEPTION_RETURN(proxy, "call Class.getName() exception occurs");
  ScopedUtfChars sof(env, name);
  return sof.c_str();
}

std::string JNIHelper::GetObjectClassName(JNIEnv *env, jobject obj, bool strict) {
  fakelinker::ProxyJNIEnv proxy(env);

  CHECK_JNI_NULL_OBJECT_RETURN(obj, "call GetObjectClassName jobject is nullptr");
  CHECK_EXCEPTION_BEFORE(proxy);
  ScopedLocalRef<jclass> clazz(&proxy, (jclass)proxy.CallObjectMethod(obj, cache_jni.java_lang_Object_getClass));
  CLEAR_EXCEPTION_RETURN(proxy, "call GetObjectClassName exception occurs");
  return GetClassName(env, clazz.get(), false);
}

std::string JNIHelper::GetMethodName(JNIEnv *env, jmethodID mid, bool strict) {
  fakelinker::ProxyJNIEnv proxy(env);

  CHECK_JNI_NULL_OBJECT_RETURN(mid, "call GetMethodName jmethodID is nullptr");
  CHECK_EXCEPTION_BEFORE(proxy);
  jobject obj = proxy.ToReflectedMethod(cache_jni.java_lang_reflect_Method, mid, false);
  ScopedLocalRef<jobject> method_obj(&proxy, obj);
  auto name =
    reinterpret_cast<jstring>(proxy.CallObjectMethod(method_obj.get(), cache_jni.java_lang_reflect_Method_getName));
  CLEAR_EXCEPTION_RETURN(proxy, "call Method.getName exception occurs");
  ScopedUtfChars sof(env, name);
  return sof.c_str();
}

std::string JNIHelper::PrettyMethod(JNIEnv *env, jmethodID mid, bool strict) {
  fakelinker::ProxyJNIEnv proxy(env);

  CHECK_JNI_NULL_OBJECT_RETURN(mid, "call PrettyMethod jmethodID is nullptr");
  CHECK_EXCEPTION_BEFORE(proxy);
  auto art = ArtSymbol::Get();
  if (art->can_pretty_method) {
    return art->PrettyMethod(mid, true);
  }
  // call toString
  return ToString(env, mid, false);
}

std::string JNIHelper::GetMethodShorty(JNIEnv *env, jmethodID mid, bool strict) {
  fakelinker::ProxyJNIEnv proxy(env);
  CHECK_JNI_NULL_OBJECT_RETURN(mid, "call PrettyMethod jmethodID is nullptr");
  // CHECK_EXCEPTION_BEFORE(proxy);
  return ArtSymbol::Get()->GetMethodShorty(mid);
}


std::string JNIHelper::GetFieldName(JNIEnv *env, jfieldID fieldId, bool strict) {
  fakelinker::ProxyJNIEnv proxy(env);
  CHECK_JNI_NULL_OBJECT_RETURN(fieldId, "call GetFieldName jfieldID is nullptr");
  CHECK_EXCEPTION_BEFORE(proxy);
  ScopedLocalRef<jobject> field_obj(env, proxy.ToReflectedField(cache_jni.java_lang_reflect_Field, fieldId, false));

  auto name =
    reinterpret_cast<jstring>(proxy.CallObjectMethod(field_obj.get(), cache_jni.java_lang_reflect_Field_getName));
  CLEAR_EXCEPTION_RETURN(proxy, "call Field.getName exception occurs");
  ScopedUtfChars sof(env, name);
  return sof.c_str();
}

std::string JNIHelper::PrettyField(JNIEnv *env, jfieldID fieldId, bool strict) {
  fakelinker::ProxyJNIEnv proxy(env);

  CHECK_JNI_NULL_OBJECT_RETURN(fieldId, "call PrettyField jmethodID is nullptr");
  CHECK_EXCEPTION_BEFORE(proxy);
  auto art = ArtSymbol::Get();
  if (art->can_pretty_field) {
    return art->PrettyField(fieldId, true);
  }
  // call toString
  return ToString(env, fieldId, false);
}


bool JNIHelper::IsClassObject(JNIEnv *env, jobject obj, bool strict) {
  fakelinker::ProxyJNIEnv proxy(env);
  CHECK_JNI_NULL_OBJECT_RETURN(obj, "call IsClassObject jobject is nullptr");
  CHECK_EXCEPTION_BEFORE(proxy);
  return proxy.IsInstanceOf(obj, cache_jni.java_lang_Class);
}


std::string JNIHelper::ToString(JNIEnv *env, jstring string, bool strict) {
  fakelinker::ProxyJNIEnv proxy(env);

  CHECK_JNI_NULL_OBJECT_RETURN(string, "call ToString jobject is nullptr");
  CHECK_EXCEPTION_BEFORE(proxy);
  ScopedUtfChars sof(env, string);
  return sof.c_str();
}

std::string JNIHelper::ToString(JNIEnv *env, jobject l, bool strict) {
  fakelinker::ProxyJNIEnv proxy(env);

  CHECK_JNI_NULL_OBJECT_RETURN(l, "call ToString jobject is nullptr");
  CHECK_EXCEPTION_BEFORE(proxy);

  auto name = reinterpret_cast<jstring>(proxy.CallObjectMethod(l, cache_jni.java_lang_Object_toString));

  CLEAR_EXCEPTION_RETURN(proxy, "call ToString exception occurs");
  ScopedUtfChars sof(env, name);
  return sof.c_str();
}

std::string JNIHelper::ToString(JNIEnv *env, jmethodID methodID, bool strict) {
  fakelinker::ProxyJNIEnv proxy(env);

  CHECK_JNI_NULL_OBJECT_RETURN(methodID, "call ToString jmethodID is nullptr");
  CHECK_EXCEPTION_BEFORE(proxy);
  ScopedLocalRef<jobject> method(&proxy, proxy.ToReflectedMethod(cache_jni.java_lang_Object, methodID, false));
  CLEAR_EXCEPTION_RETURN(proxy, "call Method.toString exception occurs");
  return ToString(env, method.get(), false);
}

std::string JNIHelper::ToString(JNIEnv *env, jfieldID fieldID, bool strict) {
  fakelinker::ProxyJNIEnv proxy(env);
  CHECK_JNI_NULL_OBJECT_RETURN(fieldID, "call ToString jfieldID is nullptr");
  CHECK_EXCEPTION_BEFORE(proxy);

  ScopedLocalRef<jobject> filed(&proxy, proxy.ToReflectedField(cache_jni.java_lang_Object, fieldID, false));

  CLEAR_EXCEPTION_RETURN(proxy, "call Field.toString exception occurs");
  return ToString(env, filed.get(), false);
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

jclass JNIHelper::CacheClass(JNIEnv *env, const char *jni_class_name) {
  ScopedLocalRef<jclass> c(env, env->FindClass(jni_class_name));
  if (c.get() == nullptr) {
    LOGE("Couldn't find class: %s", jni_class_name);
    return nullptr;
  }
  jclass result = reinterpret_cast<jclass>(env->NewGlobalRef(c.get()));
  LOGD("cache class: %s => %p", jni_class_name, result);
  return result;
}

jmethodID JNIHelper::CacheMethod(JNIEnv *env, jclass c, bool is_static, const char *name, const char *signature) {
  jmethodID mid;
  mid = is_static ? env->GetStaticMethodID(c, name, signature) : env->GetMethodID(c, name, signature);
  LOGD("cache method %s%s => %p", name, signature, mid);
  return mid;
}

void JNIHelper::Init(JNIEnv *env) {
  if (cache_jni.init) {
    return;
  }
  CHECK(cache_jni.java_lang_Object = CacheClass(env, "java/lang/Object"));
  CHECK(cache_jni.java_lang_Class = CacheClass(env, "java/lang/Class"));
  CHECK(cache_jni.java_lang_String = CacheClass(env, "java/lang/String"));

  CHECK(cache_jni.java_lang_reflect_Method = CacheClass(env, "java/lang/reflect/Method"));
  CHECK(cache_jni.java_lang_reflect_Field = CacheClass(env, "java/lang/reflect/Field"));

  CHECK(cache_jni.java_lang_Object_toString =
          CacheMethod(env, cache_jni.java_lang_Object, false, "toString", "()Ljava/lang/String;"));
  CHECK(cache_jni.java_lang_Object_getClass =
          CacheMethod(env, cache_jni.java_lang_Object, false, "getClass", "()Ljava/lang/Class;"));
  CHECK(cache_jni.java_lang_Class_getName =
          CacheMethod(env, cache_jni.java_lang_Class, false, "getName", "()Ljava/lang/String;"));
  CHECK(cache_jni.java_lang_reflect_Method_getName =
          CacheMethod(env, cache_jni.java_lang_reflect_Method, false, "getName", "()Ljava/lang/String;"));
  CHECK(cache_jni.java_lang_reflect_Field_getName =
          CacheMethod(env, cache_jni.java_lang_reflect_Field, false, "getName", "()Ljava/lang/String;"));

  cache_jni.init = true;
  if (fakelinker::linker_symbol.solist.pointer) {
    fakelinker::ArtSymbol::Init(env);
  }
}

void JNIHelper::Clear(JNIEnv *env) {
  if (cache_jni.java_lang_Object != nullptr) {
    env->DeleteGlobalRef(cache_jni.java_lang_Object);
    cache_jni.java_lang_Object = nullptr;
  }

  if (cache_jni.java_lang_Class != nullptr) {
    env->DeleteGlobalRef(cache_jni.java_lang_Class);
    cache_jni.java_lang_Class = nullptr;
  }
  if (cache_jni.java_lang_reflect_Method != nullptr) {
    env->DeleteGlobalRef(cache_jni.java_lang_reflect_Method);
    cache_jni.java_lang_reflect_Method = nullptr;
  }
  if (cache_jni.java_lang_reflect_Field != nullptr) {
    env->DeleteGlobalRef(cache_jni.java_lang_reflect_Field);
    cache_jni.java_lang_reflect_Field = nullptr;
  }

  cache_jni.java_lang_Object_toString = nullptr;
  cache_jni.java_lang_Object_getClass = nullptr;
  cache_jni.java_lang_Class_getName = nullptr;
  cache_jni.java_lang_reflect_Method_getName = nullptr;
  cache_jni.java_lang_reflect_Field_getName = nullptr;
  cache_jni.init = false;
}
} // namespace fakelinker