# fake-linker

![License](https://img.shields.io/badge/License-Apache2-blue)
[![Android CI](https://github.com/sanfengAndroid/fake-linker/actions/workflows/android.yml/badge.svg)](https://github.com/sanfengAndroid/fake-linker/actions/workflows/android.yml)
[![BrowserStack fake-linker](https://app-automate.browserstack.com/badge.svg?badge_key=UEY1VUFhYndHakhDLy91NEExQXE1eTI4Q1dwMXhxak1XN2RkNTVjQWZ2WT0tLUVoK3lpbVBvTDRIaVhxcTFBckR6WGc9PQ==--00d684412af19fbe1224ccc5e7ec58adacde9570)](https://app-automate.browserstack.com/public-build/UEY1VUFhYndHakhDLy91NEExQXE1eTI4Q1dwMXhxak1XN2RkNTVjQWZ2WT0tLUVoK3lpbVBvTDRIaVhxcTFBckR6WGc9PQ==--00d684412af19fbe1224ccc5e7ec58adacde9570?redirect=true)
[![BrowserStack fake-linker32](https://app-automate.browserstack.com/badge.svg?badge_key=Y0FEeG53d0JMYzdOVTFqbnd1OEtTZW5mbGxXdzE2YXN2TkQySllZeituaz0tLXFMLzBMMkxLdGZiZjNvMUxHK0FGV1E9PQ==--d0b7a7085348422206420ceb8ac6d3b50da0afe3)](https://app-automate.browserstack.com/public-build/Y0FEeG53d0JMYzdOVTFqbnd1OEtTZW5mbGxXdzE2YXN2TkQySllZeituaz0tLXFMLzBMMkxLdGZiZjNvMUxHK0FGV1E9PQ==--d0b7a7085348422206420ceb8ac6d3b50da0afe3?redirect=true)

[English](README.md) | [简体中文](README_CN.md)


`fake-linker` 是一款为 `android` 应用程序提供`Linker 修改`, `PLT Hook`, `Java Native Hook` 等功能的框架, 其实现原理是进程内动态查找和修改 `Linker` 数据, 对外提供基于 `类LD_PRELOAD` 模式的 `PLT hook` 和各种操作 `soinfo`, `namespace`的接口. 原理分析请查看[Android 动态修改 Linker 实现 LD_PRELOAD 全局库 PLT Hook](https://sanfengandroid.github.io/2021/01/10/modify-linker-to-implement-plt-hook/)

支持 `Android 5.0 ~ Android 14+` 设备的 `x86`, `x86_64`, `arm`, `arm64` 指令集, 支持鸿蒙系统 `2.x` 和 `3.x`版本, `3.x` 之后系统未测试.

## 项目说明

现在提供全新的 `v3.0.0+` 版本, 主要更新如下:

1. 移除不同 `Android Api Level` 使用不同的 `libfakelinker.so`, 现在内部采用函数表的方式自定适配不同 `Android` 版本
2. 新增稳定的 `C/C++` [FakeLinker](library/src/main/cpp/include/fake_linker.h) 函数表, 后续版本保证向前兼容, 不会改变 `FakeLinker` 已有的函数指针偏移, 可通过`FakeLinker.get_fakelinker_version` 获取当前版本号
3. 新增 `fake-linker` 静态库支持, 可自定义 `fake-linker` 以及仅使用其中的 [elf_reader.h](library/src/main/cpp/include/elf_reader.h), [jni_helper.h](library/src/main/cpp/include/jni_helper.h), [maps_util.h](library/src/main/cpp/include/maps_util.h) 等接口, 可实现 `fake-linker` 自身也是 `hook` 模块, 减少 `so` 数量
4. 可选的 `Java FakeLinker Api`, 新版不再强制需要 `Java Api` 可按需删除, 也可以自定义类名动态注册 `Java Api`

以下是针对子项目描述

1. `library` 项目是 `fake-linker` 的核心实现, 提供 `fakelinker` 静态库与动态库, **注意链接 `fakelinker` 动态库时仅可使用 `fake_linker.h` 头文件, 其它头文件需要链接 `fakelinker_static` 静态库才支持**
2. `emulator-testapp` 是在模拟器中测试 `fake-linker` 功能,支持 `houdini` 架构,即虽然采用动态翻译支持 `arm` 架构,但实际是可以加载 `x86`/`x64` 的库,因此实际加载 `fake-linker` 的 `x86`/`x64` 架构来提供功能
3. `fakelinker-test` 是正常 `apk` 测试 `x86`, `x86_64`, `arm`, `arm64` 架构,包含 `androidTest` 与可执行测试程序, `androidTest` 可以在导入 `Android Studio` 中通过 `UI` 运行 `FakelinkerGTest` 或执行命令行 `./gradlew :fakelinker-test:connectedDebugAndroidTest` 连接 `Android` 手机测试. 可执行测试程序则通过 `adb shell` 推送`fake-linker\fakelinker-test\src\main\cpp\build\Debug\xxxx\fakelinker_static_test` 到手机中添加可执行权限然后直接运行测试
4. `Stub` 项目仅提供私有 `api` 接口, 不打包到 `apk` 中

## 构建与集成

1. 源码构建

   > 参考 [local.properties.sample](local.properties.sample) 说明配置构建参数, 然后重命名为 `local.properties`, 后续将项目导入 `Android Studio` 或直接 `gradle` 命令行编译, 可以参考 [工作流](.github/workflows/android.yml)

   **注意新版本添加了 `BaseDexClassLoader.getLdLibraryPath`,`BaseDexClassLoader.addNativePath` 私有 api,在增量编译时可能报错**

   ```shell
   ...fake-linker\library\src\main\java\com\sanfengandroid\fakelinker\FakeLinker.java:153: error: cannot find symbol
      ((BaseDexClassLoader) loader).addNativePath(paths);
   ```

   **可以清理下工程或者命令行调用 gradlew 添加 --rerun-tasks 参数清除构建缓存再编译,后续只要不改动 `library`项目代码编译成功后就不会影响增量编译**

2. 使用构建库

   > 下载[最新 Release 版本](https://github.com/sanfengAndroid/fake-linker/releases/latest) `aar` 或最新测试版本 [Action 工件提取 aar](https://github.com/sanfengAndroid/fake-linker/actions/workflows/android.yml) 文件作为库添加至项目依赖, 开启 [prefab](https://developer.android.com/build/native-dependencies?agpversion=4.1) 功能

   ```groovy
   // build.gradle

   android{
      buildFeatures {
        prefab true
      }
   }

   dependencies {
      implementation fileTree(dir: 'libs', include: ['*.jar', '*.aar'])
   }
   ```

   然后在 `CMakeLists.txt` 中使用

   ```cmake
   find_package(FakeLinker REQUIRED CONFIG)

   ...

   target_link_libraries(
    your_target
    PRIVATE
    # 动态库依赖
    FakeLinker::fakelinker
    # 静态库依赖
    # FakeLinker::fakelinker_static
   )
   ```

3. CMake 集成

   > 使用 `FetchContent` 直接远程下载构建,添加以下代码到 `CMakeLists.txt` 中

   ```CMake
   include(FetchContent)
   FetchContent_Declare(
   fakelinker
   GIT_REPOSITORY https://github.com/sanfengAndroid/fake-linker.git
   SOURCE_SUBDIR library/src/main/cpp
   )

   FetchContent_MakeAvailable(fakelinker)
   ```

   然后在 `CMakeLists.txt` 中添加 `fakelinker` 模块的依赖

   ```CMake
   target_link_libraries(
    your_target
    PRIVATE
    # 动态库依赖
    fakelinker
    # 静态库依赖
    # fakelinker_static
   )
   ```

   **注意:** 该方式只使用了 `fake-linker` 的 `native` 库, 没有使用 `Java` 代码 [FakeLinker](library/src/main/java/com/sanfengandroid/fakelinker/FakeLinker.java) 和 [ErrorCode](library/src/main/java/com/sanfengandroid/fakelinker/ErrorCode.java), 可自行拷贝文件到项目, 也可仅使用 `native` 接口

## Hook 模块开发

使用 `aar` 包导入和 `CMake` 集成方式均已导入了 `fakelinker` 依赖, 因此直接在 `CMakeLists.txt` 中链接 `fakelinker` 动态库

```CMake
target_link_libraries(
  your_target
  PRIVATE
  # 动态库依赖
  fakelinker
  # 静态库依赖
  # fakelinker_static
)
```

因为模块已经对 `fakelinker` 有依赖, 所以有两种加载方式, 一种是采用旧版本 Java 层调用 `FakeLinker` 下的初始化方法, 方式二是直接 `System.loadLibrary(hookModule)` 它依赖 `libfakelinker.so` 会自动加载初始化`fakelinker`

### 方式一 旧版 Java 层调用

#### fakelinker 与 hook 模块初始化

本质是加载 `libfakelinker.so` 和 `hook` 模块, 但由于使用的方式不同因此加载的方式不同, 当初始化 `libfakelinker.so` 后会自动加载 `hook` 模块并回调 `fakelinker_module_init` 方法, **注意:当`fake-linker`初始化失败时不会加载和回调`hook`模块**

1. 项目自身使用
   > 可直接调用 `FakeLinker.initFakeLinker` 从 `apk` 内加载
2. `Xposed` 模块使用

   > [LSPosed](https://github.com/LSPosed/LSPosed) 框架内部已处理了 `native` 库搜索路径, 因此只需在 `build.gradle` 中以下配置, 关闭`so`压缩(它在`minSdk >= 23`默认开启),然后调用 `FakeLinker.initFakeLinker` 从 `Xposed` 模块 `APK` 中加载

   ```gradle
   android {
       packagingOptions {
           jniLibs {
               useLegacyPackaging true
           }
       }
   }
   ```

   > 非 `LSPosed` 框架或低版本则需要设置 `native` 库搜索路径然后加载, 见方式 3

3. 动态设置 `native` 库搜索路径
   > `v3.1.0+` 版本提供 [FakeLinker.addHookApkNativePath](src/main/java/com/sanfengandroid/fakelinker/FakeLinker.java) 或 [FakeLinker.addNativeLibraryPath](src/main/java/com/sanfengandroid/fakelinker/FakeLinker.java) 接口更改类加载器的 `native` 库搜索路径, 默认类加载器是 `FakeLinker` 类所属的类加载器, 因为从 `FakeLinker` 调用加载保持默认即可, 当然也可以调用它修改其它类加载器的搜索路径, 设置好 `fakelinker` 和 `hook` 模块的搜索路径再调用 `FakeLinker.initFakeLinker` 初始化即可
4. 手动安装库到指定位置,将 `fake-linker` 和 `Hook` 模块安装到应用有权限访问的路径,如: `/data/local/tmp`, 然后直接绝对路径加载, **不推荐可以设置 `native` 库搜索路径后加载**

#### 编写 hook 模块

1. 包含 `fake_linker.h` 头文件实现 `fakelinker_module_init` 方法, 需要导出 `fakelinker_module_init` 该方法, 否则 `fakelinker` 无法回调它

   ```c++
   #include <fake_linker.h>

   C_API API_PUBLIC void fakelinker_module_init(JNIEnv *env, SoinfoPtr fake_soinfo,
   const FakeLinker *fake_linker){
    // 在这里设置全局 so, 重定位已加载的 so 等等操作
   }
   ```

### 方式二 native 层调用

由于 `hook` 模块依赖 `libfakelinker.so`, 所以可以直接调用 `System.loadLibrary("hook-module")` 自动初始化 `fakelinker`, 该方式可以完全不需要 `FakeLinker` 相关的 `Java` 代码, 因为所有功能都只是对内部的 `native FakeLinker` 结构体包装, 使用方式与方式一有区别

#### hook 模块与 fakelinker 初始化

直接加载 `hook` 模块

```Java
try{
  System.loadLibrary("hook-module");
}catch(UnsatisfiedLinkError e){
  // 检查错误
}
```

此时已自动初始化了 `fakelinker` 并默认注册 `com/sanfengandroid/fakelinker/FakeLinker` 类下的 `native` 方法, 如果不存在则不会注册

#### 编写 hook 模块

该方式不再需要 `fakelinker_module_init` 函数, 直接通过 `get_fakelinker` 获取 `const FakeLinker *`指针, 在使用之前应确保 `fake_linker->is_init_success()` 初始化成功才使用

```c++
// hook_module.cpp

C_API API_PUBLIC jint JNI_OnLoad(JavaVM *vm, void *reserved){
  const FakeLinker *fake_linker = get_fakelinker();
  // 获取 FakeLinker 指针并检查初始化是否成功
  JNIEnv *env;
  if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
    async_safe_fatal("JNI environment error");
  }
  // 初始化 fake-linker
  init_fakelinker(env, static_cast<FakeLinkerMode>(FakeLinkerMode::kFMSoinfo |FakeLinkerMode::kFMNativeHook |FakeLinkerMode::kFMJavaRegister), nullptr);

  if(fake_linker->is_init_success()){
    // 获取自身 soinfo
    int error;
    SoinfoPtr thiz = fake_linker->soinfo_find(SoinfoFindType::kSTAddress, nullptr, &error);
    if(thiz){
      // 同方式一设置全局 so, 重定位已加载的 so 等等操作
      ...
    }

    // 调用 init_fakelinker 初始化判断 native hook 是否成功
    if(init_fakelinker(env, FakeLinkerMode::kFMNativeHook, nullptr) == 0){
      LOGI("native hook init success");
    }
    // 注册自定义Java api
    if(init_fakelinker(env, FakeLinkerMode::kFMJavaRegister, "java/to/your/class") == 0){
      LOGI("register custom java api success");
    }
  }else{
    // 错误处理,不能使用 fake_linker 相关方法
  }
  return JNI_VERSION_1_6;
}
```

## fakelinker 静态库的使用

从 `v3.1.0` 起提供 `fakelinker_static` 静态库, 可以自定义 `fakelinker` 或使用它提供的部分 `native api`, 静态连接不包含 `JNI_OnLoad` 因此需要自行初始化, 可以按需调用 `init_fakelinker` 初始化相应功能, **请在确定初始化成功后才使用对应功能**, 使用方式类似上面的方式二,常用代码如下

```c++
#include <fake_linker.h>

C_API API_PUBLIC jint JNI_OnLoad(JavaVM *vm, void *reserved){
  const FakeLinker *fake_linker = get_fakelinker();
  // 获取 FakeLinker 指针并检查初始化是否成功
  JNIEnv *env;
  if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
    async_safe_fatal("JNI environment error");
  }
  if(init_fakelinker(env, static_cast<FakeLinkerMode>(FakeLinkerMode::kFMSoinfo |FakeLinkerMode::kFMNativeHook |FakeLinkerMode::kFMJavaRegister), nullptr) == 0){
   // 初始化成功
  }
}
```

静态库链接的另一种常见用法是 `fake-linker` 既是 `fake-linker` 框架,又是 `hook` 模块, 初始化后将自身设置为全局 `so` 即可生效.

## 其它说明

1. 项目不同于直接设置 `LD_PRELOAD` 环境变量, 直接设置通常无法拦截 `dlsym` 方法, 因为一旦拦截则需要自己实现查找符号且高版本还有`caller`地址限制, 而该模块通过中转模块 `fake-linker` 提供调用`dlsym`方法, 因此 `hook` 模块可以拦截`dlsym`, 且提供更多 `Linker` 相关功能

## 注意事项

1. 在使用 `Xposed`等类似框架 `Hook` 系统进程时请做好备份删除工作, 避免因为出错导致系统进程死亡无法开机的情况
2. 根据模块加载时机, 重定位已经加载的模块, 通常加载 `hook` 模块时一些系统库已经加载, 如 `libjavacore.so`, `libnativehelper.so`, `libnativeloader.so`, `libart.so`, `libopenjdk.so` 等等, 要想这些库生效则需要 `native` 中手动调用 `FakeLinker.call_manual_relocation_by_names` 等方法
3. 将 `hook` 模块设为全局库, 后续加载的 `so` 才会自动生效
4. 在使用 `fake-linker` 相关功能时应先 调用 `init_fakelinker` 判断对应功能是否初始化成功, `init_fakelinker` 可重复调用并且不会重复初始化

## 使用示例

1. 对已经加载的 `so` 执行重定位, 使 `hook` 模块生效

   ```c++
   void fakelinker_module_init(JNIEnv *env, SoinfoPtr fake_soinfo, const FakeLinker *fake_linker){
     const char* loaded_libs[] = {
       "libart.so",
       "libopenjdk.so",
       "libnativehelper.so",
       "libjavacore.so",
     };
     fake_linker->call_manual_relocation_by_names(fake_soinfo, 4, loaded_libs);
   }
   ```

2. 将 `hook` 模块设置为全局库

   ```c++
   C_API API_PUBLIC jint JNI_OnLoad(JavaVM *vm, void *reserved) {
       const FakeLinker *fake_linker = get_fakelinker();
       // 获取 FakeLinker 指针并检查初始化是否成功
       JNIEnv *env;
       if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
         async_safe_fatal("JNI environment error");
       }
       if (init_fakelinker(env, static_cast<FakeLinkerMode>(FakeLinkerMode::kFMSoinfo | FakeLinkerMode::kFMNativeHook | FakeLinkerMode::kFMJavaRegister),
                           nullptr) == 0) {
         // 初始化成功
         if (SoinfoPtr thiz = fake_linker->soinfo_find(SoinfoFindType::kSTAddress, nullptr, nullptr)) {
           // 设置hook模块为全局库,之后所有 so 加载 hook 均会生效
           if (fake_linker->soinfo_add_to_global(thiz)) {
             LOG("soinfo add to global success");
           }
         }
       }
   }

   // 当已经对需要的 so hook 生效之后可以取消全局 so 设置,它不会影响已加载的库
   fake_linker->soinfo_remove_global(thiz);
   ```

3. 其它更多使用
   参考 [FakeLinker](library/src/main/cpp/include/fake_linker.h) 结构体中方法指针, 现承诺自 `v3.1.0` 版本起保持 `api` 兼容
