//
// Created by beich on 2020/11/6.
//

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <sys/auxv.h>
#include <sys/user.h>

#include "alog.h"

typedef uint64_t Address;

extern int android_api;
extern bool init_success;

#define async_safe_fatal(...)                                                                                          \
  do {                                                                                                                 \
    LOGE(__VA_ARGS__);                                                                                                 \
    abort();                                                                                                           \
  } while (0)

#define CHECK(predicate)                                                                                               \
  do {                                                                                                                 \
    if (!(predicate)) {                                                                                                \
      async_safe_fatal("%s:%d: %s CHECK '%s' failed", __FILE__, __LINE__, __FUNCTION__, #predicate);                   \
    }                                                                                                                  \
  } while (0)

#ifndef NDEBUG
#define CHECK_TRUE(predicate) CHECK(predicate)
#else
#define CHECK_TRUE(predicate)                                                                                          \
  if (!(predicate))                                                                                                    \
  return false
#endif

#define CHECK_OUTPUT(predicate, ...)                                                                                   \
  do {                                                                                                                 \
    if (!(predicate)) {                                                                                                \
      LOGE(__VA_ARGS__);                                                                                               \
      async_safe_fatal("%s:%d: %s CHECK '" #predicate "' failed", __FILE__, __LINE__, __FUNCTION__);                   \
    }                                                                                                                  \
  } while (0)

inline size_t page_size() {
#ifdef PAGE_SIZE
  return PAGE_SIZE;
#else
  static const size_t page_size = getauxval(AT_PAGESZ);
  return page_size;
#endif
}

#ifndef PAGE_MASK
#define PAGE_MASK (~(page_size() - 1))
#endif

// Returns the address of the page containing address 'x'.
#define PAGE_START(x)  ((x) & PAGE_MASK)

// Returns the offset of address 'x' in its page.
#define PAGE_OFFSET(x) ((x) & ~PAGE_MASK)

// Returns the address of the next page after address 'x', unless 'x' is
// itself at the start of a page.
#define PAGE_END(x)    PAGE_START((x) + (page_size() - 1))

#define DISALLOW_COPY_AND_ASSIGN(TypeName)                                                                             \
  TypeName(const TypeName &) = delete;                                                                                 \
  void operator=(const TypeName &) = delete

#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName)                                                                       \
  TypeName() = delete;                                                                                                 \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

#define ROUND_UP_POWER_OF_2(value)                                                                                     \
  ((sizeof(value) == 8) ? (1UL << (64 - __builtin_clzl(static_cast<unsigned long>(value))))                            \
                        : (1UL << (32 - __builtin_clz(static_cast<unsigned int>(value)))))

#define ALIGN_CHECK(size, len) (((len) % (size)) == 0)

static constexpr uintptr_t align_down(uintptr_t p, size_t align) { return p & ~(align - 1); }

static constexpr uintptr_t align_up(uintptr_t p, size_t align) { return (p + align - 1) & ~(align - 1); }

static constexpr bool is64BitBuild() {
#ifdef __LP64__
  return true;
#else
  return false;
#endif
}

template <typename T>
inline T *align_down(T *p, size_t align) {
  return reinterpret_cast<T *>(align_down(reinterpret_cast<uintptr_t>(p), align));
}

template <typename T>
inline T *align_up(T *p, size_t align) {
  return reinterpret_cast<T *>(align_up(reinterpret_cast<uintptr_t>(p), align));
}

template <typename T>
inline T *untag_address(T *p) {
#if defined(__aarch64__)
  return reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(p) & ((1ULL << 56) - 1));
#else
  return p;
#endif
}

#define powerof2(x)                                                                                                    \
  ({                                                                                                                   \
    __typeof__(x) _x = (x);                                                                                            \
    __typeof__(x) _x2;                                                                                                 \
    __builtin_add_overflow(_x, -1, &_x2) ? 1 : ((_x2 & _x) == 0);                                                      \
  })