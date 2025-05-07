#pragma once

#include "trace_jni.h"

namespace fakelinker {
class DefaultTraceJNICallback : public BaseTraceJNICallback {
public:
  explicit DefaultTraceJNICallback(bool strict) : BaseTraceJNICallback(strict) {}

  ~DefaultTraceJNICallback() override = default;

  void TraceLog(std::string_view message) override;

  template <typename ReturnType, bool AllowAccessArgs>
  void TraceStart(TraceInvokeContext<ReturnType, AllowAccessArgs> &context, const char *name) {
    TraceLine(context, "----------- %s: %p -----------", name, context.caller);
  }

  template <typename ReturnType, bool AllowAccessArgs>
  void TraceEnd(TraceInvokeContext<ReturnType, AllowAccessArgs> &context) {
    TraceLine(context, "----------------- End -----------------");
    constexpr size_t max_message_length = sizeof(TraceInvokeContext<ReturnType, AllowAccessArgs>::message);
    if (context.message_length < max_message_length) {
      context.message[context.message_length++] = '\0';
    } else {
      context.message[max_message_length - 1] = '\0';
    }
    TraceLog(std::string_view(context.message, context.message_length));
  }

  template <typename ReturnType, bool AllowAccessArgs>
  void TraceLine(TraceInvokeContext<ReturnType, AllowAccessArgs> &context, std::string_view line, ...) {
    constexpr size_t max_message_length = sizeof(TraceInvokeContext<ReturnType, AllowAccessArgs>::message);
    va_list args;
    va_start(args, line);
    int len = vsnprintf(context.message + context.message_length, max_message_length - context.message_length,
                        line.data(), args);
    if (len >= 0) {
      context.message_length += len;
    }
    if (context.message_length < max_message_length) {
      context.message[context.message_length++] = '\n';
    }
    va_end(args);
  }

