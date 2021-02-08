/*
 * Copyright (c) 2021 fake-linker by sanfengAndroid.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

package com.sanfengandroid.fakelinker;

import android.annotation.SuppressLint;
import android.os.Build;
import android.util.Log;


public class FakeLinker {
    private static final String TAG = FakeLinker.class.getCanonicalName();

    /**
     * Native Hook入口
     *
     * @param cacheDir    缓存路径,必须app有可写的权限,临时文件缓存在这里
     * @param processName 当前进程名
     * @return 加载是否成功
     */
    private static native boolean entrance(String hookModulePath, String cacheDir, String configPath, String processName);

    private static native void setLogLevel(int level);

    private static native int relinkSpecialFilterSymbol(String symbolName, boolean add);


    public static void localLoad() {
        try {
            System.loadLibrary(BuildConfig.LINKER_MODULE_NAME + "-" + Build.VERSION.SDK_INT);
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "load fake linker library error", e);
        }
    }

    @SuppressLint("UnsafeDynamicallyLoadedCode")
    public static void initFakeLinker(String linkerPath, String hookModulePath, String cacheDir, String configPath, String processName) {
        try {
            System.load(linkerPath);
            Log.d(TAG, "initialization fake linker: " + entrance(hookModulePath, cacheDir, configPath, processName));
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "load fake linker library error", e);
        }
    }

    public static void initFakeLinker(String hookModulePath, String cacheDir, String configPath, String processName) {
        try {
            localLoad();
            Log.d(TAG, "initialization fake linker: " + entrance(hookModulePath, cacheDir, configPath, processName));
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "load fake linker library error", e);
        }
    }

    /**
     * 添加/删除 手动重定位过滤指定的全局符号名
     *
     * @param symbolName 过滤符号名
     * @param add        添加/删除
     * @return 调用结果
     */
    public static ErrorCode relinkFilterSymbol(String symbolName, boolean add) {
        try {
            if (symbolName == null || symbolName.isEmpty()) {
                return ErrorCode.ERROR_JAVA_EXECUTE;
            }
            return ErrorCode.codeToError(relinkSpecialFilterSymbol(symbolName, add));
        } catch (Throwable e) {
            Log.e(TAG, "relink filter symbol error: " + symbolName, e);
            return ErrorCode.ERROR_JAVA_EXECUTE;
        }
    }

    /**
     * @param level 最低log等级
     */
    public static void setNativeLogLevel(int level) {
        setLogLevel(level);
    }
}
