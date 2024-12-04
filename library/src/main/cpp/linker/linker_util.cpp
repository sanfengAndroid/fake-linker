//
// Created by beich on 2020/11/8.
//

#include "linker_util.h"

#include <libgen.h>

#include <cinttypes>

#include <alog.h>
#include <macros.h>
#include <maps_util.h>
#include <unique_fd.h>

#define MAYBE_MAP_FLAG(x, from, to) (((x) & (from)) ? (to) : 0)
#define PFLAGS_TO_PROT(x)                                                                                              \
  (MAYBE_MAP_FLAG((x), PF_X, PROT_EXEC) | MAYBE_MAP_FLAG((x), PF_R, PROT_READ) | MAYBE_MAP_FLAG((x), PF_W, PROT_WRITE))

#define MAX_OUT 0x4000

#define STRCAT_MES(msg)                                                                                                \
  do {                                                                                                                 \
    if (msg != nullptr)                                                                                                \
      strncat(out, msg, MAX_OUT);                                                                                      \
    else                                                                                                               \
      strncat(out, "null", MAX_OUT);                                                                                   \
  } while (0)

#define STRCAT_BOOLEAN(msg, cond)                                                                                      \
  STRCAT_MES(msg);                                                                                                     \
  strncat(out, cond ? "true" : "false", MAX_OUT)
#define STRCAT_STRING(msg, str)                                                                                        \
  STRCAT_MES(msg);                                                                                                     \
  STRCAT_MES(str);
#define STRCAT_HEX(msg, value)                                                                                         \
  do {                                                                                                                 \
    STRCAT_MES(msg);                                                                                                   \
    size_t len_ = strlen(out);                                                                                         \
    snprintf(&out[len_], (size_t)(MAX_OUT - len_), "0x%" SCNx64, (uint64_t)value);                                     \
  } while (0)

#define FALLBACK_CHAR(n)                                                                                               \
  do {                                                                                                                 \
    size_t len_ = strlen(out);                                                                                         \
    out[len_ - n] = '\0';                                                                                              \
  } while (0)

std::string soinfo_to_string_debug(soinfo *si) {
  if (si == nullptr) {
    return "soinfo is null.";
  }
  char out[MAX_OUT];
  out[0] = '\0';

  STRCAT_STRING("soinfo name: ", si->get_soname());
  STRCAT_HEX(", base address: ", si->base());
  STRCAT_HEX(", flags: ", si->flags());
  STRCAT_HEX(", load bias: ", si->load_bias());

  auto format = [&](soinfo_list_t_wrapper list, const char *name) {
    STRCAT_STRING(name, "[");
    list.for_each([&](soinfo *so) {
      STRCAT_STRING(so->get_soname(), ", ");
    });
    if (!list.empty()) {
      FALLBACK_CHAR(2);
    }
    STRCAT_MES("]");
  };

  STRCAT_MES(", ");
  format(si->get_children(), "children");
  STRCAT_MES(", ");
  format(si->get_parents(), "parents");
  if (android_api >= __ANDROID_API_L_MR1__) {
    STRCAT_HEX(", rtld_flags: ", si->get_rtld_flags());
  }
  if (android_api >= __ANDROID_API_M__) {
    STRCAT_HEX(", dt_flags_1: ", si->dt_flags_1());
    STRCAT_STRING(", realpath: ", si->realpath());
  }

  if (android_api >= __ANDROID_API_N__) {
    STRCAT_MES(", dt_runpath: [");
    for (auto &n : si->dt_runpath()) {
      STRCAT_STRING(n.c_str(), ", ");
    }
    if (!si->dt_runpath().empty()) {
      FALLBACK_CHAR(2);
    }

    STRCAT_STRING("], primary namespace: {", android_namespace_to_string(si->get_primary_namespace()).c_str());
    STRCAT_MES("}");
    STRCAT_MES(", secondary_namespaces: [");
    si->get_secondary_namespaces().for_each([&](android_namespace_t *np) {
      STRCAT_STRING(np->get_name(), ", ");
    });
    if (!si->get_secondary_namespaces().empty()) {
      FALLBACK_CHAR(2);
    }
    STRCAT_HEX("], handle: ", si->get_handle());
  }
  return out;
}

static char temp[16];

const char *hex_pointer(const void *ptr) {
  sprintf(temp, "%p", ptr);
  return temp;
}

const char *hex_int(uint64_t val) {
  sprintf(temp, "0x%" SCNx64, val);
  return temp;
}

#define COMMA ","

