//
// Created by beich on 2020/11/8.
//
#pragma once

#include <link.h>

#include <string>

#include "linker_namespaces.h"
#include "linker_soinfo.h"

const char *hex_pointer(const void *ptr);
const char *hex_int(uint64_t val);

/*
 * linker的数据在写入后都被保护起来,关闭了写权限
 * */
std::string soinfo_to_string_debug(soinfo *si);
std::string soinfo_to_string(soinfo *si);
std::string android_namespace_to_string(android_namespace_t *an);
std::string android_namespace_link_t_to_string(android_namespace_link_t *link);
bool file_is_in_dir(const std::string &file, const std::string &dir);

bool file_is_under_dir(const std::string &file, const std::string &dir);

uint32_t calculate_elf_hash(const char *name);

uint32_t calculate_gnu_hash(const char *name);

bool safe_add(off64_t *out, off64_t a, size_t b);
size_t page_offset(off64_t offset);
off64_t page_start(off64_t offset);

void DL_WARN_documented_change(int api_level, const char *doc_fragment, const char *fmt, ...);

void add_dlwarning(const char *so_path, const char *message, const char *value = nullptr);

bool get_transparent_hugepages_supported();

bool is_first_stage_init();
