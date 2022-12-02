//
// Created by beich on 2020/11/15.
//
#ifndef FAKE_LINKER_FAKE_LINKER_H
#define FAKE_LINKER_FAKE_LINKER_H

#include <jni.h>
#include <link.h>
#include <stdint.h>

#include "linker_macros.h"
#include "linker_version.h"

/**
 * @brief FakeLinker
 * 1. You can implement the fakelinker_module_init method and export it,
 * then java call FakeLinker.initFakeLinker to initialize the path of the passed
 * module, which will be loaded and callback by FakeLinker.
 *
 * 2. Directly call System.load to load the FakeLinker module, because JNIEnv
 * needs to be loaded in the java layer, and direct dlopen cannot trigger the
 * JNI_OnLoad function and cannot initialize the environment. Then look for the
 * export symbol g_fakelinker_export to get the method table
 */

__BEGIN_DECLS

#define FAKELINKER_LIB_INIT_NAME "fakelinker_module_init"

enum FakeLinkerError {
  kErrorNo = 0,                          /**< No error */
  kHJErrorNO = kErrorNo,                 /**< No error */
  kErrorNpNull = 1,                      /**< android namespace empty errors */
  kErrorNpNotFound = 1 << 2,             /**< Not found the specified android namespace */
  kErrorSoinfoNull = 1 << 3,             /**< soinfo is nullptr error */
  kErrorSoinfoNotFound = 1 << 4,         /**< Not found the specified soinfo */
  kErrorSymbolNotFoundInSoinfo = 1 << 5, /**< Not found the specified symbol name */
  kErrorSoinfoRelink = 1 << 6,           /**< Relocating the specified soinfo import symbol again failed */
  kErrorFunctionNotImplemented = 1 << 7, /**< Feature not implemented */
  kErrorApiLevel = 1 << 8,               /**< The API level does not meet the requirements */
  kErrorParameter = 1 << 9,              /**< Parameter type error */
  kErrorParameterNull = 1 << 10,         /**< Parameter is empty error */
  kErrorMemoryError = 1 << 11,           /**< Internal memory request error */
  kErrorUnavailable = 1 << 12,           /**< The feature is not available, and it may be fixed in the future */
  kErrorExec = 1 << 13,                  /**< Internal execution error */
  kHJErrorOffset = 1 << 14,              /**< Method index error, out of range or illegal */
  kHJErrorMethodNull = 1 << 15,          /**< Method pointer is null error */
  kHJErrorRepeatOperation = 1 << 16,     /**< Repeated operation error */
  kHJErrorExec = 1 << 17,                /**< Inside execute error */
};

struct HookJniUnit {
  int offset;
  void *hook_method;
  void **backup_method;
};

struct HookRegisterNativeUnit {
  JNINativeMethod hook_method;
  bool is_static;
  void **backup_method;
};

#define FunPtr(Ret, Name, ...) Ret (*Name)(__VA_ARGS__)

typedef void *SoinfoPtr;    // soinfo raw pointer
typedef void *SoinfoHandle; // android 7.0+ soinfo handle
typedef void *SymbolAddress;
typedef void *AndroidDlextinfoPtr; // android_dlextinfo *
typedef void *AndroidNamespacePtr;
typedef void *AndroidLinkNamespacePtr;

struct SoinfoAttributes {
  SoinfoPtr soinfo_ptr;
  const char *so_name;
  const char *real_path;
  /**
   * @brief ELF library load base address
   *
   */
  uint64_t base;
  /**
   * @brief ELF library load size
   *
   */
  size_t size;
  uint32_t flags;
  ANDROID_GE_N SoinfoHandle handle;
};

enum SoinfoFindType {
  kSTAddress, /**< Find the soinfo data by the address, if it is null, take the
                 address of the caller */
  kSTHandle,  /**< Android 7.0+, find soinfo by handle */
  kSTName,    /**< Find soinfo by library name */
  kSTOrig,    /**< It is already soinfo, no query operation */
};

enum FindSymbolType {
  kImported, /**< find import symbol address*/
  kExported, /**< find export symbol address*/
  kInternal  /**< find internal symbol addresses, need to load library file from
                disk*/
};

struct FindSymbolUnit {
  const char *symbol_name;
  FindSymbolType symbol_type;
};

