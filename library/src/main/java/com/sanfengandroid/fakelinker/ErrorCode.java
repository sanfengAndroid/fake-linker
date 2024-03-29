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

/**
 * 错误代码,与C层错误代码相对应
 *
 * @author sanfengAndroid
 */

public enum ErrorCode {
  // 没有错误
  ERROR_NO(0),
  // 命名空间 null错误
  ERROR_NP_NULL(1),
  // 命名空间名字错误
  ERROR_NP_NAME(1 << 1),
  // 没找到对应的命名空间
  ERROR_NP_NOT_FOUND(1 << 2),
  // soinfo null错误
  ERROR_SO_NULL(1 << 3),
  // 没找到对应的soinfo
  ERROR_SO_NOT_FOUND(1 << 4),
  // 没找到指定符号
  ERROR_SO_SYMBOL_NOT_FOUND(1 << 5),
  // 重定位库失败
  ERROR_SOINFO_RELINK(1 << 6),
  // 该功能未定义
  ERROR_FUNCTION_UNDEFINED(1 << 7),
  // Api等级不满足条件
  ERROR_API_LEVEL_NOT_MATCH(1 << 8),
  // 参数类型不匹配错误
  ERROR_PARAMETER_TYPE(1 << 9),
  // 参数为null错误
  ERROR_PARAMETER_NULL(1 << 10),
  // 内部内存错误
  ERROR_Memory(1 << 11),
  // native执行失败
  ERROR_EXEC_ERROR(1 << 12),

  // hook jni 方法偏移超出范围
  ERROR_JNI_OFFSET(1 << 13),
  // hook jni 空的hook方法错误
  ERROR_JNI_METHOD_NULL(1 << 14),
  // 重复的hook jni 方法错误
  ERROR_JNI_REPEAT_OPERATION(1 << 15),
  // 内部执行hook jni错误
  ERROR_JNI_EXECUTE(1 << 16),
  // Java层添加,执行错误,参数验证不通过
  ERROR_JAVA_EXECUTE(1 << 20);

  public final int code;

  ErrorCode(int code) { this.code = code; }

  public static ErrorCode codeToError(int code) {
    for (ErrorCode error : ErrorCode.values()) {
      if (error.code == code) {
        return error;
      }
    }
    return null;
  }
}
