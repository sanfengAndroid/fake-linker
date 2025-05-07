#ifndef FAKE_LINKER_ANDROID_LEVEL_COMPAT_H
#define FAKE_LINKER_ANDROID_LEVEL_COMPAT_H

#include <android/api-level.h>

#ifndef __ANDROID_API_U__
/** Names the "U" API level (34), for comparison against `__ANDROID_API__`. */
#define __ANDROID_API_U__ 34
#endif

#ifndef __ANDROID_API_V__
/**
 * Names the Android 15 (aka "V" or "VanillaIceCream") API level (35),
 * for comparison against `__ANDROID_API__`.
 */
#define __ANDROID_API_V__ 35
#endif

#endif