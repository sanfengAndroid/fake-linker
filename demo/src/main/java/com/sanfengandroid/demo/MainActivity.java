/*
 * Copyright (c) 2025 WebViewDebugHook by sanfengAndroid.
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

package com.sanfengandroid.demo;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;
import android.widget.Toast;
import com.sanfengandroid.fakelinker.FakeLinker;
import java.io.File;

public class MainActivity extends Activity {
  private static final String TAG = "FakeLinkerTest";
  private TextView loadStaticResult;
  private TextView loadSharedResult;
  private TextView hookTestResult;
  private TextView hookNativeTestResult;

  @SuppressLint("MissingInflatedId")
  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    loadStaticResult = findViewById(R.id.load_static_result);
    loadSharedResult = findViewById(R.id.load_shared_result);
    hookTestResult = findViewById(R.id.hook_test_result);
    hookNativeTestResult = findViewById(R.id.hook_native_test_result);
    findViewById(R.id.load_static).setOnClickListener(v -> {
      if (isInit(loadSharedResult)) {
        Toast
            .makeText(this,
                      "currently the shared demo has been tested, please "
                          + "restart the app and try again",
                      Toast.LENGTH_LONG)
            .show();
        return;
      }
      try {
        System.loadLibrary("static_demo");
        loadStaticResult.setText("pass");
      } catch (Throwable e) {
        Log.e(TAG, "load static demo failed", e);
        loadStaticResult.setText("failed");
      }
    });
    findViewById(R.id.load_shared).setOnClickListener(v -> {
      if (isInit(loadStaticResult)) {
        Toast
            .makeText(this,
                      "currently the static demo has been tested, please "
                          + "restart the app and try again",
                      Toast.LENGTH_LONG)
            .show();
        return;
      }
      try {
        FakeLinker.initFakeLinker(null, "libshared_demo.so");
        loadSharedResult.setText("pass");
      } catch (Throwable e) {
        Log.e(TAG, "load shared demo failed", e);
        loadSharedResult.setText("failed");
      }
    });

    findViewById(R.id.hook_native_test).setOnClickListener(v -> {
      if (!isInit(loadStaticResult) && !isInit(loadSharedResult)) {
        Toast
            .makeText(this, "Please click Load Static/Shared Demo first",
                      Toast.LENGTH_LONG)
            .show();
        return;
      }
      try {
        System.loadLibrary("native_test");
        hookNativeTestResult.setText("pass");
      } catch (Throwable e) {
        Log.e(TAG, "test fakelinker native hook failed");
        hookNativeTestResult.setText("failed");
      }
    });

    findViewById(R.id.hook_test).setOnClickListener(v -> {
      if (!isInit(loadStaticResult) && !isInit(loadSharedResult)) {
        Toast
            .makeText(this, "Please click Load Static/Shared Demo first",
                      Toast.LENGTH_LONG)
            .show();
        return;
      }
      hookTestResult.setText(this.test() ? "pass" : "failed");
    });
  }

  boolean test() {
    File file = new File("/test/hook/path");
    return file.exists();
  }

  private boolean isInit(TextView tv) {
    if ("pass".equals(tv.getText().toString())) {
      return true;
    }
    return "failed".equals(tv.getText().toString());
  }
}
