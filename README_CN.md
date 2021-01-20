# fake-linker

## 项目描述

进程内动态查找和修改 `Linker` 数据，对外提供基于 `LD_PRELOAD` 模式加载 `PLT hook` 模块和各种操作 `soinfo`、`namespace`的功能。原理分析请查看[Android 动态修改Linker实现LD_PRELOAD全局库PLT Hook](https://sanfengandroid.github.io/2021/01/10/modify-linker-to-implement-plt-hook/)

## 支持的Android版本

支持版本：`Android 5.0 ~ Android 11+`，支持架构：`x86`，`x86_64`，`arm`，`arm64`

## 构建

1. 源码构建
> 将其作为 `Android Library` 添加到 `Android` 项目中，主模块添加其依赖，修改[`build.gradle`](build.gradle)中的 `buildApi` 变量编译指定Api等级
2. 使用构建库
> 下载[最新版本](https://github.com/sanfengAndroid/fake-linker/releases/latest)二进制文件解压将 `aar` 文件作为库添加至项目依赖，将 `include` 目录下头文件导入到Hook模块使用

## Hook 模块开发

1. 复制导出头文件(源码在 `cpp` 目录下的 [export](src/main/cpp/export) 目录)到 `Hook` 模块
2. 实现[`linker_export.h`](src/main/cpp/export/linker_export.h)中的 `fake_load_library_init` 导出函数
3. 调用各种实现方法，查看[`linker_export.h`](src/main/cpp/export/linker_export.h)中的 `RemoteInvokeInterface` 定义
4. 正常编写Hook方法如: `open`，`dlopen`，`dlsym`方法等等，方法必须被导出
5. Hook模块区分Android7.0以下(无命名空间，`soinfo handle`)，Android7.0及以上(有命名空间，`soinfo handle`)

## Java层初始化

1. 正确安装库到指定位置，将 `fake-linker` 和 `Hook` 模块安装到应用有权限访问的路径，如: `/data/local/tmp`，自身使用则可调用系统方法直接加载
模块已经集成了安装可执行文件，Java层调用 [FileInstaller](src/main/java/com/sanfengandroid/fakelinker/FileInstaller.java) 类下的方法安装，内部已经处理了各个不同的平台架构
2. 通过`FileInstaller`设置`fake-linker`模块，`Hook`模块所需不同的 [selinux](src/main/java/com/sanfengandroid/fakelinker/FileInstaller.java/#L232) 和 [uid, gid](src/main/java/com/sanfengandroid/fakelinker/FileInstaller.java/#L223) 文件属性
3. 加载并初始化 `fake-linker` 模块，调用[`FakeLinker.initFakeLinker`](src/main/java/com/sanfengandroid/fakelinker/FakeLinker.java)方法，内部会加载Hook模块并回调 `fake_load_library_init` 方法完成模块初始化

## 其它描述

1. 项目不同于直接设置 `LD_PRELOAD` 环境变量，直接设置通常无法拦截 `dlsym` 方法，因为一旦拦截则需要自己实现查找符号且高版本还有`caller`地址限制，而该模块通过中转模块 `fake-linker` 提供调用`dlsym`方法，因此Hook模块可以拦截`dlsym`，且提供更多 `Linker` 相关功能
2. Android各个版本`Linker`都有对应的修改，因此模块依赖手机的 `api` 等级，不同等级加载对应模块即可，内部使用`NDK`版本为最新版，该版本已经删除 `Api-25`，代码内是支持 `Api-25` 需要切换低版本`NDK`来编译，需要自己适配代码

## 注意事项

1. Hook系统进程时请做好备份删除工作，避免因为出错导致系统进程死亡无法开机的情况
2. 根据模块加载时机，重定位已经加载的模块
## 使用示例
1. 将`Hook`模块设置为全局库 `remote->CallCommonFunction(kCFAddSoinfoToGlobal, kSPAddress, nullptr, kSPNull, nullptr, &error_code);`
2. 重定位一些已经加载完成的模块 `remote->CallCommonFunction(kCFCallManualRelinks, kSPAddress, nullptr, kSPNames, libs, &error_code);`
    ```c++
    static const RemoteInvokeInterface *remote;
    // Hook JNI 函数RegisterNatives
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
        // 将本Hook模块添加到全局模块中,这会影响之后加载的所有模块
        remote->CallCommonFunction(kCFAddSoinfoToGlobal, kSPAddress, nullptr, kSPNull, nullptr, &error_code);
         if (error_code != kErrorNo) {
            LOGE("init global soinfo error, error code: %x", error_code);
            return;
        }
        VarLengthObject<const char *> *libs;
        // 重新解析以下模块的导入表,因为以下模块在我们还未加载前就已经加载完成了,所有重新链接使其符号链接到我们的Hook方法
        // java系统代码也主要使用是以下几个库,重链接也意味着Hook的java的核心导入函数
        libs = VaArgsToVarLengthObject<const char *>(5, "libjavacore.so", "libnativehelper.so", "libnativeloader.so", "libart.so", "libopenjdk.so");
        remote->CallCommonFunction(kCFCallManualRelinks, kSPAddress, nullptr, kSPNames, libs, &error_code);
        // Hook JNI 接口,监听RegisterNatives方法
        remote_->HookJniNative(offsetof(JNINativeInterface, RegisterNatives), (void *)HookJniRegisterNatives, nullptr);
    }
    C_API void fake_load_library_init(JNIEnv *env, void *fake_soinfo, const RemoteInvokeInterface *interface, 
    const char *cache_path, const char *config_path, const char *_process_name){
        remote = interface;
        InitHook();
    }
    ```
3. 其它更多使用请等待后续另一仓库