  template <typename T, bool AllowAccessArgs>
  void FormatReturnValue(TraceInvokeContext<T, AllowAccessArgs> &context) {
    if constexpr (std::is_same_v<T, const char *>) {
      TraceLine(context, "  return  const char*   %s", context.result);
    } else if constexpr (std::is_same_v<T, jboolean>) {
      TraceLine(context, "  return  jboolean      %s", context.result ? "true" : "false");
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jboolean *>) {
      TraceLine(context, "  return  jboolean*     %12p", context.result);
    } else if constexpr (std::is_same_v<T, jbyte>) {
      TraceLine(context, "  return  jbyte         %d", context.result);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jbyte *>) {
      TraceLine(context, "  return  jbyte*        %12p", context.result);
    } else if constexpr (std::is_same_v<T, jchar>) {
      TraceLine(context, "  return  jchar         %d", context.result);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jchar *>) {
      TraceLine(context, "  return  jchar*        %12p", context.result);
    } else if constexpr (std::is_same_v<T, jshort>) {
      TraceLine(context, "  return  jshort        %d", context.result);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jshort *>) {
      TraceLine(context, "  return  jshort*       %12p", context.result);
    } else if constexpr (std::is_same_v<T, jint>) {
      TraceLine(context, "  return  jint          %d", context.result);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jint *>) {
      TraceLine(context, "  return  jint*         %12p", context.result);
    } else if constexpr (std::is_same_v<T, jlong>) {
      TraceLine(context, "  return  jlong         %ld", context.result);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jlong *>) {
      TraceLine(context, "  return  jlong*        %12p", context.result);
    } else if constexpr (std::is_same_v<T, jfloat>) {
      TraceLine(context, "  return  jfloat        %f", context.result);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jfloat *>) {
      TraceLine(context, "  return  jfloat*       %12p", context.result);
    } else if constexpr (std::is_same_v<T, jdouble>) {
      TraceLine(context, "  return  jdouble       %f", context.result);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jdouble *>) {
      TraceLine(context, "  return  jdouble*      %12p", context.result);
    } else if constexpr (std::is_same_v<T, jobject>) {
      TraceLine(context, "  return  jobject       %12p : %s", context.result,
                FormatObjectClass(context.env, context.result, true).c_str());
    } else if constexpr (std::is_same_v<T, jclass>) {
      TraceLine(context, "  return  jclass        %12p : %s", context.result,
                FormatClass(context.env, context.result, true).c_str());
    } else if constexpr (std::is_same_v<T, jstring>) {
      TraceLine(context, "  return  jstring       %12p : %s", context.result,
                FormatString(context.env, context.result, true).c_str());
    } else if constexpr (std::is_same_v<T, jmethodID>) {
      TraceLine(context, "  return  jmethodID     %12p : %s", context.result,
                FormatMethodID(context.env, context.result, true));
    } else if constexpr (std::is_same_v<T, jfieldID>) {
      TraceLine(context, "  return  jfieldID      %12p : %s", context.result,
                FormatFieldID(context.env, context.result, true));
    } else if constexpr (std::is_same_v<T, jarray>) {
      TraceLine(context, "  return  jarray        %12p", context.result);
    } else if constexpr (std::is_same_v<T, jobjectArray>) {
      TraceLine(context, "  return  jobjectArray  %12p", context.result);
    } else if constexpr (std::is_same_v<T, jbooleanArray>) {
      TraceLine(context, "  return  jbooleanArray %12p", context.result);
    } else if constexpr (std::is_same_v<T, jbyteArray>) {
      TraceLine(context, "  return  jbyteArray    %12p", context.result);
    } else if constexpr (std::is_same_v<T, jcharArray>) {
      TraceLine(context, "  return  jcharArray    %12p", context.result);
    } else if constexpr (std::is_same_v<T, jshortArray>) {
      TraceLine(context, "  return  jshortArray   %12p", context.result);
    } else if constexpr (std::is_same_v<T, jintArray>) {
      TraceLine(context, "  return  jintArray     %12p", context.result);
    } else if constexpr (std::is_same_v<T, jlongArray>) {
      TraceLine(context, "  return  jlongArray    %12p", context.result);
    } else if constexpr (std::is_same_v<T, jfloatArray>) {
      TraceLine(context, "  return  jfloatArray   %12p", context.result);
    } else if constexpr (std::is_same_v<T, jdoubleArray>) {
      TraceLine(context, "  return  jdoubleArray  %12p", context.result);
    } else if constexpr (std::is_same_v<T, jthrowable>) {
      TraceLine(context, "  return  jthrowable    %12p", context.result);
    } else if constexpr (std::is_same_v<T, jweak>) {
      TraceLine(context, "  return  jweak         %12p", context.result);
    } else if constexpr (std::is_same_v<T, va_list>) {
      TraceLine(context, "  return  va_list");
    } else if constexpr (std::is_same_v<T, const jvalue *>) {
      TraceLine(context, "  return  const jvalue* %12p", context.result);
    } else if constexpr (std::is_void_v<T>) {
      TraceLine(context, "  return  void");
    } else {
      TraceLine(context, "  --> unknown return type");
    }
  }

