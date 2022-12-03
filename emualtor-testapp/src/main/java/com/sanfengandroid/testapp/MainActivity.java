/*
 * Copyright (c) 2022 FakeXposed by sanfengAndroid.
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

package com.sanfengandroid.testapp;

import android.os.Bundle;
import android.util.Log;
import android.widget.EditText;
import androidx.appcompat.app.AppCompatActivity;
import com.sanfengandroid.fakelinker.FakeLinker;

public class MainActivity extends AppCompatActivity {
  private EditText relocateEdit;

  private long beforeAddress, afterAddress;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    findViewById(R.id.btnLoadFakelinker).setOnClickListener(view -> {
      try {
        // 模拟器可能加载到默认命名空间中,因此要先手动加载fakelinker模块
        System.loadLibrary(FakeLinker.is64Bit() ? "fakelinker-test64"
                                                : "fakelinker-test32");
        String name = FakeLinker.is64Bit() ? "libfakelinker-test64.so"
                                           : "libfakelinker-test32.so";
        FakeLinker.initFakeLinker(null, name);
        Log.w("FakeLinker_Test", "start load test module");
      } catch (Throwable e) {
        Log.e("Fake", "load error", e);
      }
    });
    relocateEdit = findViewById(R.id.etHoudini);

    findViewById(R.id.btnRelocate).setOnClickListener(v -> {
      relocateLibrary(relocateEdit.getText().toString());
    });

    findViewById(R.id.btnLoadArmLibrary).setOnClickListener(v -> {
      // test_module 是 arm 架构, fakelinker和fakelinker模块是x86架构
      System.loadLibrary("test_module");
      testArmSymbolForFakeinkerHook();
      beforeAddress = findModuleBeforeAddress();
      afterAddress = findModuleAfterAddress();
      testBeforeHook();
      System.loadLibrary("test_module_hook");
    });

    findViewById(R.id.btnTestBeforeHook).setOnClickListener(v -> {
      beforeHookSymbol(beforeAddress);
      testBeforeHook();
    });

    findViewById(R.id.btnTestAfterHook).setOnClickListener(v -> {
      afterHookSymbol(afterAddress);
      testAfterHook();
    });
  }

  private native long findModuleBeforeAddress();

  private native long findModuleAfterAddress();

  private native void relocateLibrary(String library);

  private native void beforeHookSymbol(long addr);

  private native void testBeforeHook();

  private native void afterHookSymbol(long addr);

  private native void testAfterHook();

  private native void testArmSymbolForFakeinkerHook();
}