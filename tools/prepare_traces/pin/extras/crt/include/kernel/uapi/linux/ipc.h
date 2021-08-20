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
#ifndef _UAPI_LINUX_IPC_H
#define _UAPI_LINUX_IPC_H
#include <linux/types.h>
#define IPC_PRIVATE ((__kernel_key_t) 0)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct ipc_perm
{
 int key;
 unsigned int uid;
 unsigned int gid;
 unsigned int cuid;
 unsigned int cgid;
 unsigned short int mode;
 unsigned short int __pad1;
 unsigned short int seq;
 unsigned short int __pad2;
 unsigned long int __unused1;
 unsigned long int __unused2;
};
#include <asm/ipcbuf.h>
#define IPC_CREAT 00001000
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define IPC_EXCL 00002000
#define IPC_NOWAIT 00004000
#define IPC_DIPC 00010000
#define IPC_OWN 00020000
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define IPC_RMID 0
#define IPC_SET 1
#define IPC_STAT 2
#define IPC_INFO 3
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define IPC_OLD 0
#define IPC_64 0x0100
struct ipc_kludge {
 struct msgbuf __user *msgp;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 long msgtyp;
};
#define SEMOP 1
#define SEMGET 2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SEMCTL 3
#define SEMTIMEDOP 4
#define MSGSND 11
#define MSGRCV 12
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MSGGET 13
#define MSGCTL 14
#define SHMAT 21
#define SHMDT 22
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SHMGET 23
#define SHMCTL 24
#define DIPC 25
#define IPCCALL(version,op) ((version)<<16 | (op))
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
