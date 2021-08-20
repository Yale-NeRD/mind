/*
 * Copyright 2002-2020 Intel Corporation.
 * 
 * This software and the related documents are Intel copyrighted materials, and your
 * use of them is governed by the express license under which they were provided to
 * you ("License"). Unless the License provides otherwise, you may not use, modify,
 * copy, publish, distribute, disclose or transmit this software or the related
 * documents without Intel's prior written permission.
 * 
 * This software and the related documents are provided as is, with no express or
 * implied warranties, other than those that are expressly stated in the License.
 * 
 * This file incorporates work covered by the following copyright and permission notice:
 */

/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef __ASM_GENERIC_MMAN_COMMON_H
#define __ASM_GENERIC_MMAN_COMMON_H
#define PROT_READ 0x1
#define PROT_WRITE 0x2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PROT_EXEC 0x4
#define PROT_SEM 0x8
#define PROT_NONE 0x0
#define PROT_GROWSDOWN 0x01000000
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PROT_GROWSUP 0x02000000
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_TYPE 0x0f
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_UNINITIALIZED 0x0
#define MS_ASYNC 1
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MS_INVALIDATE 2
#define MS_SYNC 4
#define MADV_NORMAL 0
#define MADV_RANDOM 1
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MADV_SEQUENTIAL 2
#define MADV_WILLNEED 3
#define MADV_DONTNEED 4
#define MADV_REMOVE 9
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MADV_DONTFORK 10
#define MADV_DOFORK 11
#define MADV_HWPOISON 100
#define MADV_SOFT_OFFLINE 101
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MADV_MERGEABLE 12
#define MADV_UNMERGEABLE 13
#define MADV_HUGEPAGE 14
#define MADV_NOHUGEPAGE 15
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MADV_DONTDUMP 16
#define MADV_DODUMP 17
#define MAP_FILE 0
#define MAP_HUGE_SHIFT 26
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MAP_HUGE_MASK 0x3f
#endif