/**
 * @brief dlopen function signature inside FakeLinker
 *
 * @param filename      dlopen library name
 * @param flags         dlopen flags
 * @param caller_addr   Pass in the caller address, which is used to get the
 * namespace in Android 7.0+, ignore it below Android 7.0. When a null
 * pointer is specified, the current call address will be obtained as the value
 * @param extinfo       Java calls dlopen with additional information
 *
 */
typedef void *(*DlopenFun)(const char *filename, int flags, void *caller_addr, const AndroidDlextinfoPtr extinfo);

/**
 * @brief dlsym function signature inside FakeLinker
 *
 * @param handle        Same parameters as the original dlsym method, please
 * pass in soinfo handle when running on Android 7.0+.
 * @param symbol_name   Find symbol name
 * @param caller_addr   Pass in the caller address, which is used to get the
 * namespace in Android 7.0+, ignore it below Android 7.0. When a null
 * pointer is specified, the current call address will be obtained as the value
 *
 */
typedef void *(*DlsymFun)(void *handle, const char *symbol_name, void *caller_addr);

ANDROID_GE_N enum NamespaceFindType {
  kNPOriginal,      /**< It is already a namespace, do not query */
  kNPSoinfo,        /**< Specifiy the namespace in which soinfo looks for it */
  kNPNamespaceName, /**< Find the namespace with the specified name */
};

/**
 * @brief FakeLinker exports multiple function pointers, which are passed as
 * parameters when the module is loaded, or can be obtained by looking for the
 * module export symbol g_fakelinker_export
 */
