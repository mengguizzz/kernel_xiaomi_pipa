# Xiaomi Pad 6 (pipa) SukiSU-Ultra + SUSFS port

This branch is the tested Android 17 kernel tree used by the HyperOS 4 pipa
port. It is based on YumeMichi's pipa kernel and targets Linux 4.19.

## Integrated components

- SukiSU-Ultra v4.2.1-r2, upstream commit `29f8b18d` (internal version 41098)
- SUSFS 4.19 hooks from `simonpunk/susfs4ksu`, commit `30e66dc`
- Compatibility dispatcher for SukiSU's reboot-based SUSFS ABI
- Narrow Linux 4.19 seccomp exceptions for SukiSU and SUSFS handshakes
- Existing pipa direct hooks required by this non-GKI kernel

The v4.2.1 rebase keeps the local 4.19 compatibility layer while importing the
upstream AVC, package-observer, userspace and manager changes. The 4.19 build
uses no-op or `-EOPNOTSUPP` fallbacks for features that depend on newer kernel
internals, and the SUSFS reboot compatibility path is isolated in
`kernel/supercall/susfs_compat.c`.

The manager path installs `[ksu_driver]` with `ksu_install_fd()`. Do not replace
it with `ksu_install_su_fd()`: the latter creates `[ksu_driver_su]`, which is
intended only for the short-lived `su` session and is not detected by the
manager. Using the wrong descriptor leaves root working while the manager shows
"kernel driver not detected".

`drivers/kernelsu` links to `SukiSU-Ultra/kernel`. The complete SukiSU source is
vendored so a clone contains all sources required for the build.

## Configuration

The tested configuration enables:

```text
CONFIG_KSU=y
CONFIG_KSU_DEBUG=y
CONFIG_KSU_MANUAL_SU=y
CONFIG_KPROBES=y
CONFIG_KRETPROBES=y
CONFIG_KPROBE_EVENTS=y
CONFIG_KSU_SUSFS=y
CONFIG_KSU_SUSFS_SUS_PATH=y
CONFIG_KSU_SUSFS_SUS_MOUNT=y
CONFIG_KSU_SUSFS_SUS_KSTAT=y
CONFIG_KSU_SUSFS_TRY_UMOUNT=y
CONFIG_KSU_SUSFS_SPOOF_UNAME=y
CONFIG_KSU_SUSFS_ENABLE_LOG=y
```

Do not enable `CONFIG_MEMCG` or `CONFIG_PSI` on this port. Android userspace may
log failed memcg assignments, but enabling those options caused boot instability
in earlier tests.

Experimental SUSFS functions such as memory-map hiding, mount-ID reordering,
sus_su, AVC-log spoofing, open redirect, cmdline spoofing, and kallsyms hiding
are not enabled in the tested build.

## Build

The tested local toolchain is Ubuntu clang/LLD `20.1.8`. Starting with the pipa
defconfig:

```sh
export PATH=/usr/lib/llvm-20/bin:$PATH
make O=out-pipa ARCH=arm64 LLVM=1 LLVM_IAS=1 vendor/pipa_defconfig
scripts/config --file out-pipa/.config \
  -e KSU -e KSU_DEBUG -e KSU_MANUAL_SU \
  -e KPROBES -e KRETPROBES -e KPROBE_EVENTS \
  -e KSU_SUSFS -e KSU_SUSFS_SUS_PATH \
  -e KSU_SUSFS_SUS_MOUNT -e KSU_SUSFS_SUS_KSTAT \
  -e KSU_SUSFS_TRY_UMOUNT -e KSU_SUSFS_SPOOF_UNAME \
  -e KSU_SUSFS_ENABLE_LOG -d LTO_CLANG -e LTO_NONE -d MEMCG -d PSI
make O=out-pipa ARCH=arm64 LLVM=1 LLVM_IAS=1 olddefconfig
make -j$(nproc) O=out-pipa ARCH=arm64 LLVM=1 LLVM_IAS=1 Image
```

## Verified result

The v4.2.1-r2 test boot reached Android and reported:

```text
Linux 4.19.325-cip136-st20-N0-kernel++ #4
uid=0(root) context=u:r:ksu:s0
ksud 4.2.1-r2 (uapi: 6)
SukiSU-Ultra version: 41098 [v4.2.1-r2-abe57694@HyperOS4.0]
SUSFS status: true
SUSFS variant: NON-GKI
```

The SukiSU manager receives `anon_inode:[ksu_driver]`, reports a valid
`uapi_version`, and the manager automatically updated `/data/adb/ksud` from
4.2.0 to 4.2.1-r2 after the corrected boot was flashed.

Verified boot image:

```text
D:\joeyzhu\Desktop\pak\firmware\boot-n0-millet-trackpad-sukisu41098-fd-fix-local.img
SHA256: 71610D4F396D3AA92B81FF086ABFCA199E9AC1E70595E7F74498171730BA30FA
```

## Known behavior

- Zygisk Next can trigger a non-fatal scheduler warning in `update_curr()` while
  reading `/proc` and changing CPU affinity. The trace does not pass through
  SUSFS and the daemon continues normally.
- The 4.19 kstat backend supports inode, device, link count, and timestamp
  spoofing. New userspace fields for size, block count, and block size are not
  represented by this backend.
- SukiSU's `add-sus-map` configuration is translated to the 4.19 suspicious
  mount list. It is not `/proc/<pid>/maps` memory-map hiding.

