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

public class FakeLinker {
  private static final String TAG = FakeLinker.class.getCanonicalName();

  private static native boolean entrance(String hookModulePath);

  private static native void setLogLevel(int level);

  private static native int relocationFilterSymbol(String symbolName,
                                                   boolean add);

  public static native void nativeOffset();

  public static native void removeAllRelocationFilterSymbol();
}
