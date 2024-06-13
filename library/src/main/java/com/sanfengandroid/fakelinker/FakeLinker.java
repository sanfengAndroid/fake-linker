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
import dalvik.system.BaseDexClassLoader;
import dalvik.system.VMRuntime;
import java.io.File;
import java.lang.reflect.Modifier;
import java.util.Collection;
import java.util.Collections;

public class FakeLinker {
  private static final String TAG = FakeLinker.class.getCanonicalName();

  private static String libraryName = "fakelinker";

  private static native boolean entrance(String hookModulePath);

  private static native void setLogLevel(int level);

  private static native int relocationFilterSymbol(String symbolName,
                                                   boolean add);

  public static native void nativeOffset();

  public static native void removeAllRelocationFilterSymbol();

  /**
   * Load so from the current class loader environment. If loading fails, add
   * the process bit suffix and load again. You can call {@link
   * #setLibraryName(String)} to set the default loading library name, usually
   * customized fakelinker use
   */
  public static void localLoad() {
    try {
      int v = Modifier.PRIVATE | Modifier.STATIC | Modifier.NATIVE;
      System.loadLibrary(libraryName);
    } catch (UnsatisfiedLinkError ignore) {
      try {
        System.loadLibrary(libraryName + (is64Bit() ? "64" : "32"));
      } catch (UnsatisfiedLinkError e) {
        Log.e(TAG, "load fake linker library error", e);
      }
    }
  }

  /**
   * @return Is the current process a 64-bit process
   */
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

  /**
   * View {@link #initFakeLinker(String, String)}
   *
   * @param hookModulePath hook module path
   */
  public static void initLocalFakeLinker(String hookModulePath) {
    initFakeLinker(null, hookModulePath);
  }

  /**
   * Add/Remove global symbol names specified by manual relocation filter
   *
   * @param symbolName filter symbol name
   * @param add        add or remove
   * @return error code
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
   * Dynamically change fakelinker internal log level
   *
   * @param level min log level
   */
  public static void setNativeLogLevel(int level) { setLogLevel(level); }

  /**
   * Set custom loading library name when changing default fakelinker library
   * name
   *
   * @param libraryName fakelinker new library name
   */
  public static void setLibraryName(String libraryName) {
    FakeLinker.libraryName = libraryName;
  }

  public void addNativeLibraryPath(ClassLoader loader, String path) {
    addNativeLibraryPath(loader, Collections.singletonList(path));
  }

  /**
   * Add the Native library search path to the specified class loader, which can
   * be /data/app/xxxx/base.apk!/lib/arm64-v8a apk internal path, but the
   * premise for the effectiveness of the apk so is not compressed and 4k
   * aligned, that is, the android:extractNativeLibs attribute is false
   * <p><b>Warning:</b> The added path is at the end, if you want to add it at
   * the beginning, you should use reflection.</p>
   *
   * @param loader The specified class loader, the default is the current class
   *     loader
   * @param paths  A collection of native search library paths to add
   */
  public void addNativeLibraryPath(ClassLoader loader,
                                   Collection<String> paths) {
    if (loader == null) {
      loader = getClass().getClassLoader();
    }
    if (loader instanceof BaseDexClassLoader) {
      ((BaseDexClassLoader)loader).addNativePath(paths);
    }
  }

  /**
   * Get the Native library search path of the specified class loader.
   * Note that {@link #addNativeLibraryPath(ClassLoader, Collection)} is called.
   * The added path will not be included because it accesses the path
   * initialized when the class loader is created, and the path added later will
   * not modify the path.
   *
   * @param loader The class loader to query
   * @return The native library path of the corresponding class loader
   */
  public String getNativeLdLibraryPath(ClassLoader loader) {
    if (loader == null) {
      loader = getClass().getClassLoader();
    }
    if (loader instanceof BaseDexClassLoader) {
      return ((BaseDexClassLoader)loader).getLdLibraryPath();
    }
    return "";
  }

  /**
   * Add the Native path of the current apk to the specified class loader, such
   * as adding its own library path in the xposed environment, if the apk is
   * android:extractNativeLibs=false, it can be loaded directly from the apk in
   * higher versions, Otherwise, only the architecture path supported by the
   * current mobile phone can be loaded. At this time, if you want to support
   * multiple architectures, you should package all the so of the architecture
   * into one path. This function queries the apk path from the existing
   * nativeLibraryDirectories. If there is no apk path, you should use {@link
   * #addNativeLibraryPath(ClassLoader, Collection)} to add the library path
   *
   * @param loader The specified class loader, the default is the current class
   *     loader
   * @return Returns true if the addition is successful
   */
  public boolean addHookApkNativePath(ClassLoader loader) {
    String path = getNativeLdLibraryPath(null);
    String apkPath = null;
    for (String item : path.split(":")) {
      if (item.contains("base.apk")) {
        apkPath = item;
        break;
      }
    }
    if (apkPath == null) {
      return false;
    }
    String apk = apkPath.split("!")[0];
    File lib = new File(new File(apk).getParentFile(), "lib");
    // After installation, only one architecture's so will be extracted
    File[] files = lib.listFiles();
    if (files == null || files.length == 0) {
      return false;
    }
    addNativeLibraryPath(loader, files[0].getAbsolutePath());
    return true;
  }
}
