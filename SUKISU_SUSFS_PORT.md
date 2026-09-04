# Xiaomi Pad 6 (pipa) SukiSU-Ultra + SUSFS port

This branch is the tested Android 17 kernel tree used by the HyperOS 4 pipa
port. It is based on YumeMichi's pipa kernel and targets Linux 4.19.

## Integrated components

- SukiSU-Ultra v4.2.0, upstream commit `9fbe8fe8` (internal version 40901)
- SUSFS 4.19 hooks from `simonpunk/susfs4ksu`, commit `30e66dc`
- Compatibility dispatcher for SukiSU's reboot-based SUSFS ABI
- Narrow Linux 4.19 seccomp exceptions for SukiSU and SUSFS handshakes
- Existing pipa direct hooks required by this non-GKI kernel

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

The tested toolchain is Android clang `r416183b`. Starting with the pipa
defconfig:

```sh
export PATH=/path/to/clang-r416183b/bin:$PATH
make O=out-pipa ARCH=arm64 LLVM=1 LLVM_IAS=1 vendor/pipa_defconfig
scripts/config --file out-pipa/.config \
  -e KSU -e KSU_DEBUG -e KSU_MANUAL_SU \
  -e KPROBES -e KRETPROBES -e KPROBE_EVENTS \
  -e KSU_SUSFS -e KSU_SUSFS_SUS_PATH \
  -e KSU_SUSFS_SUS_MOUNT -e KSU_SUSFS_SUS_KSTAT \
  -e KSU_SUSFS_TRY_UMOUNT -e KSU_SUSFS_SPOOF_UNAME \
  -e KSU_SUSFS_ENABLE_LOG -d MEMCG -d PSI
make O=out-pipa ARCH=arm64 LLVM=1 LLVM_IAS=1 olddefconfig
make -j$(nproc) O=out-pipa ARCH=arm64 LLVM=1 LLVM_IAS=1 Image
```

## Verified result

The v2 test boot reached Android and reported:

```text
Linux 4.19.325-cip135-st19-perf+ #7
uid=0(root) context=u:r:ksu:s0
SUSFS status: true
SUSFS version: v1.5.5-k419
SUSFS variant: NON-GKI
SUSFS features: sus_path,sus_mount,sus_kstat,spoof_uname
```

Verified boot image SHA-256:

```text
773C93EF839773FD6D735FC063A8E796CE07EFE6BBD6B1D0DC18E180559058BD
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