// 简单格式化关键成员
std::string soinfo_to_string(soinfo *so) {
  std::string out;
  out.reserve(1024);
  out.append("soinfo{");
  out.append("name:").append(so->get_soname()).append(COMMA);
  out.append("phdr:").append(hex_pointer(so->phdr())).append(COMMA);
  out.append("phnum:").append(std::to_string(so->phnum())).append(COMMA);
  out.append("base:").append(hex_int(so->base())).append(COMMA);
  out.append("size:").append(hex_int(so->size())).append(COMMA);
  out.append("dynamic:").append(hex_pointer(so->dynamic())).append(COMMA);
  out.append("flag:").append(hex_int(so->flags())).append(COMMA);
  out.append("strtab:").append(hex_pointer(so->strtab())).append(COMMA);
  out.append("symtab:").append(hex_pointer(so->symtab())).append(COMMA);
  out.append("nbucket:").append(std::to_string(so->nbucket())).append(COMMA);
  out.append("nchain:").append(std::to_string(so->nchain())).append(COMMA);
  if (android_api >= __ANDROID_API_M__) {
    out.append("bucket:").append(hex_pointer(so->bucket_M())).append(COMMA);
    out.append("chain:").append(hex_pointer(so->chain_M())).append(COMMA);
  } else {
    out.append("bucket:").append(hex_pointer(so->bucket())).append(COMMA);
    out.append("chain:").append(hex_pointer(so->chain())).append(COMMA);
  }
#ifndef __LP64__
  out.append("plt_got:").append(hex_pointer(so->plt_got())).append(COMMA);
#endif

  if (android_api >= __ANDROID_API_P__) {
    out.append("preinit_array:").append(hex_pointer(so->preinit_array_P())).append(COMMA);
    out.append("preinit_array_count:").append(std::to_string(so->preinit_array_count())).append(COMMA);
    out.append("init_array:").append(hex_pointer(so->init_array_P())).append(COMMA);
    out.append("init_array_count:").append(std::to_string(so->init_array_count())).append(COMMA);
    out.append("fini_array:").append(hex_pointer(so->fini_array_P())).append(COMMA);
    out.append("fini_array_count:").append(std::to_string(so->fini_array_count())).append(COMMA);
  } else {
    out.append("preinit_array:").append(hex_pointer(so->preinit_array())).append(COMMA);
    out.append("preinit_array_count:").append(std::to_string(so->preinit_array_count())).append(COMMA);
    out.append("init_array:").append(hex_pointer(so->init_array())).append(COMMA);
    out.append("init_array_count:").append(std::to_string(so->init_array_count())).append(COMMA);
    out.append("fini_array:").append(hex_pointer(so->fini_array())).append(COMMA);
    out.append("fini_array_count:").append(std::to_string(so->fini_array_count())).append(COMMA);
  }
  out.append("constructors_called:").append(std::to_string(so->constructors_called())).append(COMMA);
  out.append("load_bias:").append(hex_int(so->load_bias())).append(COMMA);
  out.append("has_DT_SYMBOLIC:").append(std::to_string(so->has_DT_SYMBOLIC())).append(COMMA);
  out.append("version:").append(std::to_string(so->version()));
  if (android_api >= __ANDROID_API_L_MR1__) {
    out.append("file_offset:").append(hex_int(so->file_offset())).append(COMMA);
    out.append("rtld_flags:").append(hex_int(so->get_rtld_flags())).append(COMMA);
    if (android_api >= __ANDROID_API_M__) {
      out.append("dt_flags_1:").append(hex_int(so->dt_flags_1())).append(COMMA);
    }
    out.append("strtab_size:").append(std::to_string(so->strtab_size())).append(COMMA);
  }
  out.append("}");
  return out;
}

std::string android_namespace_to_string(android_namespace_t *an) {
  if (an == nullptr) {
    return "android namespace is null";
  }
  char out[MAX_OUT];
  out[0] = '\0';

  STRCAT_HEX("address: ", an);
  STRCAT_STRING(", name: ", an->get_name());
  STRCAT_BOOLEAN(", is_isolated: ", an->is_isolated());
  if (android_api >= __ANDROID_API_O__) {
    STRCAT_BOOLEAN(", is_greylist_enabled: ", an->is_greylist_enabled());
  }
  if (android_api >= __ANDROID_API_R__) {
    STRCAT_BOOLEAN(", is_also_used_as_anonymous: ", an->is_also_used_as_anonymous());
  }
  STRCAT_MES(", ld_library_paths: [");
  for (auto &n : an->get_ld_library_paths()) {
    STRCAT_STRING(n.c_str(), ", ");
  }
  if (!an->get_ld_library_paths().empty()) {
    FALLBACK_CHAR(2);
  }
  STRCAT_MES("]");

  STRCAT_MES(", default_library_paths: [");
  for (auto &n : an->get_default_library_paths()) {
    STRCAT_STRING(n.c_str(), ", ");
  }
  if (!an->get_default_library_paths().empty()) {
    FALLBACK_CHAR(2);
  }
  STRCAT_MES("]");

  STRCAT_MES(", permitted_paths: [");
  for (auto &n : an->get_permitted_paths()) {
    STRCAT_STRING(n.c_str(), ", ");
  }
  if (!an->get_permitted_paths().empty()) {
    FALLBACK_CHAR(2);
  }
  STRCAT_MES("]");
  LOGD("length: %zu", strlen(out));
  if (android_api >= __ANDROID_API_P__) {
    STRCAT_MES(", linked_namespaces: [");
    for (auto &link : an->linked_namespaceP()) {
      STRCAT_STRING("{", android_namespace_link_t_to_string((android_namespace_link_t *)&link).c_str());
      STRCAT_MES("}, ");
    }
    if (!an->linked_namespaceP().empty()) {
      FALLBACK_CHAR(2);
    }
    STRCAT_MES("]");
  } else if (android_api >= __ANDROID_API_O__) {
    STRCAT_MES(", linked_namespaces: [");
    for (auto &link : an->linked_namespacesO()) {
      STRCAT_STRING("{", android_namespace_link_t_to_string((android_namespace_link_t *)&link).c_str());
      STRCAT_MES("}, ");
    }
    if (!an->linked_namespacesO().empty()) {
      FALLBACK_CHAR(2);
    }
    STRCAT_MES("]");
  }
  LOGD("length: %zu", strlen(out));
  STRCAT_MES(", solist: [");
  an->soinfo_list().for_each([&](soinfo *so) {
    STRCAT_STRING(so->get_soname(), ", ");
  });
  if (an->soinfo_list().empty() > 0) {
    FALLBACK_CHAR(2);
  }
  STRCAT_MES("]");
  return out;
}

