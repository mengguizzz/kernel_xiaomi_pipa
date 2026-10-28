#include <linux/cred.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/susfs.h>
#include <linux/uaccess.h>

#include "uapi/supercall.h"
#include "susfs_compat.h"

#define SukiSUSFS_MAGIC 0xFAFAFAFA
#define SukiCMD_ADD_PATH 0x55550
#define SukiCMD_ADD_PATH_LOOP 0x55553
#define SukiCMD_HIDE_MOUNTS 0x55561
#define SukiCMD_ADD_KSTAT 0x55570
#define SukiCMD_UPDATE_KSTAT 0x55571
#define SukiCMD_ADD_KSTAT_STATIC 0x55572
#define SukiCMD_SET_UNAME 0x55590
#define SukiCMD_ENABLE_LOG 0x555A0
#define SukiCMD_SET_CMDLINE 0x555B0
#define SukiCMD_OPEN_REDIRECT 0x555C0
#define SukiCMD_SHOW_VERSION 0x555E1
#define SukiCMD_SHOW_FEATURES 0x555E2
#define SukiCMD_SHOW_VARIANT 0x555E3
#define SukiCMD_AVC_SPOOF 0x60010
#define SukiCMD_ADD_MAP 0x60020

struct suki_susfs_path {
	char path[256];
	int err;
};

struct suki_susfs_open_redirect {
	char target_pathname[256];
	char redirected_pathname[256];
	u32 uid_scheme;
	int err;
};

struct suki_susfs_kstat {
	u32 is_statically;
	u64 target_ino;
	char target_pathname[256];
	u64 spoofed_ino;
	u64 spoofed_dev;
	u32 spoofed_nlink;
	u64 spoofed_size;
	s64 spoofed_atime_tv_sec;
	u64 spoofed_atime_tv_nsec;
	s64 spoofed_mtime_tv_sec;
	u64 spoofed_mtime_tv_nsec;
	s64 spoofed_ctime_tv_sec;
	u64 spoofed_ctime_tv_nsec;
	u64 spoofed_blocks;
	s64 spoofed_blksize;
	u32 flags;
	int err;
};

static void susfs_call_v2(void (*fn)(void __user **), void __user *arg)
{
	void __user *ptr = arg;

	fn(&ptr);
}

static void susfs_call_v2_local(void (*fn)(void __user **), void *arg)
{
	mm_segment_t old_fs = get_fs();
	void __user *ptr = (void __user *)arg;

	set_fs(KERNEL_DS);
	fn(&ptr);
	set_fs(old_fs);
}

static void susfs_add_path_compat(void __user *arg, bool loop)
{
	struct suki_susfs_path in;
	struct st_susfs_sus_path old = { 0 };
	struct path path;

	if (copy_from_user(&in, arg, sizeof(in)))
		return;
	in.path[sizeof(in.path) - 1] = '\0';
	strlcpy(old.target_pathname, in.path, sizeof(old.target_pathname));
	in.err = kern_path(old.target_pathname, LOOKUP_FOLLOW, &path);
	if (!in.err) {
		old.target_ino = d_inode(path.dentry)->i_ino;
		old.i_uid = current_uid().val;
		path_put(&path);
		susfs_call_v2_local(loop ? susfs_add_sus_path_loop :
					   susfs_add_sus_path, &old);
		in.err = old.err;
	}
	copy_to_user(arg, &in, sizeof(in));
}

