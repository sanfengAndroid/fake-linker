//
// Created by beich on 2020/12/18.
//

#pragma once

#include <jni.h>
#include <string>

class JNIHelper {
public:
  static std::string GetClassName(JNIEnv *env, jclass clazz);

  static std::string GetObjectClassName(JNIEnv *env, jobject obj);

  static std::string GetMethodName(JNIEnv *env, jmethodID mid);

  static std::string GetFieldName(JNIEnv *env, jfieldID fieldId);

  static bool IsClassObject(JNIEnv *env, jobject obj);

  template <typename T>
  static std::string ToString(JNIEnv *env, T value) {
    return std::to_string(value);
  }

  static std::string ToString(JNIEnv *env, jobject object);

  static std::string ToString(JNIEnv *env, jmethodID methodID);

  static std::string ToString(JNIEnv *env, jfieldID fieldID);

  static void PrintAndClearException(JNIEnv *env);

  static void ReleaseByteArray(JNIEnv *env, jbyteArray arr, jbyte *bytes);
  static void ReleaseIntArray(JNIEnv *env, jintArray arr, jint *parr);
  static jclass CacheClass(JNIEnv *env, const char *jni_class_name);
  static jmethodID CacheMethod(JNIEnv *env, jclass c, bool is_static, const char *name, const char *signature);

  static void Init(JNIEnv *env);

  static void Clear(JNIEnv *env);
  static JNIHelper &Get();

public:
  bool init_;
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
};