  template <typename ReturnType, typename T>
  void FormatJNIArgument(TraceInvokeContext<ReturnType, true> &context, int index, T value) {
    if constexpr (std::is_same_v<T, const char *>) {
      TraceLine(context, "  args[%d] const char*   %s", index, value);
    } else if constexpr (std::is_same_v<T, jboolean>) {
      TraceLine(context, "  args[%d] jboolean      %s", index, value ? "true" : "false");
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jboolean *>) {
      TraceLine(context, "  args[%d] jboolean*     %12p", index, value);
    } else if constexpr (std::is_same_v<T, jbyte>) {
      TraceLine(context, "  args[%d] jbyte         %d", index, value);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jbyte *>) {
      TraceLine(context, "  args[%d] jbyte*        %12p", index, value);
    } else if constexpr (std::is_same_v<T, jchar>) {
      TraceLine(context, "  args[%d] jchar         %d", index, value);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jchar *>) {
      TraceLine(context, "  args[%d] jchar*        %12p", index, value);
    } else if constexpr (std::is_same_v<T, jshort>) {
      TraceLine(context, "  args[%d] jshort        %d", index, value);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jshort *>) {
      TraceLine(context, "  args[%d] jshort*       %12p", index, value);
    } else if constexpr (std::is_same_v<T, jint>) {
      TraceLine(context, "  args[%d] jint          %d", index, value);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jint *>) {
      TraceLine(context, "  args[%d] jint*         %12p", index, value);
    } else if constexpr (std::is_same_v<T, jlong>) {
      TraceLine(context, "  args[%d] jlong         %ld", index, value);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jlong *>) {
      TraceLine(context, "  args[%d] jlong*        %12p", index, value);
    } else if constexpr (std::is_same_v<T, jfloat>) {
      TraceLine(context, "  args[%d] jfloat        %f", index, value);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jfloat *>) {
      TraceLine(context, "  args[%d] jfloat*       %12p", index, value);
    } else if constexpr (std::is_same_v<T, jdouble>) {
      TraceLine(context, "  args[%d] jdouble       %f", index, value);
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, jdouble *>) {
      TraceLine(context, "  args[%d] jdouble*      %12p", index, value);
    } else if constexpr (std::is_same_v<T, jobject>) {
      TraceLine(context, "  args[%d] jobject       %12p : %s", index, value,
                FormatObjectClass(context.env, value, strict_mode_).c_str());
    } else if constexpr (std::is_same_v<T, jclass>) {
      TraceLine(context, "  args[%d] jclass        %12p : %s", index, value,
                FormatClass(context.env, value, strict_mode_).c_str());
    } else if constexpr (std::is_same_v<T, jstring>) {
      TraceLine(context, "  args[%d] jstring       %12p : %s", index, value,
                FormatString(context.env, value, strict_mode_).c_str());
    } else if constexpr (std::is_same_v<T, jmethodID>) {
      TraceLine(context, "  args[%d] jmethodID     %12p : %s", index, value,
                FormatMethodID(context.env, value, strict_mode_));
    } else if constexpr (std::is_same_v<T, jfieldID>) {
      TraceLine(context, "  args[%d] jfieldID      %12p : %s", index, value,
                FormatFieldID(context.env, value, strict_mode_));
    } else if constexpr (std::is_same_v<T, jarray>) {
      TraceLine(context, "  args[%d] jarray        %12p", index, value);
    } else if constexpr (std::is_same_v<T, jobjectArray>) {
      TraceLine(context, "  args[%d] jobjectArray  %12p", index, value);
    } else if constexpr (std::is_same_v<T, jbooleanArray>) {
      TraceLine(context, "  args[%d] jbooleanArray %12p", index, value);
    } else if constexpr (std::is_same_v<T, jbyteArray>) {
      TraceLine(context, "  args[%d] jbyteArray    %12p", index, value);
    } else if constexpr (std::is_same_v<T, jcharArray>) {
      TraceLine(context, "  args[%d] jcharArray    %12p", index, value);
    } else if constexpr (std::is_same_v<T, jshortArray>) {
      TraceLine(context, "  args[%d] jshortArray   %12p", index, value);
    } else if constexpr (std::is_same_v<T, jintArray>) {
      TraceLine(context, "  args[%d] jintArray     %12p", index, value);
    } else if constexpr (std::is_same_v<T, jlongArray>) {
      TraceLine(context, "  args[%d] jlongArray    %12p", index, value);
    } else if constexpr (std::is_same_v<T, jfloatArray>) {
      TraceLine(context, "  args[%d] jfloatArray   %12p", index, value);
    } else if constexpr (std::is_same_v<T, jdoubleArray>) {
      TraceLine(context, "  args[%d] jdoubleArray  %12p", index, value);
    } else if constexpr (std::is_same_v<T, jthrowable>) {
      TraceLine(context, "  args[%d] jthrowable    %12p", index, value);
    } else if constexpr (std::is_same_v<T, jweak>) {
      TraceLine(context, "  args[%d] jweak         %12p", index, value);

    } else if constexpr (std::is_same_v<T, va_list>) {
      std::string shorty = GetMethodShorty(context.env, context.method, true);
      TraceLine(context, "  args[%d] va_list shorty: %s", index, shorty.c_str());
      if (!shorty.empty()) {
        // shorty[0] is return type, so skip it
        for (size_t i = 1; i < shorty.size(); ++i) {
          switch (shorty[i]) {
          case 'Z':
            TraceLine(context, "    args[%d] jboolean    %s", index++, va_arg(value, jint));
            break;
          case 'B':
            TraceLine(context, "    args[%d] jbyte       %d", index++, va_arg(value, jint));
            break;
          case 'C':
            TraceLine(context, "    args[%d] jchar       %d", index++, va_arg(value, jint));
            break;
          case 'S':
            TraceLine(context, "    args[%d] jshort      %d", index++, va_arg(value, jint));
            break;
          case 'I':
            TraceLine(context, "    args[%d] jint        %d", index++, va_arg(value, jint));
            break;
          case 'F':
            TraceLine(context, "    args[%d] jfloat      %f", index++, va_arg(value, jdouble));
            break;
          case 'L': {
            jobject arg = va_arg(value, jobject);
            TraceLine(context, "    args[%d] jobject     %12p : %s", index++, arg,
                      FormatObjectClass(context.env, arg, strict_mode_).c_str());
          } break;
          case 'D':
            TraceLine(context, "    args[%d] jdouble     %f", index++, va_arg(value, jdouble));
            break;
          case 'J':
            TraceLine(context, "    args[%d] jlong       %ld", index++, va_arg(value, jlong));
            break;
          default:
            TraceLine(context, "    args[%d] unknown type, possible reading error of short method", index++);
            break;
          }
        }
      }
    } else if constexpr (std::is_same_v<T, const jvalue *>) {
      std::string shorty = GetMethodShorty(context.env, context.method, true);
      TraceLine(context, "  args[%d] jvalue* shorty : %s", index, shorty.c_str());
      if (!shorty.empty()) {
        // shorty[0] is return type, so skip it
        for (size_t i = 1; i < shorty.size(); ++i) {
          switch (shorty[i]) {
          case 'Z':
            TraceLine(context, "    args[%d] jboolean    %s", index++, value[i - 1].z);
            break;
          case 'B':
            TraceLine(context, "    args[%d] jbyte       %d", index++, value[i - 1].b);
            break;
          case 'C':
            TraceLine(context, "    args[%d] jchar       %d", index++, value[i - 1].c);
            break;
          case 'S':
            TraceLine(context, "    args[%d] jshort      %d", index++, value[i - 1].s);
            break;
          case 'I':
            TraceLine(context, "    args[%d] jint        %d", index++, value[i - 1].i);
            break;
          case 'F':
            TraceLine(context, "    args[%d] jfloat      %d", index++, value[i - 1].i);
            break;
          case 'L': {
            jobject arg = value[i - 1].l;
            TraceLine(context, "    args[%d] jobject     %12p : %s", index++, arg,
                      FormatObjectClass(context.env, arg, strict_mode_).c_str());
          } break;
          case 'D':
            TraceLine(context, "    args[%d] jdouble     %ld", index++, value[i - 1].j);
            break;
          case 'J':
            TraceLine(context, "    args[%d] jlong       %ld", index++, value[i - 1].j);
            break;
          default:
            TraceLine(context, "    args[%d] unknown type, possible reading error of short method", index++);
            break;
          }
        }
      }
    } else {
      TraceLine(context, "  args[%d] unknown type", index);
    }
  }