typedef struct {
  /**
   * @brief Get FakeLinker version,since version 2000, the new version will be
   * compatible with the old version api
   *
   */
  FunPtr(int, get_fakelinker_version);

  /**
   * @brief The fake-linker initialization should be detected successfully
   * before calling other function. When you load a module in the second way,
   * you should first check
   *
   */
  FunPtr(bool, is_init_success);

  /**
   * @brief Specify the parameter to find the soinfo pointer for other function
   * calls as parameters
   *
   * @param       find_type  Specifies the type of parameter
   * @param       param      Specific parameters
   * @param[out]  out_error  Write error code on error exists
   */
  FunPtr(SoinfoPtr, soinfo_find, SoinfoFindType find_type, const void *param, int *out_error);

  /**
   * @brief Finds some properties of a specified SoinfoPtr
   *
   * @param       soinfo_ptr  Specify the soinfo pointer
   * @param[out]  attr_ptr    A pointer to save the property, it will be written
   * to after the call is successful
   *
   * @return Return the error code, and the call succeeds when
   * FakeLinkerError::kErrorNo is returned
   */
  FunPtr(int, soinfo_get_attribute, SoinfoPtr soinfo_ptr, SoinfoAttributes *attr_ptr);

  /**
   * Internal output to the Android logcat,
   */
  FunPtr(void, soinfo_to_string, SoinfoPtr);
  /**
   *  @brief Get all soinfo of the current process
   *
   * @param[out] out_soinfo_info_array  Holds pointers to all soinfo
   * @note that free_inside_memory is called to release memory after use to
   * avoid memory leaks
   * @param out_error Write error code on error exists
   */
  FunPtr(int, soinfo_query_all, MEMORY_FREE SoinfoPtr **out_soinfo_info_array, int *out_error);

  /**
   * @brief Get soinfo handle, requires Android 7.0+.
   * kErrorApiLevel is returned when the requirement is not met
   *
   * @note Requires Android 7.0+
   *
   * @param       soinfo_ptr  Specify the soinfo pointer
   * @param[out]  out_error   Write error code on error exists
   * @return soinfo handle or error
   */
  ANDROID_GE_N FunPtr(SoinfoHandle, soinfo_get_handle, SoinfoPtr soinfo_ptr, int *out_error);

  /**
   * @brief Get soinfo library name
   *
   * @param       soinfo_ptr  Specify the soinfo pointer
   * @param[out]  out_error   Write error code on error exists
   * @return Library name or nullptr
   */
  FunPtr(const char *, soinfo_get_name, SoinfoPtr soinfo_ptr, int *out_error);

  /**
   * @brief Detect that soinfo is a global library
   *
   * @param       soinfo_ptr  soinfo pointer
   * @param[out]  out_error   Write error code on error exists
   * @return is a global return true
   */
  FunPtr(bool, soinfo_is_global, SoinfoPtr soinfo_ptr, int *out_error);

  /**
   * @brief Get library real path
   *
   * @param       soinfo_ptr  Specify the soinfo pointer
   * @param[out]  out_error   Write error code on error exists
   * @return Real path or nullptr
   */
  FunPtr(const char *, soinfo_get_realpath, SoinfoPtr soinfo_ptr, int *out_error);

  /**
   * @brief Quickly get the linker soinfo pointer
   *
   */
  FunPtr(SoinfoPtr, soinfo_get_linker_soinfo);
  /**
   * @brief Specify the soinfo pointer to get the import symbol address of the
   * specified name
   *
   * @param       soinfo_ptr  Specify the soinfo pointer
   * @param       name        Import symbol name
   * @param[out]  out_error   Write error code on error exists
   * @return Symbolic address or nullptr
   */
  FunPtr(SymbolAddress *, soinfo_get_import_symbol_address, SoinfoPtr soinfo_ptr, const char *name, int *out_error);

  /**
   * @briefSpecify the soinfo pointer to get the export symbol address of the
   * specified name
   *
   * @param       soinfo_ptr  Specify the soinfo pointer
   * @param       name        Export symbol name
   * @param[out]  out_error   Write error code on error exists
   * @return Symbolic address or nullptr
   */
  FunPtr(SymbolAddress, soinfo_get_export_symbol_address, SoinfoPtr soinfo_ptr, const char *name, int *out_error);
  /**
   * @brief Get FakeLinker soinfo pointer, for modules to get other attributes
   * of FakeLinker
   *
   */
  FunPtr(SoinfoPtr, soinfo_get_fakelinker_soinfo);

  /**
   * @brief Get the dlopen function pointer inside FakeLinker, dlopen errors are
   * handled internally. If you want to get the original function address use
   * the dlsym function.
   *
   */
  FunPtr(DlopenFun, get_dlopen_inside_func_ptr);
  /**
   * @brief Get the dlsym function pointer inside FakeLinker, dlsym errors are
   * handled internally.
   *
   */
  FunPtr(DlsymFun, get_dlsym_inside_func_ptr);

  /**
   * @brief Call the dlopen function to automatically adapt the android version,
   * you can pass the caller address to change the command space where so is
   * located
   *
   * @param  filename    so file name
   * @param  flags       dlopen flags
   * @param  caller_addr Caller address android7.0+ use
   * @param  extinfo     Additional android information to mimic System.load
   * loading
   */
  FunPtr(void *, call_dlopen_inside, const char *filename, int flags, void *caller_addr,
         const AndroidDlextinfoPtr extinfo);

  /**
   * @brief Call the dlsym method to automatically adapt the version to remove
   * the namespace restriction.
   *
   * @param  handle      soinfo pointer or soinfo handle in Android 7.0+
   * @param  name        Find symbol name
   * @return Symbolic address or nullptr
   *
   */
  FunPtr(void *, call_dlsym_inside, void *handle, const char *name);

  /**
   * @brief Get linker export symbol address.
   *
   * @param  name        Export symbol name
   * @param  error       Write error code on error exists
   * @return Symbolc address or nullptr
   */
  FunPtr(SymbolAddress, get_linker_export_symbol, const char *name, int *out_error);

  /**
   * @brief Add soinfo to global library
   *
   * @param  soinfo_ptr  Adding the specified so as a global library will affect
   * subsequent library loading
   * @return Return true if successful
   */
  FunPtr(bool, soinfo_add_to_global, SoinfoPtr soinfo_ptr);

  /**
   * @brief Remove soinfo from global library
   *
   * @param  soinfo_ptr  soinfo pointer
   * @return Successful removal returns true
   */
  FunPtr(bool, soinfo_remove_global, SoinfoPtr soinfo_ptr);

  /**
   * @brief Add relocation symbol blacklist. Affects manual calls to relocations
   * after, note that system relocation is not affected
   *
   * @param  symbol_name Blacklist symbol name
   */
  FunPtr(void, add_relocation_blacklist, const char *symbol_name);
  /**
   * @brief Remove relocation symbol blacklist.  Affects manual calls to
   * relocations after, note that system relocation is not affected
   *
   * @param  symbol_name Remove symbol name
   */
  FunPtr(void, remove_relocation_blacklist, const char *symbol_name);

  /**
   * @brief Remove all relocation symbol blacklist.
   *
   */
  FunPtr(void, clear_relocation_blacklist);
  /**
   * @brief Manually relocate the target library again and enter the custom
   * global_lib library. Blacklisted symbols set will be excluded in global_lib
   * exported symbols
   *
   * @param  global_lib  Specify a library as a global library, it can be a
   * global library or a non-global library.
   * @param  target      Relocated target library
   * @return Return true if the relocation is successful
   */
  FunPtr(bool, call_manual_relocation_by_soinfo, SoinfoPtr global_lib, SoinfoPtr target);

  /**
   * @brief Manually relocate multiple target libraries again to exclude
   * blacklist symbols.
   *
   * @param  global_lib  Specify a library as a global library, it can be a
   * global library or a non-global library.
   * @param  len         target number
   * @param  targets     target SoinfoPtr array
   * @return Return true if all libraries are relocated successfully
   */
  FunPtr(bool, call_manual_relocation_by_soinfos, SoinfoPtr global_lib, int len, SoinfoPtr targets[]);

  /**
   * @brief Manually relocate the target library again. Blacklisted symbols set
   * will be excluded in global_lib exported symbols
   *
   * @param  global_lib  pecify a library as a global library, it can be a
   * global library or a non-global library.
   * @param  target_name Specify the library name, find soinfo internally
   * @return Return true if the relocation is successful
   */
  FunPtr(bool, call_manual_relocation_by_name, SoinfoPtr global_lib, const char *target_name);
  /**
   * @brief Manually relocate multiple target libraries again to exclude
   * blacklist symbols.
   *
   * @param  global_lib   Specify a library as a global library, it can be a
   * global library or a non-global library.
   * @param  len          target number
   * @param  target_names target library name array
   * @return Return true if all libraries are relocated successfully
   */
  FunPtr(bool, call_manual_relocation_by_names, SoinfoPtr global_lib, int len, const char *target_names[]);

  /**
   * @brief New version expansion reserved slot
   *
   */
  FunPtr(void, unused0);
  FunPtr(void, unused1);
  FunPtr(void, unused2);
  FunPtr(void, unused3);
  FunPtr(void, unused4);
  FunPtr(void, unused5);
  FunPtr(void, unused6);
  FunPtr(void, unused7);
  FunPtr(void, unused8);
  FunPtr(void, unused9);

  /**
   * @brief Find the android namespace
   *
   * @note Requires Android 7.0+
   *
   * @param       find_type   Specifies the type of parameter
   * @param       param       Specify parameters
   * @param[out]  out_error   Write error code on error exists
   * @return android namespace pointer or nullptr
   */
  ANDROID_GE_N FunPtr(AndroidNamespacePtr, android_namespace_find, NamespaceFindType find_type, const void *param,
                      int *out_error);

  /**
   * @brief Get android namespace name
   *
   * @note Requires Android 7.0+
   *
   * @param       android_namespace_ptr android namespace pointer
   * @param[out]  out_error             Write error code on error exists
   * @return nullptr or name
   */
  ANDROID_GE_N FunPtr(const char *, android_namespace_get_name, AndroidNamespacePtr android_namespace_ptr,
                      int *out_error);

  /**
   * @brief Find all existing android namespace, be careful to release memory
   * after use.
   *
   * @note Requires Android 7.0+
   *
   * @param[out]  android_namespace_ptr_array  Holds pointers to all
   * android_namespace_t.
   * @note that free_inside_memory is called to release memory after use to
   * avoid memory leaks
   * @param[out]  out_error   Write error code on error exists
   * @return Return the count of all namespaces
   */
  ANDROID_GE_N
  FunPtr(int, android_namespace_query_all, MEMORY_FREE AndroidNamespacePtr **out_android_namespace_ptr_array,
         int *out_error);

  FunPtr(AndroidNamespacePtr, android_namespace_create, const char *name, const char *ld_library_path,
         const char *default_library_path, uint64_t type, const char *permitted_when_isolated_path,
         AndroidNamespacePtr parent_namespace, const void *caller_addr);

  /**
   * @brief Get all soinfo in the namespace
   *
   * @note Requires Android 7.0+
   *
   * @param       android_namespace_ptr  Specify android namespace
   * @param[out]  soinfo_ptr_array       Holds pointers to all soinfo. @note
   * that free_inside_memory is called to release memory after use to avoid
   * memory leaks
   * @param[out]  out_error            Write error code on error
   * exists
   * @return Return the count of all soinfo
   */
  ANDROID_GE_N FunPtr(int, android_namespace_get_all_soinfo, AndroidNamespacePtr android_namespace_ptr,
                      MEMORY_FREE SoinfoPtr **out_soinfo_ptr_array, int *out_error);

  /**
   * @brief Get the address contained in the namespace, often used in the dlopen function to pass the caller address.
   * @note that the namespace must contain at least one soinfo
   *
   * @param  android_namespace_ptrDoc
   * @param  out_error   Doc
   * @return ANDROID_GE_N
   */
  ANDROID_GE_N FunPtr(void *, android_namespace_get_caller_address, AndroidNamespacePtr android_namespace_ptr,
                      int *out_error);

  /**
   * @brief Get all linked namespaces in the namespace
   *
   * @note Requires Android 8.0+
   *
   * @param       android_namespace_ptr  Specify android namespace
   * @param       link_np_array          Holds pointers to all
   * android_namespace_link_t
   * @param[out]  out_error              Write error code on error
   * exists
   * @return Return the count of all link namespace
   */
  ANDROID_GE_O FunPtr(int, android_namespace_get_link_namespace, AndroidNamespacePtr android_namespace_ptr,
                      ONLY_READ AndroidLinkNamespacePtr *link_np_array, int *out_error);

  /**
   * @brief Get all global soinfo in the namespace
   *
   * @note Requires Android 7.0+
   *
   * @param       android_namespace_ptr  Specify android namespace
   * @param[out]  soinfo_ptr_array       Holds pointers to all soinfo.
   * @note that free_inside_memory is called to release memory after use to
   * avoid memory leaks
   * @param[out]  out_error              Write error code on error
   * exists
   * @return Return the count of all soinfo
   */
  ANDROID_GE_N FunPtr(int, android_namespace_get_global_soinfos, AndroidNamespacePtr android_namespace_ptr,
                      MEMORY_FREE SoinfoPtr **out_soinfo_ptr_array, int *out_error);

  /**
   * @brief Add only one global library in the specified namespace
   *
   * @note Requires Android 7.0+
   *
   * @param  android_namespace_ptr  Specify android namespace
   * @param  global_soinfo_ptr      Library to be added
   * @return Return error code or kErrorNo
   */
  ANDROID_GE_N FunPtr(int, android_namespace_add_global_soinfo, AndroidNamespacePtr android_namespace_ptr,
                      SoinfoPtr global_soinfo_ptr);

  /**
   * @brief Add soinfo to the specified namespace
   *
   * @note Requires Android 7.0+
   *
   * @param  android_namespace_ptr  Specify android namespace
   * @param  soinfo_ptr             Library to be added
   * @return Return error code or kErrorNo
   */
  ANDROID_GE_N FunPtr(int, android_namespace_add_soinfo, AndroidNamespacePtr android_namespace_ptr,
                      SoinfoPtr soinfo_ptr);
  /**
   * @brief Remove the specified soinfo from the android namespace
   *
   * @note Requires Android 7.0+
   *
   * @param  android_namespace_ptr  android namespace pointer
   * @param  soinfo_ptr  specified soinfo
   * @param  clear_global_flags If it is a global soinfo clear flag, it will not
   * take effect in other android namespaces.
   * @return Return error code or kErrorNo
   */
  ANDROID_GE_N FunPtr(int, android_namespace_remove_soinfo, AndroidNamespacePtr android_namespace_ptr,
                      SoinfoPtr soinfo_ptr, bool clear_global_flags);

  /**
   * @brief Get android namespace whitelist libraries.
   *
   * @note Requires Android 10.0+
   * @param       android_namespace_ptr android namespace pointer
   * @param[out]  white_list  save the whitelist pointer
   * @param[out]  out_error   Write error code on error
   * @return whilelist lenth or 0
   */
  ANDROID_GE_Q FunPtr(int, android_namespace_get_white_list, AndroidNamespacePtr android_namespace_ptr,
                      MEMORY_FREE const char **out_white_list, int *out_error);

  /**
   * @brief Add library whitelist to namespace
   *
   * @note  Requires Android 10.0+
   *
   * @param  android_namespace_ptr  Specify android namespace
   * @param  libname                Library name to be added
   * @return Return error code or kErrorNo
   */
  ANDROID_GE_Q FunPtr(int, android_namespace_add_soinfo_whitelist, AndroidNamespacePtr android_namespace_ptr,
                      const char *libname);

  /**
   * @brief Remove whitelisted library in namespace
   *
   * @note Requires Android 10.0+
   *
   * @param  android_namespace_ptr android namespace pointer
   * @param  libname     library name
   * @return Return error code or kErrorNo
   */
  ANDROID_GE_Q FunPtr(int, android_namespace_remove_whitelist, AndroidNamespacePtr android_namespace_ptr,
                      const char *libname);

  /**
   * @brief Add load path to namespace.
   * When loading the library, first look in this directory
   *
   * @note Requires Android 7.0+
   *
   * @param  android_namespace_ptr Specify android namespace
   * @param  path                  Preferential loading path
   * @return Return error code or kErrorNo
   */
  ANDROID_GE_N FunPtr(int, android_namespace_add_ld_library_path, AndroidNamespacePtr android_namespace_ptr,
                      const char *path);

  /**
   * @brief Add default load path to namespace.
   * Second lookup directory when loading libraries
   *
   * @note Requires Android 7.0+
   *
   * @param  android_namespace_ptr Specify android namespace
   * @param  path                  Default load path
   * @return Return error code or kErrorNo
   */
  ANDROID_GE_N FunPtr(int, android_namespace_add_default_library_path, AndroidNamespacePtr android_namespace_ptr,
                      const char *path);

  /**
   * @brief Add permitted path to namespace.
   * Third lookup directory when loading libraries
   *
   * @note Requires Android 7.0+
   *
   * @param  android_namespace_ptr Specify android namespace
   * @param  path                  Permitted load path
   * @return Return error code or kErrorNo
   */
  ANDROID_GE_N FunPtr(int, android_namespace_add_permitted_library_path, AndroidNamespacePtr android_namespace_ptr,
                      const char *path);

  /**
   * @brief Adds a linked namespace to the namespace. Internally, a linked
   * namespace object is constructed
   *
   * @note Requires Android 8.0+
   *
   * @param       android_namespace_ptr  Specify android namespace
   * @param       add_namespace_ptr      The android namespace where the link
   * namespace is located
   * @param       allow_all_shared_libs  Android 9.0+ controls whether
   * all libraries contained in the namespace can be accessed, otherwise it is
   * restricted by the allowed library name, and it is ignored below 9.0
   * @param[out]  len                    Input the number of shared_libs
   * @param       shared_libs            Specifies the names of all shared
   * libraries that can be accessed
   * @return Return error code or kErrorNo
   */
  ANDROID_GE_O FunPtr(int, android_namespace_add_linked_namespace, AndroidNamespacePtr android_namespace_ptr,
                      AndroidNamespacePtr add_namespace_ptr, ANDROID_GE_P bool allow_all_shared_libs, int len,
                      const char *shared_libs[]);

  /**
   * @brief add second namespace to soinfo
   *
   * @note Requires Android 7.0+
   *
   * @param  soinfo_ptr             Specify soinfo
   * @param  android_namespace_ptr  Specify android namespace
   * @return Return error code or kErrorNo
   */
  ANDROID_GE_N FunPtr(int, soinfo_add_second_namespace, SoinfoPtr soinfo_ptr,
                      AndroidNamespacePtr android_namespace_ptr);

  /**
   * @brief Remove second namespace to soinfo
   *
   * @note Requires Android 7.0+
   *
   * @param  soinfo_ptr  soinfo poiner
   * @param  android_namespace_ptr android namespace pointer
   * @return Return error code or kErrorNo
   */
  ANDROID_GE_N FunPtr(int, soinfo_remove_second_namespace, SoinfoPtr soinfo_ptr,
                      AndroidNamespacePtr android_namespace_ptr);

  /**
   * @brief Change the namespace of soinfo
   *
   * @note Requires Android 7.0+
   *
   * @param  soinfo_ptr             Specify soinfo
   * @param  android_namespace_ptr  Changed namespace
   * @return Return error code or kErrorNo
   */
  ANDROID_GE_N FunPtr(int, soinfo_change_namespace, SoinfoPtr soinfo_ptr, AndroidNamespacePtr android_namespace_ptr);

  /**
   * @brief New version expansion reserved slot
   *
   */
  FunPtr(void, unused10);
  FunPtr(void, unused11);
  FunPtr(void, unused12);
  FunPtr(void, unused13);
  FunPtr(void, unused14);
  FunPtr(void, unused15);
  FunPtr(void, unused16);
  FunPtr(void, unused17);
  FunPtr(void, unused18);
  FunPtr(void, unused19);

  /**
   * @brief Release the pointer marked by MEMORY_FREE, only release the pointer
   * created by FakeLinker, please do not call it when other pointers are
   * released
   *
   * @param  ptr         Memory pointer
   */
  FunPtr(void, free_inside_memory, const void *ptr);

  /**
   * @brief Replace function pointers in JNINativeInterface, equivalent to JNI
   * Hook
   *
   * @param       func_offset      The offset of the method in the
   * JNINativeInterface struct,please use offsetof(JNINativeInterface,
   * FunctionName) to get the offset
   * @param       hook_method      Replaced method pointer
   * @param[out]  backup_method    Backup the original method pointer for later
   * calling. If it is empty, it will not be backed up
   */
  FunPtr(FakeLinkerError, hook_jni_native_function, int func_offset, void *hook_method, void **backup_method);

  /**
   * @brief Hook multiple Jni methods, @see hook_jni_native_function
   *
   * @param  items       Hook unit array
   * @param  len         The number of methods to hook
   * @return Number of successful hooks
   */
  FunPtr(int, hook_jni_native_functions, HookJniUnit items[], int len);

  /**
   * @brief Hook general java native method, replace the previous address by
   * registering the function address, and read the original address backup
   *
   * @note The original address obtained when RegisterNative registration is not
   * called is jni_dlsym_lookup_critical_trampoline_address
   *
   * @param  env         JNIEnv pointer
   * @param  clazz       The class in which the method is registered
   * @param  items       Register the required parameters
   * @param  len         Number of registration methods
   */
  FunPtr(int, hook_java_native_functions, JNIEnv *env, jclass clazz, HookRegisterNativeUnit *items, size_t len);

  /**
   * @brief New version expansion reserved slot
   *
   */
  FunPtr(void, unused20);
  FunPtr(void, unused21);
  FunPtr(void, unused22);
  FunPtr(void, unused23);
  FunPtr(void, unused24);
  FunPtr(void, unused25);
  FunPtr(void, unused26);
  FunPtr(void, unused27);
  FunPtr(void, unused28);
  FunPtr(void, unused29);

  /**
   * @brief Call the android_log_print function of the android system directly
   * to avoid circular dependencies in modules
   * @param prio
   * @param tag
   * @param fmt
   */
  FunPtr(int, android_log_print, int prio, const char *tag, const char *fmt, ...);

  /**
   * @brief Resolve symbol addresses of libraries from maps file
   *
   * @param  library_name find library name
   * @param  symbol_name  find symbol name
   * @param  symbol_type  type of symbol to find
   * @return symbol address or 0
   */
  FunPtr(uint64_t, find_library_symbol, const char *library_name, const char *symbol_name,
         const FindSymbolType symbol_type);

  /**
   * @brief Resolving multi-symbol addresses of libraries from maps files
   *
   * @param  library_name find library name
   * @param  symbols      pointer to an array of symbols to resolve
   * @param  size         Number of symbols resolved
   * @return[out] Returns the resolved address of each symbol or 0, the memory
   * needs to be freed
   */
  FunPtr(MEMORY_FREE uint64_t *, find_library_symbols, const char *library_name, const FindSymbolUnit *symbols,
         int size);

  /**
   * @brief Set linker internal log level
   *
   * LINKER_VERBOSITY_PRINT (-1)
   * LINKER_VERBOSITY_INFO  0
   * LINKER_VERBOSITY_TRACE 1
   * LINKER_VERBOSITY_DEBUG 2
   *
   * @param  level   log level, the larger the value, the more logs
   */
  FunPtr(bool, set_ld_debug_verbosity, int level);

} FakeLinker;

#undef FunPtr

/**
 * @brief The initialization function of the Hook module, which is called by
 * FakeLinker
 *
 * @param  env            JNI Environment
 * @param  fake_soinfo    Hook module self soinfo pointer
 * @param  fake_linker    Fakelinker's method table for module call
 */
extern void fakelinker_module_init(JNIEnv *env, SoinfoPtr fake_soinfo, const FakeLinker *fake_linker);

__END_DECLS

#endif // FAKE_LINKER_FAKE_LINKER_H