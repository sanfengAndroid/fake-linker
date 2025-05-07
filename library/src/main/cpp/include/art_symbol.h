#pragma once

#include <jni.h>

#include <string>

#include <linker_macros.h>
#include <macros.h>

namespace art {

inline namespace compatible {
class JavaVMExt;

class DamagedRuntime {
public:
  DamagedRuntime() = delete;

  static DamagedRuntime *FromRuntime(void *runtime) {
    CHECK(jvm_offset_ != -1);
    return reinterpret_cast<DamagedRuntime *>(reinterpret_cast<char *>(runtime) + jvm_offset_ - sizeof(void *));
  }

  static bool InitJavaVMOffset(void *runtime, JavaVMExt *vm) {
    if (!runtime || !vm) {
      return false;
    }
    auto *ptr = reinterpret_cast<size_t *>(runtime);
    auto target = reinterpret_cast<size_t>(vm);
    for (std::size_t i = 0; i < 2000; i++) {
      if (ptr[i] == target) {
        jvm_offset_ = i * sizeof(size_t);
        LOGD("init art::Runtime::java_vm_ offset: %zu", jvm_offset_);
        return true;
      }
    }
    return false;
  }

  JavaVMExt *GetJavaVM() const { return java_vm_.get(); }

  ANDROID_GE_R /* art::Runtime* */ void *GetJniIdManager() const { return jni_id_manager_.get(); }

private:
  ANDROID_GE_R std::unique_ptr<void *> jni_id_manager_;
  std::unique_ptr<JavaVMExt> java_vm_;

  // 在 Runtime 类中的偏移,使用前应设置
  static size_t jvm_offset_;
};

class JavaVMExt : public JavaVM {
public:
  JavaVMExt() = delete;
  /* art::Runtime* */ void *GetRuntime() const { return runtime_; }

private:
  /* art::Runtime* */ void *const runtime_;
};

class Thread {
public:
  Thread() = delete;

private:
  void *placeholder_;
};

class JNIEnvExt : public JNIEnv {
public:
  JNIEnvExt() = delete;
  JavaVMExt *GetVm() const { return vm_; }

  static JNIEnvExt *FromJNIEnv(JNIEnv *env) { return reinterpret_cast<JNIEnvExt *>(env); }

private:
  Thread *const self_;
  JavaVMExt *const vm_;
};

} // namespace compatible
} // namespace art

namespace fakelinker {

class ArtSymbol {
public:
  static ArtSymbol *Get();
  /**
   * @brief Requests basic soinfo to be initialized
   */
  static bool Init(JNIEnv *env);

  std::string PrettyMethod(jmethodID method, bool with_signature);
  std::string PrettyField(jfieldID field, bool with_signature);
  /* ArtMethod* */ void *DecodeMethodId(jmethodID method);
  /* ArtField* */ void *DecodeFieldId(jfieldID field);

  std::string GetMethodShorty(/* ArtMethod* */ void *art_method);

  std::string GetMethodShorty(jmethodID method);

  /**
   * @brief android 11+ Memory layout in the art::Runtime class
   *          std::unique_ptr<jni::JniIdManager> jni_id_manager_;
   *          std::unique_ptr<JavaVMExt> java_vm_;
   */
  bool can_pretty_field = false;
  bool can_pretty_method = false;

private:
  ArtSymbol() = default;
  bool init_ = false;
  ANDROID_GE_R /* art::jni::JniIdManager */ void *jni_id_manager = nullptr;
  std::string (*ArtMethodPrettyMethodPtr)(/* ArtMethod* */ void *art_method, bool with_signature) = nullptr;
  std::string (*ArtFieldPrettyFieldPtr)(/* ArtField* */ void *art_field, bool with_signature) = nullptr;
  ANDROID_GE_R /* ArtMethod* */ void *(*DecodeMethodIdPtr)(/* JniIdManager */ void *thiz, jmethodID method) = nullptr;
  ANDROID_GE_R /* ArtField* */ void *(*DecodeFieldIdPtr)(/* JniIdManager */ void *thiz, jfieldID field) = nullptr;

  const char *(*NterpGetShortyPtr)(/* ArtMethod* */ void *art_method) = nullptr;
  const char *(*ArtMethodGetShortyPtr)(/* ArtMethod* */ void *art_method, uint32_t *out_length) = nullptr;
};

} // namespace fakelinker