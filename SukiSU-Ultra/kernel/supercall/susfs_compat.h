#ifndef __KSU_SUSFS_COMPAT_H
#define __KSU_SUSFS_COMPAT_H

#include <linux/types.h>

bool ksu_susfs_reboot_handler(int magic1, int magic2, unsigned int cmd, void __user *arg);

#endif
