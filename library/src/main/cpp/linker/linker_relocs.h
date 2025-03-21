/*
 * Copyright (C) 2015 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <elf.h>
#include <link.h>

#define R_GENERIC_NONE 0 // R_*_NONE is always 0

#if defined(__aarch64__)

#define R_GENERIC_JUMP_SLOT  R_AARCH64_JUMP_SLOT
// R_AARCH64_ABS64 is classified as a static relocation but it is common in
// DSOs.
#define R_GENERIC_ABSOLUTE   R_AARCH64_ABS64
#define R_GENERIC_GLOB_DAT   R_AARCH64_GLOB_DAT
#define R_GENERIC_RELATIVE   R_AARCH64_RELATIVE
#define R_GENERIC_IRELATIVE  R_AARCH64_IRELATIVE
#define R_GENERIC_COPY       R_AARCH64_COPY
#define R_GENERIC_TLS_DTPMOD R_AARCH64_TLS_DTPMOD
#define R_GENERIC_TLS_DTPREL R_AARCH64_TLS_DTPREL
#define R_GENERIC_TLS_TPREL  R_AARCH64_TLS_TPREL
#define R_GENERIC_TLSDESC    R_AARCH64_TLSDESC

#elif defined(__arm__)

#define R_GENERIC_JUMP_SLOT  R_ARM_JUMP_SLOT
// R_ARM_ABS32 is classified as a static relocation but it is common in DSOs.
#define R_GENERIC_ABSOLUTE   R_ARM_ABS32
#define R_GENERIC_GLOB_DAT   R_ARM_GLOB_DAT
#define R_GENERIC_RELATIVE   R_ARM_RELATIVE
#define R_GENERIC_IRELATIVE  R_ARM_IRELATIVE
#define R_GENERIC_COPY       R_ARM_COPY
#define R_GENERIC_TLS_DTPMOD R_ARM_TLS_DTPMOD32
#define R_GENERIC_TLS_DTPREL R_ARM_TLS_DTPOFF32
#define R_GENERIC_TLS_TPREL  R_ARM_TLS_TPOFF32
#define R_GENERIC_TLSDESC    R_ARM_TLS_DESC

#elif defined(__i386__)

#define R_GENERIC_JUMP_SLOT  R_386_JMP_SLOT
#define R_GENERIC_ABSOLUTE   R_386_32
#define R_GENERIC_GLOB_DAT   R_386_GLOB_DAT
#define R_GENERIC_RELATIVE   R_386_RELATIVE
#define R_GENERIC_IRELATIVE  R_386_IRELATIVE
#define R_GENERIC_COPY       R_386_COPY
#define R_GENERIC_TLS_DTPMOD R_386_TLS_DTPMOD32
#define R_GENERIC_TLS_DTPREL R_386_TLS_DTPOFF32
#define R_GENERIC_TLS_TPREL  R_386_TLS_TPOFF
#define R_GENERIC_TLSDESC    R_386_TLS_DESC

#elif defined(__x86_64__)

#define R_GENERIC_JUMP_SLOT  R_X86_64_JUMP_SLOT
#define R_GENERIC_ABSOLUTE   R_X86_64_64
#define R_GENERIC_GLOB_DAT   R_X86_64_GLOB_DAT
#define R_GENERIC_RELATIVE   R_X86_64_RELATIVE
#define R_GENERIC_IRELATIVE  R_X86_64_IRELATIVE
#define R_GENERIC_COPY       R_X86_64_COPY
#define R_GENERIC_TLS_DTPMOD R_X86_64_DTPMOD64
#define R_GENERIC_TLS_DTPREL R_X86_64_DTPOFF64
#define R_GENERIC_TLS_TPREL  R_X86_64_TPOFF64
#define R_GENERIC_TLSDESC    R_X86_64_TLSDESC

#endif

inline bool is_tls_reloc(ElfW(Word) type) {
  switch (type) {
  case R_GENERIC_TLS_DTPMOD:
  case R_GENERIC_TLS_DTPREL:
  case R_GENERIC_TLS_TPREL:
  case R_GENERIC_TLSDESC:
    return true;
  default:
    return false;
  }
}