//
// Created by beich on 2020/11/8.
//
#include "hook_installer.h"

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <jni.h>
#include <libgen.h>
#include <sys/stat.h>
#include <syscall.h>
#include <unistd.h>

#include <alog.h>
#include <linker_macros.h>
#include <macros.h>

static uid_t set_uid = 1000;
static gid_t set_gid = 1000;
const char *lib_xattr = "u:object_r:system_file:s0";
const char *file_xattr = "u:object_r:system_file:s0";

int g_log_level = 5;

int SetFileXattr(const char *path, const char *value) {
  if (access("/sys/fs/selinux", F_OK)) {
    return 0;
  }
  return syscall(__NR_setxattr, path, "security.selinux", value, strlen(value), 0);
}

static bool CreateDir(const char *dir) {
  char *parent = dirname(dir);
  char *copy = strdup(parent);
  if (copy != nullptr && access(copy, F_OK) != 0) {
    bool create = CreateDir(copy);
    if (!create) {
      return false;
    }
  }
  if (opendir(dir) == nullptr) {
    int ret = mkdir(dir, 00777);
    if (ret == -1) {
      LOGE("native create dir error: %s", dir);
      return false;
    }
    chown(dir, set_uid, set_gid);
  }
  return true;
}

static bool CopyFile(const char *dst, const char *src) {
  FILE *f1, *f2;
  int c;

  f1 = fopen(src, "rb");
  f2 = fopen(dst, "wb");
  if (f1 == nullptr || f2 == nullptr) {
    LOGE("read/write file error,src: %s, dst: %s", src, dst);
    return false;
  }
  while ((c = fgetc(f1)) != EOF) {
    fputc(c, f2);
  }
  fclose(f1);
  fclose(f2);
  return true;
}

int RemoveDir(const char *path) {
  DIR *dir;
  struct dirent *dp;
  char tmp_path[1024];
  int result = 0;

  dir = opendir(path);
  if (dir == nullptr) {
    return errno;
  }
  while ((dp = readdir(dir)) != nullptr) {
    if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
      strcpy(tmp_path, path);
      strcat(tmp_path, "/");
      strcat(tmp_path, dp->d_name);
      if (dp->d_type == DT_REG) {
        if (remove(tmp_path)) {
          result = errno;
          goto end;
        }
      } else if (dp->d_type == DT_DIR) {
        result = RemoveDir(tmp_path);
        if (result != 0) {
          goto end;
        }
      }
    }
  }
end:
  closedir(dir);
  if (result == 0) {
    remove(path);
  }
  return result;
}

static int Uninstall(int len, char **argv) {
  int result = 0;
  struct stat buf;

  for (int i = 0; i < len; ++i) {
    LOGD("uninstall file: %s", argv[i]);
    if (access(argv[i], F_OK)) {
      continue;
    }
    result = stat(argv[i], &buf);
    if (result != 0) {
      return errno;
    }
    if (S_ISDIR(buf.st_mode)) {
      result = RemoveDir(argv[i]);
      if (result != 0) {
        return result;
      }
    } else if (S_ISREG(buf.st_mode)) {
      result = remove(argv[i]);
      if (result != 0) {
        return result;
      }
    }
  }
  return result;
}

C_API API_PUBLIC int main(int argc, char **argv) {
  int index = 1;
  if (!strcmp(argv[index], "remove")) {
    return Uninstall(argc - 2, &argv[++index]);
  }
  if (strcmp(argv[index], "copy") != 0) {
    LOGE("error operation type: %s", argv[index]);
    printf("exit code -1");
    return -1;
  }
  set_uid = strtol(argv[++index], nullptr, 0);
  set_gid = strtol(argv[++index], nullptr, 0);
  bool lib = strcmp(argv[++index], "lib") == 0;
  if (lib) {
    lib_xattr = argv[++index];
  } else {
    file_xattr = argv[++index];
  }
  if (!CreateDir(argv[++index])) {
    printf("exit code -1");
    return -1;
  }
  char path[PATH_MAX];
  for (int i = ++index; i < argc; ++i) {
    memset(path, 0, PATH_MAX);
    if (!CreateDir(argv[i])) {
      return -1;
    }
    const char *name = basename(argv[i + 1]);
    strcat(path, argv[i]);
    strcat(path, "/");
    strcat(path, name);
    if (access(argv[i + 1], R_OK)) {
      if (chmod(argv[i + 1], 00755)) {
        LOGE("set file chmod error: %s", argv[i + 1]);
        return -2;
      }
    }
    LOGD("copy file %s -> %s", argv[i + 1], path);
    if (!CopyFile(path, argv[++i])) {
      printf("exit code -2");
      return -2;
    }
    if (chmod(path, lib ? 00755 : 00644)) {
      LOGE("set file chmod error: %s", path);
      printf("exit code -3");
      return -3;
    }
    if (chown(path, set_uid, set_gid)) {
      LOGE("set file owner error: %s", path);
      printf("exit code -4");
      return -4;
    }
    if (SetFileXattr(path, lib ? lib_xattr : file_xattr) != 0) {
      LOGE("set file xattr failed, file: %s", path);
      printf("exit code -5");
      return -5;
    }
  }
  return 0;
}
