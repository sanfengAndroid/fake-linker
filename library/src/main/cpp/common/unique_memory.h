#pragma once

#include <stdint.h>

class unique_memory {
public:
  unique_memory() {}

  unique_memory(size_t size, bool check = false);

  unique_memory(void *ptr, size_t size = 0, bool isMmap = false) : size_(size), isMmap_(isMmap) { reset(ptr); }

  ~unique_memory() { reset(); }

  unique_memory(unique_memory &&other) noexcept { reset(other.release()); }

  unique_memory &operator=(unique_memory &&s) noexcept {
    void *v = s.ptr_;
    s.ptr_ = nullptr;
    size_ = s.size_;
    s.size_ = 0;
    reset(v);
    return *this;
  }

  bool ok() { return ptr_ != nullptr; }

  char *char_ptr() { return reinterpret_cast<char *>(ptr_); }

  template <typename T = void>
  T *get() {
    return reinterpret_cast<T *>(ptr_);
  }

  template <typename T = void>
  T *release() {
    T *v = reinterpret_cast<T *>(ptr_);
    ptr_ = nullptr;
    size_ = 0;
    return v;
  }

  void reset(void *ptr = nullptr, size_t size = 0, bool is_mmap = false);

  size_t size() { return size_; }

  void clean();

  template <typename T>
  void set(const size_t index, T value) {
    get<T>()[index] = value;
  }

private:
  void *ptr_ = nullptr;
  size_t size_ = 0;
  bool isMmap_ = false;
};