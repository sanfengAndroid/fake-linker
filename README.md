# fake-linker
![License](https://img.shields.io/badge/License-Apache2-blue)

Chinese document click [here](README_CN.md)

## Project description

Modify Android linker to provide loading module and plt hook features.Please check the detailed principle [modify linker to implement plt hook](https://sanfengandroid.github.io/2021/01/10/modify-linker-to-implement-plt-hook/)

## Supported Android

Android version: `Android 5.0` ~ `Android 11`+. Support instructions: `x86`, `x86_64`, `arm`, `arm64`

## Build

1. Source build
> Add it as an `Android Library` to the `Android` project,the main module adds its dependencies.Change [`build.gradle`](build.gradle) `buildApi` variable,compile the specified Api level.
2. Use build library
> Download [the latest version](https://github.com/sanfengAndroid/fake-linker/releases/latest) of the binary file, decompress it, add the `aar` file as a library to the project dependencies, and import the header file under the `include` directory into the Hook module for use.
3. Build configuration
> Refer to [FakeXposed](https://github.com/sanfengAndroid/FakeXposed) configuration scripts `build.py` and `build.gradle`
## Hook module development
1. Copy the export header file (the source code is in the [export](src/main/cpp/export) directory under the `cpp` directory) to the `Hook` module.
2. Implement the `fake_load_library_init` export function in [`linker_export.h`](src/main/cpp/export/linker_export.h).
3. Call various implementation methods, check the definition of `FakeLinker` in [`linker_export.h`](src/main/cpp/export/linker_export.h).
4. Normally implement Hook methods such as: `open`, `dlopen`, `dlsym` method, etc., the method must be exported.
5. Hook module distinguishes Android7.0 or lower (no namespace, `soinfo handle`), Android7.0 and above (namespace, `soinfo handle`).

## Java initialization
1. Install the library correctly to the specified location, install the `fake-linker` and `Hook` modules to the path that the application has access to, such as: `/data/local/tmp`, you can call the system method to load directly.
The module has integrated the installation executable file, and the Java layer calls the method under the [FileInstaller](src/main/java/com/sanfengandroid/fakelinker/FileInstaller.java) class to install, and various different platform architectures have been processed inside.
2. Set the `fake-linker` module through `FileInstaller`. The `Hook` module requires different [selinux](src/main/java/com/sanfengandroid/fakelinker/FileInstaller.java/#L232) and [uid, gid]( src/main/java/com/sanfengandroid/fakelinker/FileInstaller.java/#L223) file attributes.
3. Load and initialize the `fake-linker` module, call the [`FakeLinker.initFakeLinker`](src/main/java/com/sanfengandroid/fakelinker/FakeLinker.java) method, internally load the Hook module and call back the `fake_load_library_init` method to complete the module initialization.

## Other description

1. The project is different from directly setting the `LD_PRELOAD` environment variable. Direct setting usually cannot intercept the `dlsym` method, because once intercepted, you need to implement the search symbol yourself and the higher version has the `caller` address restriction, and the module passes the transfer module `fake- linker` provides calling `dlsym` method, so Hook module can intercept `dlsym` and provide more `Linker` related functions.
2. Each version of Android `Linker` has corresponding modifications, so the module depends on the `api` level of the phone, and the corresponding modules can be loaded at different levels. When loading manually, you need to pay attention to `Api 25` directly use the library of `Api 24`

## Note

1. When hooking the system process, please do a good job of deleting the backup to avoid the situation that the system process is dead and cannot be booted due to an error.
2. Relocate the loaded module according to the module loading time.

## Usage example

1. Set the `Hook` module as the global library `remote->CallCommonFunction(kCFAddSoinfoToGlobal, kSPAddress, nullptr, kSPNull, nullptr, &error_code);`.
2. Relocate some loaded modules `remote->CallCommonFunction(kCFCallManualRelinks, kSPAddress, nullptr, kSPNames, libs, &error_code);`.
    ```c++
    static const FakeLinker *remote;
    // Hook the jni function RegisterNatives
    static jint HookJniRegisterNatives(JNIEnv *env, jclass c, const JNINativeMethod *methods, jint nMethods) {
        LOG("start register native function %p", __builtin_return_address(0));
        jint ret = original_functions->RegisterNatives(env, c, methods, nMethods);
        if (ret != JNI_ERR && !original_functions->ExceptionCheck(env)) {
            std::string cls = JNIHelper::GetClassName(env, c);
            for (int i = 0; i < nMethods; ++i) {
                LOG("native register class: %s, method name: %s, function signature: %s, register address: %p", cls.c_str(), methods[i].name, methods[i].signature, methods[i].fnPtr);
            }
        }
        return ret;
    }

    static void InitHook() {
        int error_code;
        // Add this hook module to the global module, which will affect all modules loaded later
        remote->CallCommonFunction(kCFAddSoinfoToGlobal, kSPAddress, nullptr, kSPNull, nullptr, &error_code);
         if (error_code != kErrorNo) {
            LOGE("init global soinfo error, error code: %x", error_code);
            return;
        }
        VarArray<const char *> *libs;
        // Re-parse the import table of the following modules, because the following modules have been loaded before we have loaded them, and all re-links make their symbolic links to our Hook method
        // The java system code also mainly uses the following libraries, and relinking also means the core import function of Hook's java
        libs = VaargToVarArray<const char *>(5, "libjavacore.so", "libnativehelper.so", "libnativeloader.so", "libart.so", "libopenjdk.so");
        remote->CallCommonFunction(kCFCallManualRelinks, kSPAddress, nullptr, kSPNames, libs, &error_code);
        // Hook JNI interface
        remote_->HookJniNative(offsetof(JNINativeInterface, RegisterNatives), (void *)HookJniRegisterNatives, nullptr);
    }
    C_API void fake_load_library_init(JNIEnv *env, void *fake_soinfo, const FakeLinker *interface,
    const char *cache_path, const char *config_path, const char *_process_name){
        remote = interface;
        InitHook();
    }
    ```
3. For other more uses
[FakeXposed](https://github.com/sanfengAndroid/FakeXposed) `Xposed`，`root` Shield detection，File redirection, etc.