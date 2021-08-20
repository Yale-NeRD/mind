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
#ifndef _UAPI_LINUX_SHM_H_
#define _UAPI_LINUX_SHM_H_
#include <linux/ipc.h>
#include <linux/errno.h>
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#include <unistd.h>
#define SHMMAX 0x2000000
#define SHMMIN 1
#define SHMMNI 4096
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SHMALL (SHMMAX/getpagesize()*(SHMMNI/16))
#define SHMSEG SHMMNI

typedef unsigned long int shmatt_t;

struct shmid_ds {
 struct ipc_perm shm_perm;
 size_t shm_segsz;
 __time_t shm_atime;
#if __WORDSIZE == 32
 unsigned long int __unused1;
#endif
 __time_t shm_dtime;
#if __WORDSIZE == 32
 unsigned long int __unused2;
#endif
 __time_t shm_ctime;
#if __WORDSIZE == 32
 unsigned long int __unused3;
#endif
 int shm_cpid;
 int shm_lpid;
 shmatt_t shm_nattch;
 unsigned long int __unused4;
 unsigned long int __unused5;
};
#include <asm/shmbuf.h>
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SHM_R 0400
#define SHM_W 0200
#define SHM_RDONLY 010000
#define SHM_RND 020000
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SHM_REMAP 040000
#define SHM_EXEC 0100000
#define SHM_LOCK 11
#define SHM_UNLOCK 12
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SHM_STAT 13
#define SHM_INFO 14

struct  shminfo
{
  unsigned long int shmmax;
  unsigned long int shmmin;
  unsigned long int shmmni;
  unsigned long int shmseg;
  unsigned long int shmall;
  unsigned long int __unused1;
  unsigned long int __unused2;
  unsigned long int __unused3;
  unsigned long int __unused4;
};

struct shm_info
{
  int used_ids;
  unsigned long int shm_tot;  /* total allocated shm */
  unsigned long int shm_rss;  /* total resident shm */
  unsigned long int shm_swp;  /* total swapped shm */
  unsigned long int swap_attempts;
  unsigned long int swap_successes;
};

__BEGIN_DECLS

extern int shmctl(int shmid, int cmd, struct shmid_ds *buf);
extern int shmget(key_t key, size_t size, int shmflg);
extern void *shmat(int shmid, const void *shmaddr, int shmflg);
extern int shmdt(const void *shmaddr);

__END_DECLS

#endif
