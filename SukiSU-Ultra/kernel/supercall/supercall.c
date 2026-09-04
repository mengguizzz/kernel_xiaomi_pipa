#include <linux/anon_inodes.h>
#include <linux/err.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kprobes.h>
#include <linux/pid.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/task_work.h>
#ifndef TWA_RESUME
#define TWA_RESUME true
#endif
#include <linux/uaccess.h>
#include <linux/version.h>
#ifdef CONFIG_KSU_SUSFS
#include <linux/namei.h>
#include <linux/susfs.h>
#endif

#include "uapi/supercall.h"
#include "kpm/kpm.h"
#include "supercall/internal.h"
#include "arch.h"
#include "util.h"
#include "klog.h" // IWYU pragma: keep

struct ksu_install_fd_tw {
    struct callback_head cb;
    int __user *outp;
};

#ifdef CONFIG_KSU_SUSFS
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

struct suki_susfs_path { char path[256]; int err; };
struct suki_susfs_uname { char release[65]; char version[65]; int err; };
struct suki_susfs_log { u32 enabled; int err; };
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
struct suki_susfs_version { char value[16]; int err; };
struct suki_susfs_features { char value[8192]; int err; };

static int susfs_call_kernel_ptr(int (*fn)(void __user *), void *ptr)
{
    mm_segment_t old_fs = get_fs();
    int ret;

    set_fs(KERNEL_DS);
    ret = fn((void __user *)ptr);
    set_fs(old_fs);
    return ret;
}

static int susfs_add_path_compat(void __user *arg)
{
    struct suki_susfs_path in;
    struct st_susfs_sus_path old = { 0 };
    struct path path;

    if (copy_from_user(&in, arg, sizeof(in)))
        return -EFAULT;
    strlcpy(old.target_pathname, in.path, sizeof(old.target_pathname));
    in.err = kern_path(old.target_pathname, LOOKUP_FOLLOW, &path);
    if (!in.err) {
        old.target_ino = d_inode(path.dentry)->i_ino;
        path_put(&path);
        in.err = susfs_call_kernel_ptr((int (*)(void __user *))susfs_add_sus_path, &old);
    }
    return copy_to_user(arg, &in, sizeof(in)) ? -EFAULT : 0;
}

static int susfs_set_uname_compat(void __user *arg)
{
    struct suki_susfs_uname in;
    struct st_susfs_uname old = { 0 };

    if (copy_from_user(&in, arg, sizeof(in)))
        return -EFAULT;
    strlcpy(old.sysname, "default", sizeof(old.sysname));
    strlcpy(old.nodename, "default", sizeof(old.nodename));
    strlcpy(old.release, in.release, sizeof(old.release));
    strlcpy(old.version, in.version, sizeof(old.version));
    strlcpy(old.machine, "default", sizeof(old.machine));
    in.err = susfs_call_kernel_ptr((int (*)(void __user *))susfs_set_uname, &old);
    return copy_to_user(arg, &in, sizeof(in)) ? -EFAULT : 0;
}

static int susfs_add_mount_compat(void __user *arg)
{
    struct suki_susfs_path in;
    struct st_susfs_sus_mount old = { 0 };

    if (copy_from_user(&in, arg, sizeof(in)))
        return -EFAULT;
    strlcpy(old.target_pathname, in.path, sizeof(old.target_pathname));
    in.err = susfs_call_kernel_ptr((int (*)(void __user *))susfs_add_sus_mount,
                                   &old);
    return copy_to_user(arg, &in, sizeof(in)) ? -EFAULT : 0;
}

static int susfs_kstat_compat(void __user *arg, bool update)
{
    struct suki_susfs_kstat in;
    struct st_susfs_sus_kstat old = { 0 };

    if (copy_from_user(&in, arg, sizeof(in)))
        return -EFAULT;
    old.target_ino = (unsigned long)in.target_ino;
    strlcpy(old.target_pathname, in.target_pathname,
            sizeof(old.target_pathname));
    old.spoofed_ino = (unsigned long)in.spoofed_ino;
    old.spoofed_dev = (unsigned long)in.spoofed_dev;
    old.spoofed_nlink = in.spoofed_nlink;
    old.spoofed_atime_tv_sec = (long)in.spoofed_atime_tv_sec;
    old.spoofed_atime_tv_nsec = (long)in.spoofed_atime_tv_nsec;
    old.spoofed_mtime_tv_sec = (long)in.spoofed_mtime_tv_sec;
    old.spoofed_mtime_tv_nsec = (long)in.spoofed_mtime_tv_nsec;
    old.spoofed_ctime_tv_sec = (long)in.spoofed_ctime_tv_sec;
    old.spoofed_ctime_tv_nsec = (long)in.spoofed_ctime_tv_nsec;
    in.err = susfs_call_kernel_ptr(
        (int (*)(void __user *))(update ? susfs_update_sus_kstat :
                                         susfs_add_sus_kstat),
        &old);
    return copy_to_user(arg, &in, sizeof(in)) ? -EFAULT : 0;
}

static void susfs_write_status(void __user *arg, const char *value, size_t size)
{
    char *out = kzalloc(size, GFP_ATOMIC);

    if (!out)
        return;
    strlcpy(out, value, size - sizeof(int));
    *(int *)(out + size - sizeof(int)) = 0;
    copy_to_user(arg, out, size);
    kfree(out);
}

