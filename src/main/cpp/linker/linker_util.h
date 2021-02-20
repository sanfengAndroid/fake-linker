//
// Created by beich on 2020/11/8.
//
#pragma once

#include "linker_soinfo.h"
#include <string>

/*
 * linker的数据在写入后都被保护起来,关闭了写权限
 * */

std::string soinfo_to_string(soinfo *si);

bool file_is_in_dir(const std::string &file, const std::string &dir);

bool file_is_under_dir(const std::string &file, const std::string &dir);

uint32_t calculate_elf_hash(const char *name);

uint32_t calculate_gnu_hash(const char *name);

int phdr_table_protect_segments(const ElfW(Phdr) *phdr_table, size_t phdr_count, ElfW(Addr) load_bias);

int phdr_table_unprotect_segments(const ElfW(Phdr) *phdr_table, size_t phdr_count, ElfW(Addr) load_bias);