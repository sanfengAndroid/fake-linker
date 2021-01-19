/*
 * Copyright (c) 2021 XpFilter by beich.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.sanfengandroid.fakelinker;

import android.content.Context;
import android.os.Build;
import android.os.Process;
import android.text.TextUtils;
import android.util.Log;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import eu.chainfire.libsuperuser.Shell;

public class FileInstaller {
    private static final String TAG = FileInstaller.class.getSimpleName();
    private static final boolean isX86;
    private static final boolean support64Bit;
    private static final String RUNNING_ABI;
    private static final String PATH_32;
    private static final String PATH_64;
    private static final String LINKER_MODULE;
    private static String execName;
    private static String configPath;
    private static int uid = Process.myUid();
    private static int gid = uid;
    private static String libXattr = "u:object_r:system_file:s0";
    private static String fileXattr = "u:object_r:system_data_file:s0";

    static {
        boolean temp = false;
        for (String name : Build.SUPPORTED_32_BIT_ABIS) {
            if (name.contains("x86")) {
                temp = true;
                break;
            }
        }
        isX86 = temp;
        temp = false;
        for (String abi : Build.SUPPORTED_ABIS) {
            if (abi.contains("64")) {
                temp = true;
                break;
            }
        }
        support64Bit = temp;
        if (support64Bit && (Build.CPU_ABI.contains("64") || Build.CPU_ABI2.contains("64"))) {
            RUNNING_ABI = isX86 ? "x86_64" : "arm64-v8a";
        } else {
            RUNNING_ABI = isX86 ? "x86" : "armeabi-v7a";
        }
        PATH_32 = isX86 ? "x86" : "armeabi-v7a";
        PATH_64 = isX86 ? "x86_64" : "arm64-v8a";
        LINKER_MODULE = "lib" + BuildConfig.LINKER_MODULE_NAME + "-" + Build.VERSION.SDK_INT + ".so";
    }

    public static void extractFakeLibrary(Context context) throws Exception {
        extractLibrary(context, LINKER_MODULE);
    }

    public static void extractLibrary(Context context, String name) throws Exception {
        String entry = "lib" + File.separator + PATH_32 + File.separator + name;
        releaseFile(context, entry, context.getCacheDir() + File.separator + PATH_32 + File.separator + name);
        if (support64Bit) {
            entry = "lib" + File.separator + PATH_64 + File.separator + name;
            releaseFile(context, entry, context.getCacheDir() + File.separator + PATH_64 + File.separator + name);
        }
    }

    public static void installFakeLibrary(Context context, boolean root) throws Exception {
        extractInstallFile(context);
        installLibrary(context, LINKER_MODULE, root);
    }

    public static void setConfigPath(String path) {
        configPath = path;
    }

    public static void installLibrary(final Context context, String name, boolean root) throws Exception {
        if (execName == null) {
            extractInstallFile(context);
        }
        if (TextUtils.isEmpty(configPath)) {
            throw new FileNotFoundException("Please set the installation path first, invoke 'setConfigPath' method");
        }
        List<String> list = new ArrayList<>();
        list.add(context.getCacheDir() + File.separator + execName);
        list.add("copy");
        list.add("" + uid);
        list.add("" + gid);
        list.add("lib");
        list.add(libXattr);
        list.add(configPath);
        list.add(configPath + File.separator + PATH_32);
        list.add(context.getCacheDir() + File.separator + PATH_32 + File.separator + name);
        if (support64Bit) {
            list.add(configPath + File.separator + PATH_64);
            list.add(context.getCacheDir() + File.separator + PATH_64 + File.separator + name);
        }
        executeSuccess(list, root);
    }

    public static void installFile(final Context context, String[] files, boolean root) throws Exception {
        if (execName == null) {
            extractInstallFile(context);
        }
        if (TextUtils.isEmpty(configPath)) {
            throw new FileNotFoundException("Please set the installation path first, invoke 'setConfigPath' method");
        }
        List<String> list = new ArrayList<>();
        list.add(context.getCacheDir() + File.separator + execName);
        list.add("copy");
        list.add("" + uid);
        list.add("" + gid);
        list.add("file");
        list.add(fileXattr);
        list.add(configPath);
        for (String file : files) {
            list.add(configPath);
            list.add(context.getCacheDir() + File.separator + file);
        }
        executeSuccess(list, root);
    }

    public static void uninstallLibrary(Context context, boolean root) throws Exception {
        if (TextUtils.isEmpty(configPath)) {
            throw new FileNotFoundException("Please set the installation path first, invoke 'setConfigPath' method");
        }
        if (execName == null) {
            extractInstallFile(context);
        }
        List<String> list = new ArrayList<>();
        list.add(context.getCacheDir() + File.separator + execName);
        list.add("remove");
        list.add(configPath + File.separator + PATH_32);
        if (support64Bit) {
            list.add(configPath + File.separator + PATH_64);
        }
        executeSuccess(list, root);
    }

    public static void uninstallFile(Context context, String[] files, boolean root) throws Exception {
        if (TextUtils.isEmpty(configPath)) {
            throw new FileNotFoundException("Please set the installation path first, invoke 'setConfigPath' method");
        }
        if (files == null || files.length == 0) {
            return;
        }
        if (execName == null) {
            extractInstallFile(context);
        }
        List<String> list = new ArrayList<>();
        list.add(context.getCacheDir() + File.separator + execName);
        list.add("remove");
        for (String file : files) {
            list.add(configPath + File.separator + file);
        }
        executeSuccess(list, root);
    }

    public static boolean hasInstallFakeLibrary() {
        return hasInstallLibrary(LINKER_MODULE);
    }

    public static boolean hasInstallLibrary(String name) {
        if (TextUtils.isEmpty(configPath)) {
            return false;
        }
        String hookPath32 = configPath + File.separator + PATH_32;
        boolean installed = new File(hookPath32, name).exists();
        if (support64Bit) {
            String hookPath64 = configPath + File.separator + PATH_64;
            installed &= new File(hookPath64, name).exists();
        }
        return installed;
    }

    public static boolean hasInstallFile(String file) {
        if (TextUtils.isEmpty(configPath)) {
            return false;
        }
        return new File(configPath, file).exists();
    }

    public static boolean hasInstallFile(String[] files) {
        boolean success = true;
        for (String file : files) {
            success &= hasInstallFile(file);
        }
        return success;
    }

    /**
     * 不同Hook模式下需要的uid,gid不同,没有root权限可以设置为应用自身uid,gid
     *
     * @param uid 文件uid
     * @param gid 文件gid
     */
    public static void setFileOwner(int uid, int gid) {
        FileInstaller.uid = uid;
        FileInstaller.gid = gid;
    }

    /**
     * @param libXattr  库文件的selinux属性
     * @param fileXattr 普通文件的selinux属性
     */
    public static void setFileXattr(String libXattr, String fileXattr) {
        FileInstaller.libXattr = libXattr;
        FileInstaller.fileXattr = fileXattr;
    }

    public static String getRunningAbi() {
        return RUNNING_ABI;
    }

    public static boolean isIsX86() {
        return isX86;
    }

    public static boolean isSupport64Bit() {
        return support64Bit;
    }

    private static void executeSuccess(List<String> cmds, boolean root) throws Exception {
        if (root && !Shell.SU.available()) {
            throw new Exception("Execution error, no root permission");
        }
        StringBuilder sb = new StringBuilder();
        for (String v : cmds) {
            sb.append(v).append(" ");
        }
        List<String> out = root ? Shell.SU.run(sb.toString()) : Shell.SH.run(sb.toString());
        Log.d(TAG, "install file result: " + (out == null ? "error" : Arrays.toString(out.toArray())));
        if (out == null) {
            throw new Exception("Install file failed, cmd: " + sb.toString());
        }
        if (out.isEmpty()) {
            return;
        }
        throw new Exception("Install file failed, result: " + Arrays.toString(out.toArray()) + "\ncmd: " + sb.toString());
    }

    private static void releaseFile(Context context, String entryName, String out) throws Exception {
        try {
            File file = new File(out);
            if (file.getParentFile() == null) {
                throw new IOException("release error path: " + file.getAbsolutePath());
            }
            if (!file.getParentFile().exists()) {
                if (!file.getParentFile().mkdirs()) {
                    throw new IOException("create dir error: " + file.getParentFile().getAbsolutePath());
                }
            }
            ZipFile zip = new ZipFile(context.getPackageResourcePath());
            ZipEntry entry = zip.getEntry(entryName);
            InputStream is = zip.getInputStream(entry);
            byte[] bytes = new byte[4096];
            OutputStream os = new FileOutputStream(file);
            int len;
            while ((len = is.read(bytes)) != -1) {
                os.write(bytes, 0, len);
            }
            os.flush();
            os.close();
            is.close();
        } catch (IOException e) {
            throw new IOException("write file error: " + e.getMessage());
        }
    }

    private static void extractInstallFile(Context context) throws Exception {
        String entry = "assets/" + RUNNING_ABI + File.separator + BuildConfig.HOOK_INSTALL_MODULE_NAME;
        releaseFile(context, entry, context.getCacheDir() + File.separator + BuildConfig.HOOK_INSTALL_MODULE_NAME);
        List<String> result = Shell.SH.run("chmod 755 " + context.getCacheDir() + File.separator + BuildConfig.HOOK_INSTALL_MODULE_NAME);
        if (result == null || !result.isEmpty()) {
            throw new IOException("Extract install file failed, result: " + (result == null ? "null" : Arrays.toString(result.toArray())));
        }
        execName = BuildConfig.HOOK_INSTALL_MODULE_NAME;
    }
}
