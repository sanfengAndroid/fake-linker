//
// Created by beich on 2020/12/18.
//
#pragma once

#include <jni.h>

#include <string>

namespace fakelinker {


class JNIHelper {
public:
  static std::string GetClassName(JNIEnv *env, jclass clazz, bool strict = true);

  static std::string GetObjectClassName(JNIEnv *env, jobject obj, bool strict = true);

  static std::string GetMethodName(JNIEnv *env, jmethodID mid, bool strict = true);

  static std::string PrettyMethod(JNIEnv *env, jmethodID mid, bool strict = true);

  static std::string GetMethodShorty(JNIEnv *env, jmethodID mid, bool strict = true);

  static std::string GetFieldName(JNIEnv *env, jfieldID fieldId, bool strict = true);

  static std::string PrettyField(JNIEnv *env, jfieldID fieldId, bool strict = true);

  static bool IsClassObject(JNIEnv *env, jobject obj, bool strict = true);

  template <typename T>
  static std::string ToString(T value) {
    return std::to_string(value);
  }

  static std::string ToString(JNIEnv *env, jstring string, bool strict = true);

  static std::string ToString(JNIEnv *env, jobject object, bool strict = true);

  static std::string ToString(JNIEnv *env, jmethodID methodID, bool strict = true);

  static std::string ToString(JNIEnv *env, jfieldID fieldID, bool strict = true);

  static void PrintAndClearException(JNIEnv *env);

  static void ReleaseByteArray(JNIEnv *env, jbyteArray arr, jbyte *bytes);
  static void ReleaseIntArray(JNIEnv *env, jintArray arr, jint *parr);
  static jclass CacheClass(JNIEnv *env, const char *jni_class_name);
  static jmethodID CacheMethod(JNIEnv *env, jclass c, bool is_static, const char *name, const char *signature);

  static void Init(JNIEnv *env);

  static void Clear(JNIEnv *env);

private:
  JNIHelper() = delete;
};
} // namespace fakelinker