#pragma once

#include <errno.h>
#include <unistd.h>

#include <string>

template <typename Closer>
class unique_fd_impl final {
public:
  unique_fd_impl() {}

  explicit unique_fd_impl(int fd) { reset(fd); }

  ~unique_fd_impl() { reset(); }

  unique_fd_impl(const unique_fd_impl &) = delete;
  void operator=(const unique_fd_impl &) = delete;

  unique_fd_impl(unique_fd_impl &&other) noexcept { reset(other.release()); }

  unique_fd_impl &operator=(unique_fd_impl &&s) noexcept {
    int fd = s.fd_;
    s.fd_ = -1;
    reset(fd, &s);
    return *this;
  }

  [[clang::reinitializes]] void reset(int new_value = -1) { reset(new_value, nullptr); }

  int get() const { return fd_; }

  bool operator>=(int rhs) const { return get() >= rhs; }

  bool operator<(int rhs) const { return get() < rhs; }

  bool operator==(int rhs) const { return get() == rhs; }

  bool operator!=(int rhs) const { return get() != rhs; }

  bool operator==(const unique_fd_impl &rhs) const { return get() == rhs.get(); }

  bool operator!=(const unique_fd_impl &rhs) const { return get() != rhs.get(); }

  // Catch bogus error checks (i.e.: "!fd" instead of "fd != -1").
  bool operator!() const = delete;

  bool ok() const { return get() >= 0; }

  int release() __attribute__((warn_unused_result)) {
    tag(fd_, this, nullptr);
    int ret = fd_;
    fd_ = -1;
    return ret;
  }

private:
  void reset(int new_value, void *previous_tag) {
    int previous_errno = errno;

    if (fd_ != -1) {
      close(fd_, this);
    }

    fd_ = new_value;
    if (new_value != -1) {
      tag(new_value, previous_tag, this);
    }

    errno = previous_errno;
  }

  int fd_ = -1;

  // Template magic to use Closer::Tag if available, and do nothing if not.
  // If Closer::Tag exists, this implementation is preferred, because int is a
  // better match. If not, this implementation is SFINAEd away, and the no-op
  // below is the only one that exists.
  template <typename T = Closer>
  static auto tag(int fd, void *old_tag, void *new_tag) -> decltype(T::Tag(fd, old_tag, new_tag), void()) {
    T::Tag(fd, old_tag, new_tag);
  }

  template <typename T = Closer>
  static void tag(long, void *, void *) {
    // No-op.
  }

  // Same as above, to select between Closer::Close(int) and Closer::Close(int,
  // void*).
  template <typename T = Closer>
  static auto close(int fd, void *tag_value) -> decltype(T::Close(fd, tag_value), void()) {
    T::Close(fd, tag_value);
  }

  template <typename T = Closer>
  static auto close(int fd, void *) -> decltype(T::Close(fd), void()) {
    T::Close(fd);
  }
};

// The actual details of closing are factored out to support unusual cases.
// Almost everyone will want this DefaultCloser, which handles fdsan on bionic.
struct DefaultCloser {
  static void Close(int fd) {
    // Even if close(2) fails with EINTR, the fd will have been closed.
    // Using TEMP_FAILURE_RETRY will either lead to EBADF or closing someone
    // else's fd.
    // http://lkml.indiana.edu/hypermail/linux/kernel/0509.1/0877.html
    close(fd);
  }
};

// A wrapper type that can be implicitly constructed from either int or
// unique_fd. This supports cases where you don't actually own the file
// descriptor, and can't take ownership, but are temporarily acting as if
// you're the owner.
//
// One example would be a function that needs to also allow
// STDERR_FILENO, not just a newly-opened fd. Another example would be JNI code
// that's using a file descriptor that's actually owned by a
// ParcelFileDescriptor or whatever on the Java side, but where the JNI code
// would like to enforce this weaker sense of "temporary ownership".
//
// If you think of unique_fd as being like std::string in that represents
// ownership, borrowed_fd is like std::string_view (and int is like const
// char*).
struct borrowed_fd {
  /* implicit */ borrowed_fd(int fd) : fd_(fd) {} // NOLINT

  template <typename T>
  /* implicit */ borrowed_fd(const unique_fd_impl<T> &ufd) : fd_(ufd.get()) {} // NOLINT

  int get() const { return fd_; }

  bool operator>=(int rhs) const { return get() >= rhs; }

  bool operator<(int rhs) const { return get() < rhs; }

  bool operator==(int rhs) const { return get() == rhs; }

  bool operator!=(int rhs) const { return get() != rhs; }

private:
  int fd_ = -1;
};

using unique_fd = unique_fd_impl<DefaultCloser>;

bool ReadFileToString(const std::string &path, std::string *content, bool follow_symlinks = false);

bool ReadFdToString(borrowed_fd fd, std::string *content);