static void susfs_kstat_compat(void __user *arg, bool update)
{
	struct suki_susfs_kstat in;
	struct st_susfs_sus_kstat old = { 0 };

	if (copy_from_user(&in, arg, sizeof(in)))
		return;
	old.is_statically = in.is_statically;
	old.target_ino = (unsigned long)in.target_ino;
	strlcpy(old.target_pathname, in.target_pathname,
		sizeof(old.target_pathname));
	old.spoofed_ino = (unsigned long)in.spoofed_ino;
	old.spoofed_dev = (unsigned long)in.spoofed_dev;
	old.spoofed_nlink = in.spoofed_nlink;
	old.spoofed_size = in.spoofed_size;
	old.spoofed_atime_tv_sec = (long)in.spoofed_atime_tv_sec;
	old.spoofed_atime_tv_nsec = (long)in.spoofed_atime_tv_nsec;
	old.spoofed_mtime_tv_sec = (long)in.spoofed_mtime_tv_sec;
	old.spoofed_mtime_tv_nsec = (long)in.spoofed_mtime_tv_nsec;
	old.spoofed_ctime_tv_sec = (long)in.spoofed_ctime_tv_sec;
	old.spoofed_ctime_tv_nsec = (long)in.spoofed_ctime_tv_nsec;
	old.spoofed_blocks = in.spoofed_blocks;
	old.spoofed_blksize = (unsigned long)in.spoofed_blksize;
	susfs_call_v2_local(update ? susfs_update_sus_kstat :
				    susfs_add_sus_kstat, &old);
	in.err = old.err;
	copy_to_user(arg, &in, sizeof(in));
}

static void susfs_open_redirect_compat(void __user *arg)
{
	struct suki_susfs_open_redirect in;
	struct st_susfs_open_redirect old = { 0 };
	struct path path;

	if (copy_from_user(&in, arg, sizeof(in)))
		return;
	in.target_pathname[sizeof(in.target_pathname) - 1] = '\0';
	in.redirected_pathname[sizeof(in.redirected_pathname) - 1] = '\0';
	in.err = kern_path(in.target_pathname, LOOKUP_FOLLOW, &path);
	if (!in.err) {
		old.target_ino = d_inode(path.dentry)->i_ino;
		path_put(&path);
		strlcpy(old.target_pathname, in.target_pathname,
			sizeof(old.target_pathname));
		strlcpy(old.redirected_pathname, in.redirected_pathname,
			sizeof(old.redirected_pathname));
		susfs_call_v2_local(susfs_add_open_redirect, &old);
		in.err = old.err;
	}
	copy_to_user(arg, &in, sizeof(in));
}

static void susfs_handle_reboot(unsigned int cmd, void __user *arg)
{
	if (current_uid().val != 0 || !arg)
		return;

	switch (cmd) {
	case SukiCMD_ADD_PATH:
		susfs_add_path_compat(arg, false);
		break;
	case SukiCMD_ADD_PATH_LOOP:
		susfs_add_path_compat(arg, true);
		break;
	case SukiCMD_HIDE_MOUNTS:
		susfs_call_v2(susfs_set_hide_sus_mnts_for_all_procs, arg);
		break;
	case SukiCMD_ADD_KSTAT:
	case SukiCMD_ADD_KSTAT_STATIC:
		susfs_kstat_compat(arg, false);
		break;
	case SukiCMD_UPDATE_KSTAT:
		susfs_kstat_compat(arg, true);
		break;
	case SukiCMD_SET_UNAME:
		susfs_call_v2(susfs_set_uname, arg);
		break;
	case SukiCMD_ENABLE_LOG:
		susfs_call_v2(susfs_enable_log, arg);
		break;
	case SukiCMD_SET_CMDLINE:
		susfs_call_v2(susfs_set_cmdline_or_bootconfig, arg);
		break;
	case SukiCMD_OPEN_REDIRECT:
		susfs_open_redirect_compat(arg);
		break;
	case SukiCMD_SHOW_VERSION:
		susfs_call_v2(susfs_show_version, arg);
		break;
	case SukiCMD_SHOW_FEATURES:
		susfs_call_v2(susfs_get_enabled_features, arg);
		break;
	case SukiCMD_SHOW_VARIANT:
		susfs_call_v2(susfs_show_variant, arg);
		break;
	case SukiCMD_AVC_SPOOF:
		susfs_call_v2(susfs_set_avc_log_spoofing, arg);
		break;
	case SukiCMD_ADD_MAP:
		susfs_call_v2(susfs_add_sus_map, arg);
		break;
	default:
		break;
	}
}

bool ksu_susfs_reboot_handler(int magic1, int magic2, unsigned int cmd,
			      void __user *arg)
{
	if (magic1 != KSU_INSTALL_MAGIC1 || magic2 != SukiSUSFS_MAGIC)
		return false;

	susfs_handle_reboot(cmd, arg);
	return true;
}
