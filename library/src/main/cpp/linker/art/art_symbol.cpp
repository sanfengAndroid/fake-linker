#include <fakelinker/art_symbol.h>

#include "../linker_globals.h"

size_t art::DamagedRuntime::jvm_offset_ = -1;
namespace fakelinker {

ArtSymbol *ArtSymbol::Get() {
  static ArtSymbol art;
  return &art;
}

bool ArtSymbol::Init(JNIEnv *env) {
  ArtSymbol *art = Get();
  if (art->init_) {
    return true;
  }
  auto ext = art::JNIEnvExt::FromJNIEnv(env);
  bool init_runtime = art::DamagedRuntime::InitJavaVMOffset(ext->GetVm()->GetRuntime(), ext->GetVm());
  auto soinfo = ProxyLinker::Get().FindSoinfoByName("libart.so");
  if (!soinfo) {
    return false;
  }
  // art::ArtMethod::PrettyMethod(art::ArtMethod*, bool)
  auto pretty_method = soinfo->find_export_symbol_address("_ZN3art9ArtMethod12PrettyMethodEPS0_b");
  void *pretty_field = nullptr;
  if (pretty_method) {
    pretty_field = soinfo->find_export_symbol_address("_ZN3art8ArtField11PrettyFieldEPS0_b");
    art->DecodeMethodIdPtr = reinterpret_cast<decltype(ArtSymbol::DecodeMethodIdPtr)>(
      soinfo->find_export_symbol_address("_ZN3art3jni12JniIdManager14DecodeMethodIdEP10_jmethodID"));
    art->DecodeFieldIdPtr = reinterpret_cast<decltype(ArtSymbol::DecodeFieldIdPtr)>(
      soinfo->find_export_symbol_address("_ZN3art3jni12JniIdManager13DecodeFieldIdEP9_jfieldID"));
    if (init_runtime) {
      art->jni_id_manager = art::DamagedRuntime::FromRuntime(ext->GetVm()->GetRuntime())->GetJniIdManager();
    }
  } else {
    pretty_method = soinfo->find_export_symbol_address("_ZN3art12PrettyMethodEPNS_6mirror9ArtMethodEb");
    pretty_field = soinfo->find_export_symbol_address("_ZN3art11PrettyFieldEPNS_6mirror8ArtFieldEb");
  }
  art->NterpGetShortyPtr =
    reinterpret_cast<decltype(ArtSymbol::NterpGetShortyPtr)>(soinfo->find_export_symbol_address("NterpGetShorty"));

  if (!art->NterpGetShortyPtr) {
    art->ArtMethodGetShortyPtr = reinterpret_cast<decltype(ArtSymbol::ArtMethodGetShortyPtr)>(
      soinfo->find_export_symbol_address("_ZN3art6mirror9ArtMethod9GetShortyEPj"));
  }

  art->ArtMethodPrettyMethodPtr = reinterpret_cast<decltype(ArtSymbol::ArtMethodPrettyMethodPtr)>(pretty_method);
  art->ArtFieldPrettyFieldPtr = reinterpret_cast<decltype(ArtSymbol::ArtFieldPrettyFieldPtr)>(pretty_field);
  if (art->ArtFieldPrettyFieldPtr != nullptr &&
      (android_api < __ANDROID_API_R__ || (art->DecodeFieldIdPtr != nullptr && art->jni_id_manager != nullptr))) {
    art->can_pretty_field = true;
  }
  if (art->ArtMethodPrettyMethodPtr &&
      (android_api < __ANDROID_API_R__ || (art->DecodeMethodIdPtr != nullptr && art->jni_id_manager != nullptr))) {
    art->can_pretty_method = true;
  }
  LOGD("art::ArtMethod::PrettyMethod address: %p, art::ArtField::PrettyField address: %p, "
       "art::jni::JniIdManager::DecodeMethodId address: %p, art::jni::JniIdManager::DecodeFieldId address: %p, "
       "art::jni::JniIdManager address: %p",
       art->ArtMethodPrettyMethodPtr, art->ArtFieldPrettyFieldPtr, art->DecodeFieldIdPtr, art->DecodeMethodIdPtr,
       art->jni_id_manager);
  LOGD("NterpGetShorty address: %p, ArtMethodGetShorty address: %p", art->NterpGetShortyPtr,
       art->ArtMethodGetShortyPtr);
  art->init_ = true;
  return true;
}

std::string ArtSymbol::PrettyMethod(jmethodID method, bool with_signature) {
  if (!can_pretty_method) {
    return "";
  }
  return ArtMethodPrettyMethodPtr(DecodeMethodId(method), with_signature);
}
std::string ArtSymbol::PrettyField(jfieldID field, bool with_signature) {
  if (!can_pretty_field) {
    return "";
  }
  return ArtFieldPrettyFieldPtr(DecodeFieldId(field), with_signature);
}

void *ArtSymbol::DecodeMethodId(jmethodID method) {
  if (can_pretty_method) {
    return DecodeMethodIdPtr ? DecodeMethodIdPtr(jni_id_manager, method) : method;
  }
  return nullptr;
}

void *ArtSymbol::DecodeFieldId(jfieldID field) {
  if (can_pretty_field) {
    return DecodeFieldIdPtr ? DecodeFieldIdPtr(jni_id_manager, field) : field;
  }
  return nullptr;
}

std::string ArtSymbol::GetMethodShorty(/* ArtMethod* */ void *art_method) {
  const char *result = nullptr;
  if (art_method) {
    if (NterpGetShortyPtr) {
      result = NterpGetShortyPtr(art_method);
    } else if (ArtMethodGetShortyPtr) {
      uint32_t out;
      result = ArtMethodGetShortyPtr(art_method, &out);
    }
  }
  return result ? result : "";
}

std::string ArtSymbol::GetMethodShorty(jmethodID method) {
  void *art_method = DecodeMethodId(method);
  return GetMethodShorty(art_method);
}


} // namespace fakelinker