  template <typename ReturnType, typename... Args>
  void FormatJNIArguments(const char *name, TraceInvokeContext<ReturnType, true> &context, Args... args) {
    TraceStart(context, name);
    int index = 0;
    (FormatJNIArgument(context, index++, std::forward<Args>(args)), ...);
    FormatReturnValue(context);
    TraceEnd(context);
  }

  bool Register();

  void FindClass(TraceInvokeContext<jclass> &context, const char *name) override;

  void GetSuperclass(TraceInvokeContext<jclass> &context, jclass clazz) override;

  void NewGlobalRef(TraceInvokeContext<jobject> &context, jobject obj) override;

  void DeleteGlobalRef(TraceInvokeContext<void> &context, jobject globalRef) override;

  void NewObjectV(TraceInvokeContext<jobject> &context, jclass clazz, jmethodID methodID, va_list args) override;

  void NewObjectA(TraceInvokeContext<jobject> &context, jclass clazz, jmethodID methodID, const jvalue *args) override;

  void GetObjectClass(TraceInvokeContext<jclass> &context, jobject obj) override;

  void GetMethodID(TraceInvokeContext<jmethodID> &context, jclass clazz, const char *name, const char *sig) override;

  void CallObjectMethodV(TraceInvokeContext<jobject> &context, jobject obj, jmethodID methodID, va_list args) override;

  void CallObjectMethodA(TraceInvokeContext<jobject> &context, jobject obj, jmethodID methodID,
                         const jvalue *args) override;

  void CallBooleanMethodV(TraceInvokeContext<jboolean> &context, jobject obj, jmethodID methodID,
                          va_list args) override;

  void CallBooleanMethodA(TraceInvokeContext<jboolean> &context, jobject obj, jmethodID methodID,
                          const jvalue *args) override;

  void CallByteMethodV(TraceInvokeContext<jbyte> &context, jobject obj, jmethodID methodID, va_list args) override;