static void susfs_handle_reboot(unsigned int cmd, void __user *arg)
{
    struct suki_susfs_log toggle;

    if (current_uid().val != 0 || !arg)
        return;
    switch (cmd) {
    case SukiCMD_ADD_PATH:
    case SukiCMD_ADD_PATH_LOOP:
        susfs_add_path_compat(arg);
        break;
    case SukiCMD_SET_UNAME:
        susfs_set_uname_compat(arg);
        break;
    case SukiCMD_ENABLE_LOG:
        if (!copy_from_user(&toggle, arg, sizeof(toggle))) {
            susfs_set_log(toggle.enabled != 0);
            toggle.err = 0;
            copy_to_user(arg, &toggle, sizeof(toggle));
        }
        break;
    case SukiCMD_HIDE_MOUNTS:
        if (!copy_from_user(&toggle, arg, sizeof(toggle))) {
            susfs_set_hide_mounts_for_root(toggle.enabled != 0);
            toggle.err = 0;
            copy_to_user(arg, &toggle, sizeof(toggle));
        }
        break;
    case SukiCMD_ADD_KSTAT:
    case SukiCMD_ADD_KSTAT_STATIC:
        susfs_kstat_compat(arg, false);
        break;
    case SukiCMD_UPDATE_KSTAT:
        susfs_kstat_compat(arg, true);
        break;
    case SukiCMD_ADD_MAP:
        susfs_add_mount_compat(arg);
        break;
    case SukiCMD_SHOW_VERSION:
        susfs_write_status(arg, "v1.5.5-k419", sizeof(struct suki_susfs_version));
        break;
    case SukiCMD_SHOW_VARIANT:
        susfs_write_status(arg, "NON-GKI", sizeof(struct suki_susfs_version));
        break;
    case SukiCMD_SHOW_FEATURES:
        susfs_write_status(arg, "sus_path,sus_mount,sus_kstat,spoof_uname",
                           sizeof(struct suki_susfs_features));
        break;
    default:
        break;
    }
}
#endif

static int anon_ksu_release(struct inode *inode, struct file *filp)
{
    pr_info("ksu fd released\n");
    return 0;
}

static long anon_ksu_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return ksu_supercall_handle_ioctl(cmd, (void __user *)arg);
}

static const struct file_operations anon_ksu_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = anon_ksu_ioctl,
    .compat_ioctl = anon_ksu_ioctl,
    .release = anon_ksu_release,
};

static int ksu_install_fd_flags(unsigned int flags)
{
    struct file *filp;
    int fd;

    fd = get_unused_fd_flags(flags);
    if (fd < 0) {
        pr_err("ksu_install_fd: failed to get unused fd\n");
        return fd;
    }

    filp = anon_inode_getfile("[ksu_driver]", &anon_ksu_fops, NULL,
                              O_RDWR | flags);
    if (IS_ERR(filp)) {
        pr_err("ksu_install_fd: failed to create anon inode file\n");
        put_unused_fd(fd);
        return PTR_ERR(filp);
    }

    fd_install(fd, filp);
    pr_info("ksu fd installed: %d for pid %d\n", fd, current->pid);
    return fd;
}

int ksu_install_fd(void)
{
    return ksu_install_fd_flags(O_CLOEXEC);
}

int ksu_install_manager_fd(void)
{
    return ksu_install_fd_flags(0);
}

static void ksu_install_fd_tw_func(struct callback_head *cb)
{
    struct ksu_install_fd_tw *tw = container_of(cb, struct ksu_install_fd_tw, cb);
    int fd = ksu_install_fd();

    pr_info("[%d] install ksu fd: %d\n", current->pid, fd);
    if (copy_to_user(tw->outp, &fd, sizeof(fd))) {
        pr_err("install ksu fd reply err\n");
        ksu_close_fd(fd);
    }

    kfree(tw);
}

static int reboot_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct pt_regs *real_regs = PT_REAL_REGS(regs);
    int magic1 = (int)PT_REGS_PARM1(real_regs);
    int magic2 = (int)PT_REGS_PARM2(real_regs);

#ifdef CONFIG_KSU_SUSFS
    if (magic1 == KSU_INSTALL_MAGIC1 && magic2 == SukiSUSFS_MAGIC) {
        unsigned int cmd = (unsigned int)PT_REGS_PARM3(real_regs);
        void __user *arg = (void __user *)PT_REGS_SYSCALL_PARM4(real_regs);
        susfs_handle_reboot(cmd, arg);
        return 0;
    }
#endif

    if (magic1 == KSU_INSTALL_MAGIC1 && magic2 == KSU_INSTALL_MAGIC2) {
        struct ksu_install_fd_tw *tw;
        unsigned long arg4 = (unsigned long)PT_REGS_SYSCALL_PARM4(real_regs);

        tw = kzalloc(sizeof(*tw), GFP_ATOMIC);
        if (!tw)
            return 0;

        tw->outp = (int __user *)arg4;
        tw->cb.func = ksu_install_fd_tw_func;

        if (task_work_add(current, &tw->cb, TWA_RESUME)) {
            kfree(tw);
            pr_warn("install fd add task_work failed\n");
        }
    }

    return 0;
}

static struct kprobe reboot_kp = {
    .symbol_name = REBOOT_SYMBOL,
    .pre_handler = reboot_handler_pre,
};

void __init ksu_supercalls_init(void)
{
    int rc;

    ksu_supercall_dump_commands();

    rc = register_kprobe(&reboot_kp);
    if (rc) {
        pr_err("reboot kprobe failed: %d\n", rc);
    } else {
        pr_info("reboot kprobe registered successfully\n");
    }
}

void __exit ksu_supercalls_exit(void)
{
    unregister_kprobe(&reboot_kp);
    ksu_supercall_cleanup_state();
}