std::string android_namespace_link_t_to_string(android_namespace_link_t *link) {
  if (link == nullptr) {
    return "android namespace link is null";
  }
  char out[MAX_OUT];
  out[0] = '\0';
  STRCAT_HEX("link namespace address: ", link->linked_namespace());
  STRCAT_STRING(", name: ", link->linked_namespace()->get_name());
  if (android_api > __ANDROID_API_P__) {
    android_namespace_link_t_P *lp = static_cast<android_namespace_link_t_P *>(link);
    STRCAT_BOOLEAN(", allow_all_shared_libs: ", lp->allow_all_shared_libs_);
  }
  STRCAT_MES(", shared_lib_sonames: [");
  for (auto &n : link->shared_lib_sonames()) {
    STRCAT_STRING(n.c_str(), ", ");
  }
  if (!link->shared_lib_sonames().empty()) {
    FALLBACK_CHAR(2);
  }
  STRCAT_MES("]");
  return out;
}

bool file_is_in_dir(const std::string &file, const std::string &dir) {
  const char *needle = dir.c_str();
  const char *haystack = file.c_str();
  size_t needle_len = strlen(needle);

  return strncmp(haystack, needle, needle_len) == 0 && haystack[needle_len] == '/' &&
    strchr(haystack + needle_len + 1, '/') == nullptr;
}

bool file_is_under_dir(const std::string &file, const std::string &dir) {
  const char *needle = dir.c_str();
  const char *haystack = file.c_str();
  size_t needle_len = strlen(needle);

  return strncmp(haystack, needle, needle_len) == 0 && haystack[needle_len] == '/';
}

uint32_t calculate_elf_hash(const char *name) {
  const uint8_t *name_bytes = (const uint8_t *)name;
  uint32_t h = 0, g;

  while (*name_bytes) {
    h = (h << 4) + *name_bytes++;
    g = h & 0xf0000000;
    h ^= g;
    h ^= g >> 24;
  }
  return h;
}

uint32_t calculate_gnu_hash(const char *name) {
  const uint8_t *name_bytes = (const uint8_t *)name;
  uint32_t h = 5381;

  while (*name_bytes != 0) {
    h += (h << 5) + *name_bytes++;
  }
  return h;
}

bool safe_add(off64_t *out, off64_t a, size_t b) {
  if (a < 0) {
    return false;
  }
  if (static_cast<uint64_t>(INT64_MAX - a) < b) {
    return false;
  }

  *out = a + b;
  return true;
}

size_t page_offset(off64_t offset) { return static_cast<size_t>(offset & (page_size() - 1)); }

off64_t page_start(off64_t offset) { return offset & ~static_cast<off64_t>(page_size() - 1); }

void DL_WARN_documented_change(int api_level, const char *doc_fragment, const char *fmt, ...) {
  LOGW("Deprecated api: %d, %s", api_level, doc_fragment);
  va_list ap;
  va_start(ap, fmt);
  __android_log_vprint(ANDROID_LOG_WARN, LOG_TAG, fmt, ap);
  va_end(ap);
}

static std::string current_msg;

void add_dlwarning(const char *so_path, const char *message, const char *value) {
  if (!current_msg.empty()) {
    current_msg += '\n';
  }

  current_msg = current_msg + basename(so_path) + ": " + message;

  if (value != nullptr) {
    current_msg = current_msg + " \"" + value + "\"";
  }
}

bool get_transparent_hugepages_supported() {
  static bool transparent_hugepages_supported = []() {
    std::string enabled;
    if (!ReadFileToString("/sys/kernel/mm/transparent_hugepage/enabled", &enabled)) {
      return false;
    }
    return enabled.find("[never]") == std::string::npos;
  }();
  return transparent_hugepages_supported;
}

bool is_first_stage_init() {
  static bool ret = (getpid() == 1 && access("/proc/self/exe", F_OK) == -1);
  return ret;
}
