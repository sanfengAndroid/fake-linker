#pragma once

#include <android/log.h>

#include "trace_jni.h"

namespace fakelinker {
class DefaultTraceJNICallback : public BaseTraceJNICallback<DefaultTraceJNICallback> {
public:
  explicit DefaultTraceJNICallback(bool strict) : BaseTraceJNICallback<DefaultTraceJNICallback>(strict) {}
};

} // namespace fakelinker