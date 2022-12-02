//
// Created by beich on 2020/11/6.
//
#pragma once
#include <android/log.h>
#ifdef STD_LOG
#include <stdio.h>
#endif

extern int g_log_level;

#ifndef _PRINT

#ifdef STD_LOG
#define _STD_PRINT(format, ...)                                                                                        \
  do {                                                                                                                 \
    printf(format "\n", ##__VA_ARGS__);                                                                                \
  } while (0)
#else
#define _STD_PRINT(format, ...)
#endif

#define LOG_TAG "FakeLinker"
#define _PRINT(v, format, ...)                                                                                         \
  do {                                                                                                                 \
    if (g_log_level <= (v)) {                                                                                          \
      __android_log_print(v, LOG_TAG, format, ##__VA_ARGS__);                                                          \
      _STD_PRINT(format, ##__VA_ARGS__);                                                                               \
    }                                                                                                                  \
  } while (0)


#ifdef NDEBUG
#define LOGV(format, ...)
#define LOGD(format, ...)
#define LOGI(format, ...)
#else
#define LOGV(format, ...) _PRINT(ANDROID_LOG_VERBOSE, format, ##__VA_ARGS__)
#define LOGD(format, ...) _PRINT(ANDROID_LOG_DEBUG, format, ##__VA_ARGS__)
#define LOGI(format, ...) _PRINT(ANDROID_LOG_INFO, format, ##__VA_ARGS__)
#endif
#define LOGW(format, ...) _PRINT(ANDROID_LOG_WARN, format, ##__VA_ARGS__)
#define LOGE(format, ...) _PRINT(ANDROID_LOG_ERROR, format, ##__VA_ARGS__)

#endif
