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
import android.os.Process;
import android.util.Log;
import dalvik.system.VMRuntime;

public class FakeLinker {
  private static final String TAG = FakeLinker.class.getCanonicalName();

  private static String libraryName = "fakelinker";

  private static native boolean entrance(String hookModulePath);

  private static native void setLogLevel(int level);

  private static native int relocationFilterSymbol(String symbolName,
                                                   boolean add);

  public static native void nativeOffset();

  public static native void removeAllRelocationFilterSymbol();

  public static void localLoad() {
    try {
      System.loadLibrary(libraryName);
    } catch (UnsatisfiedLinkError ignore) {
      try {
        System.loadLibrary(libraryName + (is64Bit() ? "64" : "32"));
      } catch (UnsatisfiedLinkError e) {
        Log.e(TAG, "load fake linker library error", e);
      }
    }
  }

  public static boolean is64Bit() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
      return Process.is64Bit();
    }
    return VMRuntime.getRuntime().is64Bit();
  }

  @SuppressLint("UnsafeDynamicallyLoadedCode")
  public static void initFakeLinker(String linkerPath, String hookModulePath) {
    try {
      if (linkerPath == null) {
        localLoad();
      } else {
        System.load(linkerPath);
      }
      Log.d(TAG, "initialization fake linker: " + entrance(hookModulePath));
    } catch (UnsatisfiedLinkError e) {
      Log.e(TAG, "load fake linker library error", e);
    }
  }

  public static void initLocalFakeLinker(String hookModulePath) {
    try {
      localLoad();
      Log.d(TAG, "initialization fake linker: " + entrance(hookModulePath));
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
      return ErrorCode.codeToError(relocationFilterSymbol(symbolName, add));
    } catch (Throwable e) {
      Log.e(TAG, "relink filter symbol error: " + symbolName, e);
      return ErrorCode.ERROR_JAVA_EXECUTE;
    }
  }

  /**
   * @param level 最低log等级
   */
  public static void setNativeLogLevel(int level) { setLogLevel(level); }

  public static void setLibraryName(String libraryName) {
    FakeLinker.libraryName = libraryName;
  }
}