  void CallByteMethodA(TraceInvokeContext<jbyte> &context, jobject obj, jmethodID methodID,
                       const jvalue *args) override;

  void CallCharMethodV(TraceInvokeContext<jchar> &context, jobject obj, jmethodID methodID, va_list args) override;

  void CallCharMethodA(TraceInvokeContext<jchar> &context, jobject obj, jmethodID methodID,
                       const jvalue *args) override;

  void CallShortMethodV(TraceInvokeContext<jshort> &context, jobject obj, jmethodID methodID, va_list args) override;

  void CallShortMethodA(TraceInvokeContext<jshort> &context, jobject obj, jmethodID methodID,
                        const jvalue *args) override;

  void CallIntMethodV(TraceInvokeContext<jint> &context, jobject obj, jmethodID methodID, va_list args) override;

  void CallIntMethodA(TraceInvokeContext<jint> &context, jobject obj, jmethodID methodID, const jvalue *args) override;

  void CallLongMethodV(TraceInvokeContext<jlong> &context, jobject obj, jmethodID methodID, va_list args) override;

  void CallLongMethodA(TraceInvokeContext<jlong> &context, jobject obj, jmethodID methodID,
                       const jvalue *args) override;

  void CallFloatMethodV(TraceInvokeContext<jfloat> &context, jobject obj, jmethodID methodID, va_list args) override;

  void CallFloatMethodA(TraceInvokeContext<jfloat> &context, jobject obj, jmethodID methodID,
                        const jvalue *args) override;

  void CallDoubleMethodV(TraceInvokeContext<jdouble> &context, jobject obj, jmethodID methodID, va_list args) override;

  void CallDoubleMethodA(TraceInvokeContext<jdouble> &context, jobject obj, jmethodID methodID,
                         const jvalue *args) override;

  void CallVoidMethodV(TraceInvokeContext<void> &context, jobject obj, jmethodID methodID, va_list args) override;

  void CallVoidMethodA(TraceInvokeContext<void> &context, jobject obj, jmethodID methodID, const jvalue *args) override;

  void CallNonvirtualObjectMethodV(TraceInvokeContext<jobject> &context, jobject obj, jclass clazz, jmethodID methodID,
                                   va_list args) override;

  void CallNonvirtualObjectMethodA(TraceInvokeContext<jobject> &context, jobject obj, jclass clazz, jmethodID methodID,
                                   const jvalue *args) override;

  void CallNonvirtualBooleanMethodV(TraceInvokeContext<jboolean> &context, jobject obj, jclass clazz,
                                    jmethodID methodID, va_list args) override;

  void CallNonvirtualBooleanMethodA(TraceInvokeContext<jboolean> &context, jobject obj, jclass clazz,
                                    jmethodID methodID, const jvalue *args) override;

  void CallNonvirtualByteMethodV(TraceInvokeContext<jbyte> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 va_list args) override;

  void CallNonvirtualByteMethodA(TraceInvokeContext<jbyte> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 const jvalue *args) override;

  void CallNonvirtualCharMethodV(TraceInvokeContext<jchar> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 va_list args) override;

  void CallNonvirtualCharMethodA(TraceInvokeContext<jchar> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 const jvalue *args) override;

  void CallNonvirtualShortMethodV(TraceInvokeContext<jshort> &context, jobject obj, jclass clazz, jmethodID methodID,
                                  va_list args) override;

  void CallNonvirtualShortMethodA(TraceInvokeContext<jshort> &context, jobject obj, jclass clazz, jmethodID methodID,
                                  const jvalue *args) override;

  void CallNonvirtualIntMethodV(TraceInvokeContext<jint> &context, jobject obj, jclass clazz, jmethodID methodID,
                                va_list args) override;

  void CallNonvirtualIntMethodA(TraceInvokeContext<jint> &context, jobject obj, jclass clazz, jmethodID methodID,
                                const jvalue *args) override;

  void CallNonvirtualLongMethodV(TraceInvokeContext<jlong> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 va_list args) override;

  void CallNonvirtualLongMethodA(TraceInvokeContext<jlong> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 const jvalue *args) override;

  void CallNonvirtualFloatMethodV(TraceInvokeContext<jfloat> &context, jobject obj, jclass clazz, jmethodID methodID,
                                  va_list args) override;

  void CallNonvirtualFloatMethodA(TraceInvokeContext<jfloat> &context, jobject obj, jclass clazz, jmethodID methodID,
                                  const jvalue *args) override;

