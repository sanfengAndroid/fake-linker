# fake-linker

![License](https://img.shields.io/badge/License-Apache2-blue)
[![Android CI](https://github.com/sanfengAndroid/fake-linker/actions/workflows/android.yml/badge.svg)](https://github.com/sanfengAndroid/fake-linker/actions/workflows/android.yml)
[![BrowserStack fake-linker](https://app-automate.browserstack.com/badge.svg?badge_key=UEY1VUFhYndHakhDLy91NEExQXE1eTI4Q1dwMXhxak1XN2RkNTVjQWZ2WT0tLUVoK3lpbVBvTDRIaVhxcTFBckR6WGc9PQ==--00d684412af19fbe1224ccc5e7ec58adacde9570)](https://app-automate.browserstack.com/public-build/UEY1VUFhYndHakhDLy91NEExQXE1eTI4Q1dwMXhxak1XN2RkNTVjQWZ2WT0tLUVoK3lpbVBvTDRIaVhxcTFBckR6WGc9PQ==--00d684412af19fbe1224ccc5e7ec58adacde9570?redirect=true)
[![BrowserStack fake-linker32](https://app-automate.browserstack.com/badge.svg?badge_key=Y0FEeG53d0JMYzdOVTFqbnd1OEtTZW5mbGxXdzE2YXN2TkQySllZeituaz0tLXFMLzBMMkxLdGZiZjNvMUxHK0FGV1E9PQ==--d0b7a7085348422206420ceb8ac6d3b50da0afe3)](https://app-automate.browserstack.com/public-build/Y0FEeG53d0JMYzdOVTFqbnd1OEtTZW5mbGxXdzE2YXN2TkQySllZeituaz0tLXFMLzBMMkxLdGZiZjNvMUxHK0FGV1E9PQ==--d0b7a7085348422206420ceb8ac6d3b50da0afe3?redirect=true)

[English](README.md) | [简体中文](README_CN.md)

`fake-linker` is a framework that provides features such as `Linker` modification, `PLT Hook`, and `Java Native Hook` for `android` applications. Its implementation principle involves dynamically searching for and modifying `Linker` data within the process. It offers `PLT hook` based on a `LD_PRELOAD-like` mode and various interfaces for operating on `soinfo` and `namespace`. For a detailed analysis of the principle, please refer to [Android Dynamic Modification of Linker to Implement LD_PRELOAD Global Library PLT Hook](https://sanfengandroid.github.io/2021/01/10/modify-linker-to-implement-plt-hook/).

Supports `Android 5.0 ~ Android 14+` devices with `x86`, `x86_64`, `arm`, and `arm64` instruction sets. It also supports HarmonyOS `2.x` and `3.x` versions, with versions beyond `3.x` untested.

## Project Description

We now offer the new v3.0.0+ version with the following major updates:

1. Removed the need for different `libfakelinker.so` files for different `Android Api Levels`. The internal function table now automatically adapts to different `Android` versions.
2. Introduced a stable C/C++ [FakeLinker](library/src/main/cpp/include/fake_linker.h) function table. Future versions guarantee backward compatibility and will not change the function pointer offsets of `FakeLinker`. The current version number can be obtained via `FakeLinker.get_fakelinker_version`.
3. Added support for the `fake-linker` static library, allowing customization of `fake-linker` and usage of interfaces such as [elf_reader.h](library/src/main/cpp/include/elf_reader.h), [jni_helper.h](library/src/main/cpp/include/elf_reader.h), [maps_util.h](library/src/main/cpp/include/maps_util.h) individually. This enables `fake-linker` to act as a hook module and reduces the number of `so` files.
4. Optional `Java FakeLinker API`. The new version no longer requires the `Java API` mandatorily; it can be deleted as needed. You can also customize the class name for dynamic registration of the `Java API`.

Below is the description for the sub-projects:

1. The `library` project is the core implementation of `fake-linker`, providing both static and dynamic libraries for `fakelinker`. **Note that when linking the `fakelinker` dynamic library, only the `fake_linker.h` header file can be used. Other header files require linking to the `fakelinker_static` static library for support.**
2. The `emulator-testapp` project is used to test the functionality of `fake-linker` in an emulator. It supports the `houdini` architecture, meaning that despite using dynamic translation to support the `arm` architecture, it can actually load `x86`/`x64` libraries. Therefore, it effectively uses the `x86`/`x64` architecture of `fake-linker` to provide functionality.
3. The `fakelinker-test` project is for normal `apk` testing on `x86`, `x86_64`, `arm`, `arm64` architectures. It includes `androidTest` and executable test programs. `androidTest` can be run via the `UI` in `Android Studio` by importing the project and running `FakelinkerGTest`, or by executing the command `./gradlew :fakelinker-test:connectedDebugAndroidTest` to connect to an `Android` device for testing. The executable test programs can be pushed to a device via `adb shell` and executed after adding executable permissions, using the path `fake-linker\fakelinker-test\src\main\cpp\build\Debug\xxxx\fakelinker_static_test`.
4. The `Stub` project only provides private `api` interfaces and is not packaged into the `apk`.

## Build and Integrate

1. Source Code Build

   > Refer to the [local.properties.sample](local.properties.sample) for configuration parameters, then rename it to `local.properties`. After that, you can import the project into `Android Studio` or compile it directly using the `gradle` command line. For reference, see the [workflow](.github/workflows/android.yml).

   **Note: The new version adds the private APIs `BaseDexClassLoader.getLdLibraryPath` and `BaseDexClassLoader.addNativePath`. Incremental builds might cause errors.**

   ```shell
   ...fake-linker\library\src\main\java\com\sanfengandroid\fakelinker\FakeLinker.java:153: error: cannot find symbol
      ((BaseDexClassLoader) loader).addNativePath(paths);
   ```

   **You can clean the project or use the `gradlew` command with the `--rerun-tasks` parameter to clear the build cache and then compile again. Once the `library` project code is compiled successfully, subsequent incremental builds should not be affected.**

2. Using the Built Library

   > Download the latest [Release version](https://github.com/sanfengAndroid/fake-linker/releases/latest) `aar` or the latest test version [Action artifact to extract aar](https://github.com/sanfengAndroid/fake-linker/actions/workflows/android.yml) file as a library and add it to your project dependencies, enabling the [prefab](https://developer.android.com/build/native-dependencies?agpversion=4.1) feature.

   ```groovy
   // build.gradle

   android {
       buildFeatures {
           prefab true
       }
   }

   dependencies {
       implementation fileTree(dir: 'libs', include: ['*.jar', '*.aar'])
   }
   ```

   Then use the following in `CMakeLists.txt`:

   ```cmake
   find_package(FakeLinker REQUIRED CONFIG)

   ...

   target_link_libraries(
       your_target
       PRIVATE
       # Dynamic library dependency
       FakeLinker::fakelinker
       # Static library dependency
       # FakeLinker::fakelinker_static
   )
   ```

3. CMake Integration

   > Use `FetchContent` to directly download and build from a remote source by adding the following code to your `CMakeLists.txt`:

   ```CMake
   include(FetchContent)
   FetchContent_Declare(
   fakelinker
   GIT_REPOSITORY https://github.com/sanfengAndroid/fake-linker.git
   SOURCE_SUBDIR library/src/main/cpp
   )

   FetchContent_MakeAvailable(fakelinker)
   ```

   Then, add the `fakelinker` module dependency in your `CMakeLists.txt`:

   ```CMake
   target_link_libraries(
   your_target
   PRIVATE
   # Dynamic library dependency
   fakelinker
   # Static library dependency
   # fakelinker_static
   )
   ```

   **Note:** This method only uses the `native` library of `fake-linker` and does not use the `Java` code [FakeLinker](library/src/main/java/com/sanfengandroid/fakelinker/FakeLinker.java) and [ErrorCode](library/src/main/java/com/sanfengandroid/fakelinker/ErrorCode.java). You can manually copy these files to your project or use only the `native` interfaces.

## Hook Module Development

Both the `aar` package import and `CMake` integration methods have already imported the `fakelinker` dependency. Therefore, you can directly link the `fakelinker` dynamic library in your `CMakeLists.txt`:

```CMake
target_link_libraries(
  your_target
  PRIVATE
  # Dynamic library dependency
  fakelinker
  # Static library dependency
  # fakelinker_static
)
```

Since the module already depends on `fakelinker`, there are two ways to load it. One method is to use the old version's Java layer to call the initialization method under `FakeLinker`. The second method is to directly use `System.loadLibrary(hookModule)`. It relies on `libfakelinker.so`, which will automatically load and initialize `fakelinker`.

### Method One: Old Version Java Invocation

#### Initialization of `fakelinker` and `Hook` Module

Essentially, this involves loading `libfakelinker.so` and the hook module. However, due to the different methods used, the loading process varies. When `libfakelinker.so` is initialized, it will automatically load the hook module and call back the `fakelinker_module_init` method. **Note: If the initialization of `fake-linker` fails, it will not load or call back the hook module.**

1. Project Self-Usage

   > You can directly call `FakeLinker.initFakeLinker` to load it from the `apk`.

2. Usage in `Xposed` Modules

   > The [LSPosed](https://github.com/LSPosed/LSPosed) framework has already handled the native library search paths internally. Therefore, you only need to configure the following settings in the `build.gradle` file to disable `so` compression (which is enabled by default when `minSdk >= 23`), and then call `FakeLinker.initFakeLinker` to load from the `Xposed` module `APK`.

   ```gradle
   android {
       packagingOptions {
           jniLibs {
               useLegacyPackaging true
           }
       }
   }
   ```

   > For `non-LSPosed` frameworks or lower versions, you need to set the native library search path and then load it. See method 3 for details.

3. Dynamically Setting the Native Library Search Path

   > Version `v3.1.0+` provides the interfaces [FakeLinker.addHookApkNativePath](src/main/java/com/sanfengandroid/fakelinker/FakeLinker.java) or [FakeLinker.addNativeLibraryPath](src/main/java/com/sanfengandroid/fakelinker/FakeLinker.java) to change the native library search path of the classloader. The default classloader is the one associated with the `FakeLinker` class. Since the loading call is from `FakeLinker`, you can keep the default. You can also call it to change the search path of other classloaders. Set the search paths for `fakelinker` and the hook module, then call `FakeLinker.initFakeLinker` to initialize.

4. Manually Installing Libraries to a Specified Location
   > Install the `fake-linker` and Hook modules to a path accessible by the application, such as `/data/local/tmp`, and then load it directly using the absolute path. **It is not recommended if you can set the native library search path and then load.**

#### Writing the Hook Module

1. Include the `fake_linker.h` header file and implement the `fakelinker_module_init` method. This method needs to be exported; otherwise, `fakelinker` cannot call it back.

   ```c++
   #include <fake_linker.h>

   C_API API_PUBLIC void fakelinker_module_init(JNIEnv *env, SoinfoPtr fake_soinfo, const FakeLinker *fake_linker) {
       // Set global so here, relocate already loaded so, etc.
   }
   ```

### Method Two: Native Invocation

Since the hook module depends on `libfakelinker.so`, you can directly call `System.loadLibrary("hook-module")` to automatically initialize `fakelinker`. This method does not require any `Java` code related to `FakeLinker` since all functionalities are merely wrappers around the internal `native FakeLinker` structure. This usage differs from Method One.

#### Initialization of Hook Module and `fakelinker`

Directly load the hook module:

```Java
try {
    System.loadLibrary("hook-module");
} catch (UnsatisfiedLinkError e) {
    // Handle the error
}
```

At this point, `fakelinker` is automatically initialized and the native methods under the `com/sanfengandroid/fakelinker/FakeLinker` class are registered by default. If they do not exist, they will not be registered.

This method no longer requires the `fakelinker_module_init` function. Instead, you can directly obtain the `const FakeLinker*` pointer through `get_fakelinker` and ensure that `fake_linker->is_init_success()` returns true before using it.

```c++
// hook_module.cpp

C_API API_PUBLIC jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    const FakeLinker *fake_linker = get_fakelinker();
    // Obtain the FakeLinker pointer and check if initialization is successful
    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        async_safe_fatal("JNI environment error");
    }
    // Initialize fake-linker
    init_fakelinker(env, static_cast<FakeLinkerMode>(FakeLinkerMode::kFMSoinfo | FakeLinkerMode::kFMNativeHook | FakeLinkerMode::kFMJavaRegister), nullptr);

    if (fake_linker->is_init_success()) {
        // Get own soinfo
        int error;
        SoinfoPtr thiz = fake_linker->soinfo_find(SoinfoFindType::kSTAddress, nullptr, &error);
        if (thiz) {
            // Similar to Method One, set global SO, relocate already loaded SOs, etc.
            ...
        }

        // Call init_fakelinker to check if native hook initialization is successful
        if (init_fakelinker(env, FakeLinkerMode::kFMNativeHook, nullptr) == 0) {
            LOGI("native hook init success");
        }
        // Register custom Java API
        if (init_fakelinker(env, FakeLinkerMode::kFMJavaRegister, "java/to/your/class") == 0) {
            LOGI("register custom java api success");
        }
    } else {
        // Error handling, cannot use fake_linker related methods
    }
    return JNI_VERSION_1_6;
}
```

## Using the `fakelinker` Static Library

Starting from version `v3.1.0`, the `fakelinker_static` static library is available, which allows you to customize `fakelinker` or use some of its provided native APIs. Static linking does not include `JNI_OnLoad`, so you need to initialize it manually. You can call `init_fakelinker` to initialize the required functionalities as needed. **Please ensure that the initialization is successful before using the corresponding functions.** The usage is similar to Method Two mentioned above. Common code is as follows:

```c++
#include <fake_linker.h>

C_API API_PUBLIC jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    const FakeLinker *fake_linker = get_fakelinker();
    // Obtain the FakeLinker pointer and check if initialization is successful
    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        async_safe_fatal("JNI environment error");
    }
    if (init_fakelinker(env, static_cast<FakeLinkerMode>(FakeLinkerMode::kFMSoinfo | FakeLinkerMode::kFMNativeHook | FakeLinkerMode::kFMJavaRegister), nullptr) == 0) {
        // Initialization successful
    }
}
```

Another common use of static library linking is for the `fake-linker` to act both as the `fake-linker` framework and the `hook` module. After initialization, it can be set as the global `so` to take effect.

## Additional Notes

1. Unlike directly setting the `LD_PRELOAD` environment variable, which typically cannot intercept the `dlsym` method since intercepting it would require implementing symbol lookup manually (with higher versions also imposing caller address restrictions), this module provides a `dlsym` call through the intermediary `fake-linker` module. Thus, the `hook` module can intercept `dlsym` and offer more `Linker-related` functionalities.

## Precautions

1. When using frameworks like `Xposed` to hook system processes, please ensure you have backup and removal measures in place to avoid system process crashes that might prevent the device from booting.
2. Depending on the module loading timing, relocate already loaded modules. Typically, when loading the `hook` module, some system libraries have already been loaded, such as `libjavacore.so`, `libnativehelper.so`, `libnativeloader.so`, `libart.so`, `libopenjdk.so`, etc. To make these libraries effective, you need to manually call methods like `FakeLinker.call_manual_relocation_by_names` in native code.
3. Set the `hook` module as a global library so that subsequently loaded `so` files will automatically take effect.
4. Before using any `fake-linker` related functionalities, ensure you call `init_fakelinker` to check if the corresponding functionality is successfully initialized. `init_fakelinker` can be called multiple times without causing reinitialization.

## Usage Examples

1. Relocate already loaded `so` files to make the `hook` module effective

   ```c++
   void fakelinker_module_init(JNIEnv *env, SoinfoPtr fake_soinfo, const FakeLinker *fake_linker) {
       const char* loaded_libs[] = {
           "libart.so",
           "libopenjdk.so",
           "libnativehelper.so",
           "libjavacore.so",
       };
       fake_linker->call_manual_relocation_by_names(fake_soinfo, 4, loaded_libs);
   }
   ```

2. Set the `hook` module as a global library

   ```c++
   C_API API_PUBLIC jint JNI_OnLoad(JavaVM *vm, void *reserved) {
       const FakeLinker *fake_linker = get_fakelinker();
       // Obtain the FakeLinker pointer and check if initialization is successful
       JNIEnv *env;
       if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
           async_safe_fatal("JNI environment error");
       }
       if (init_fakelinker(env, static_cast<FakeLinkerMode>(FakeLinkerMode::kFMSoinfo | FakeLinkerMode::kFMNativeHook | FakeLinkerMode::kFMJavaRegister), nullptr) == 0) {
           // Initialization successful
           if (SoinfoPtr thiz = fake_linker->soinfo_find(SoinfoFindType::kSTAddress, nullptr, nullptr)) {
               // Set the hook module as a global library; subsequent `so` loads will trigger the hook
               if (fake_linker->soinfo_add_to_global(thiz)) {
                   LOG("soinfo add to global success");
               }
           }
       }
       return JNI_VERSION_1_6;
   }

   // After the necessary hooks have taken effect, you can remove the global so setting.
   // This will not affect already loaded libraries.
   fake_linker->soinfo_remove_global(thiz);
   ```

3. For more usage examples, refer to the method pointers in the [FakeLinker](library/src/main/cpp/include/fake_linker.h) struct. We promise to maintain API compatibility starting from version `v3.1.0`.