  void CallNonvirtualDoubleMethodV(TraceInvokeContext<jdouble> &context, jobject obj, jclass clazz, jmethodID methodID,
                                   va_list args) override;

  void CallNonvirtualDoubleMethodA(TraceInvokeContext<jdouble> &context, jobject obj, jclass clazz, jmethodID methodID,
                                   const jvalue *args) override;

  void CallNonvirtualVoidMethodV(TraceInvokeContext<void> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 va_list args) override;

  void CallNonvirtualVoidMethodA(TraceInvokeContext<void> &context, jobject obj, jclass clazz, jmethodID methodID,
                                 const jvalue *args) override;

  void GetFieldID(TraceInvokeContext<jfieldID> &context, jclass clazz, const char *name, const char *sig) override;

  void GetObjectField(TraceInvokeContext<jobject> &context, jobject obj, jfieldID fieldID) override;

  void GetBooleanField(TraceInvokeContext<jboolean> &context, jobject obj, jfieldID fieldID) override;

  void GetByteField(TraceInvokeContext<jbyte> &context, jobject obj, jfieldID fieldID) override;

  void GetCharField(TraceInvokeContext<jchar> &context, jobject obj, jfieldID fieldID) override;

  void GetIntField(TraceInvokeContext<jint> &context, jobject obj, jfieldID fieldID) override;

  void GetLongField(TraceInvokeContext<jlong> &context, jobject obj, jfieldID fieldID) override;

  void GetFloatField(TraceInvokeContext<jfloat> &context, jobject obj, jfieldID fieldID) override;

  void GetDoubleField(TraceInvokeContext<jdouble> &context, jobject obj, jfieldID fieldID) override;

  void SetObjectField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jobject value) override;

  void SetBooleanField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jboolean value) override;

  void SetByteField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jbyte value) override;

  void SetCharField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jchar value) override;

  void SetShortField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jshort value) override;

  void SetIntField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jint value) override;

  void SetLongField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jlong value) override;

  void SetFloatField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jfloat value) override;

  void SetDoubleField(TraceInvokeContext<void> &context, jobject obj, jfieldID fieldID, jdouble value) override;

  void GetStaticMethodID(TraceInvokeContext<jmethodID> &context, jclass clazz, const char *name,
                         const char *sig) override;

  void CallStaticObjectMethodV(TraceInvokeContext<jobject> &context, jclass clazz, jmethodID methodID,
                               va_list args) override;

  void CallStaticObjectMethodA(TraceInvokeContext<jobject> &context, jclass clazz, jmethodID methodID,
                               const jvalue *args) override;

  void CallStaticBooleanMethodV(TraceInvokeContext<jboolean> &context, jclass clazz, jmethodID methodID,
                                va_list args) override;

  void CallStaticBooleanMethodA(TraceInvokeContext<jboolean> &context, jclass clazz, jmethodID methodID,
                                const jvalue *args) override;

  void CallStaticByteMethodV(TraceInvokeContext<jbyte> &context, jclass clazz, jmethodID methodID,
                             va_list args) override;

  void CallStaticByteMethodA(TraceInvokeContext<jbyte> &context, jclass clazz, jmethodID methodID,
                             const jvalue *args) override;

  void CallStaticCharMethodV(TraceInvokeContext<jchar> &context, jclass clazz, jmethodID methodID,
                             va_list args) override;

  void CallStaticCharMethodA(TraceInvokeContext<jchar> &context, jclass clazz, jmethodID methodID,
                             const jvalue *args) override;

  void CallStaticShortMethodV(TraceInvokeContext<jshort> &context, jclass clazz, jmethodID methodID,
                              va_list args) override;

  void CallStaticShortMethodA(TraceInvokeContext<jshort> &context, jclass clazz, jmethodID methodID,
                              const jvalue *args) override;

  void CallStaticIntMethodV(TraceInvokeContext<jint> &context, jclass clazz, jmethodID methodID, va_list args) override;

  void CallStaticIntMethodA(TraceInvokeContext<jint> &context, jclass clazz, jmethodID methodID,
                            const jvalue *args) override;

  void CallStaticLongMethodV(TraceInvokeContext<jlong> &context, jclass clazz, jmethodID methodID,
                             va_list args) override;

  void CallStaticLongMethodA(TraceInvokeContext<jlong> &context, jclass clazz, jmethodID methodID,
                             const jvalue *args) override;

  void CallStaticFloatMethodV(TraceInvokeContext<jfloat> &context, jclass clazz, jmethodID methodID,
                              va_list args) override;

  void CallStaticFloatMethodA(TraceInvokeContext<jfloat> &context, jclass clazz, jmethodID methodID,
                              const jvalue *args) override;

  void CallStaticDoubleMethodV(TraceInvokeContext<jdouble> &context, jclass clazz, jmethodID methodID,
                               va_list args) override;

  void CallStaticDoubleMethodA(TraceInvokeContext<jdouble> &context, jclass clazz, jmethodID methodID,
                               const jvalue *args) override;

  void CallStaticVoidMethodV(TraceInvokeContext<void> &context, jclass clazz, jmethodID methodID,
                             va_list args) override;

  void CallStaticVoidMethodA(TraceInvokeContext<void> &context, jclass clazz, jmethodID methodID,
                             const jvalue *args) override;

  void GetStaticFieldID(TraceInvokeContext<jfieldID> &context, jclass clazz, const char *name,
                        const char *sig) override;

  void GetStaticObjectField(TraceInvokeContext<jobject> &context, jclass clazz, jfieldID fieldID) override;

  void GetStaticBooleanField(TraceInvokeContext<jboolean> &context, jclass clazz, jfieldID fieldID) override;

  void GetStaticByteField(TraceInvokeContext<jbyte> &context, jclass clazz, jfieldID fieldID) override;

  void GetStaticCharField(TraceInvokeContext<jchar> &context, jclass clazz, jfieldID fieldID) override;

  void GetStaticShortField(TraceInvokeContext<jshort> &context, jclass clazz, jfieldID fieldID) override;

  void GetStaticIntField(TraceInvokeContext<jint> &context, jclass clazz, jfieldID fieldID) override;

  void GetStaticLongField(TraceInvokeContext<jlong> &context, jclass clazz, jfieldID fieldID) override;

  void GetStaticFloatField(TraceInvokeContext<jfloat> &context, jclass clazz, jfieldID fieldID) override;

  void GetStaticDoubleField(TraceInvokeContext<jdouble> &context, jclass clazz, jfieldID fieldID) override;

  void SetStaticObjectField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jobject value) override;

  void SetStaticBooleanField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID,
                             jboolean value) override;

  void SetStaticByteField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jbyte value) override;

  void SetStaticCharField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jchar value) override;

  void SetStaticShortField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jshort value) override;

  void SetStaticIntField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jint value) override;

  void SetStaticLongField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jlong value) override;

  void SetStaticFloatField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jfloat value) override;

  void SetStaticDoubleField(TraceInvokeContext<void> &context, jclass clazz, jfieldID fieldID, jdouble value) override;

  void NewString(TraceInvokeContext<jstring> &context, const jchar *unicodeChars, jsize len) override;

  void GetStringLength(TraceInvokeContext<jsize> &context, jstring string) override;

  void GetStringChars(TraceInvokeContext<const jchar *> &context, jstring string, jboolean *isCopy) override;

  void NewStringUTF(TraceInvokeContext<jstring> &context, const char *bytes) override;

  void GetStringUTFLength(TraceInvokeContext<jsize> &context, jstring string) override;

  void GetStringUTFChars(TraceInvokeContext<const char *> &context, jstring string, jboolean *isCopy) override;

  void GetArrayLength(TraceInvokeContext<jsize> &context, jarray array) override;

  void NewObjectArray(TraceInvokeContext<jobjectArray> &context, jsize length, jclass elementClass,
                      jobject initialElement) override;

  void GetObjectArrayElement(TraceInvokeContext<jobject> &context, jobjectArray array, jsize index) override;

  void SetObjectArrayElement(TraceInvokeContext<void> &context, jobjectArray array, jsize index,
                             jobject value) override;

  void NewBooleanArray(TraceInvokeContext<jbooleanArray> &context, jsize length) override;

  void NewByteArray(TraceInvokeContext<jbyteArray> &context, jsize length) override;

  void NewCharArray(TraceInvokeContext<jcharArray> &context, jsize length) override;

  void NewShortArray(TraceInvokeContext<jshortArray> &context, jsize length) override;

  void NewIntArray(TraceInvokeContext<jintArray> &context, jsize length) override;

  void NewLongArray(TraceInvokeContext<jlongArray> &context, jsize length) override;

  void NewFloatArray(TraceInvokeContext<jfloatArray> &context, jsize length) override;

  void NewDoubleArray(TraceInvokeContext<jdoubleArray> &context, jsize length) override;

  void GetBooleanArrayElements(TraceInvokeContext<jboolean *> &context, jbooleanArray array, jboolean *isCopy) override;

  void GetByteArrayElements(TraceInvokeContext<jbyte *> &context, jbyteArray array, jboolean *isCopy) override;

  void GetCharArrayElements(TraceInvokeContext<jchar *> &context, jcharArray array, jboolean *isCopy) override;

  void GetShortArrayElements(TraceInvokeContext<jshort *> &context, jshortArray array, jboolean *isCopy) override;

  void GetIntArrayElements(TraceInvokeContext<jint *> &context, jintArray array, jboolean *isCopy) override;

  void GetLongArrayElements(TraceInvokeContext<jlong *> &context, jlongArray array, jboolean *isCopy) override;

  void GetFloatArrayElements(TraceInvokeContext<jfloat *> &context, jfloatArray array, jboolean *isCopy) override;

  void GetDoubleArrayElements(TraceInvokeContext<jdouble *> &context, jdoubleArray array, jboolean *isCopy) override;

  void GetBooleanArrayRegion(TraceInvokeContext<void> &context, jbooleanArray array, jsize start, jsize len,
                             jboolean *buf) override;

  void GetByteArrayRegion(TraceInvokeContext<void> &context, jbyteArray array, jsize start, jsize len,
                          jbyte *buf) override;

  void GetCharArrayRegion(TraceInvokeContext<void> &context, jcharArray array, jsize start, jsize len,
                          jchar *buf) override;

  void GetShortArrayRegion(TraceInvokeContext<void> &context, jshortArray array, jsize start, jsize len,
                           jshort *buf) override;

  void GetIntArrayRegion(TraceInvokeContext<void> &context, jintArray array, jsize start, jsize len,
                         jint *buf) override;

  void GetLongArrayRegion(TraceInvokeContext<void> &context, jlongArray array, jsize start, jsize len,
                          jlong *buf) override;

  void GetFloatArrayRegion(TraceInvokeContext<void> &context, jfloatArray array, jsize start, jsize len,
                           jfloat *buf) override;

  void GetDoubleArrayRegion(TraceInvokeContext<void> &context, jdoubleArray array, jsize start, jsize len,
                            jdouble *buf) override;

  void SetBooleanArrayRegion(TraceInvokeContext<void> &context, jbooleanArray array, jsize start, jsize len,
                             const jboolean *buf) override;

  void SetByteArrayRegion(TraceInvokeContext<void> &context, jbyteArray array, jsize start, jsize len,
                          const jbyte *buf) override;

  void SetCharArrayRegion(TraceInvokeContext<void> &context, jcharArray array, jsize start, jsize len,
                          const jchar *buf) override;

  void SetShortArrayRegion(TraceInvokeContext<void> &context, jshortArray array, jsize start, jsize len,
                           const jshort *buf) override;

  void SetIntArrayRegion(TraceInvokeContext<void> &context, jintArray array, jsize start, jsize len,
                         const jint *buf) override;

  void SetLongArrayRegion(TraceInvokeContext<void> &context, jlongArray array, jsize start, jsize len,
                          const jlong *buf) override;

  void SetFloatArrayRegion(TraceInvokeContext<void> &context, jfloatArray array, jsize start, jsize len,
                           const jfloat *buf) override;

  void SetDoubleArrayRegion(TraceInvokeContext<void> &context, jdoubleArray array, jsize start, jsize len,
                            const jdouble *buf) override;

  void RegisterNatives(TraceInvokeContext<jint> &context, jclass clazz, const JNINativeMethod *methods,
                       jint nMethods) override;

  void UnregisterNatives(TraceInvokeContext<jint> &context, jclass clazz) override;

  void GetJavaVM(TraceInvokeContext<jint> &context, JavaVM **vm) override;

  void GetStringRegion(TraceInvokeContext<void> &context, jstring str, jsize start, jsize len, jchar *buf) override;

  void GetStringUTFRegion(TraceInvokeContext<void> &context, jstring str, jsize start, jsize len, char *buf) override;

  void NewWeakGlobalRef(TraceInvokeContext<jweak> &context, jobject obj) override;

  void DeleteWeakGlobalRef(TraceInvokeContext<void> &context, jweak obj) override;
};

} // namespace fakelinker