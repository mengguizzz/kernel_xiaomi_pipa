# portcolor — Xiaomi Pad 6 (pipa) ColorOS 17 移植内核编译说明

这份树是在原 HyperOS 4 移植内核基础上，为 ColorOS 17 (Android 17) 移植补丁过的版本。
以下记录是踩过坑之后总结的**可复现构建方式**。

## 1. 工具链（最重要）

**必须用 clang / LLD 20.1.8。**

同一份源码 + 同一份配置：

| 工具链 | 结果 |
|---|---|
| clang/lld **20.1.8**（Arch 的 `clang20`/`lld20`/`llvm20` 包） | ✅ 正常开机 |
| Ubuntu clang 21.1.8 + LLD 21.1.8 | ❌ 卡在第一屏（内核在 console 注册前就挂了，pstore 里没有任何输出） |

```bash
# Arch 上安装：
pacman -S clang20 lld20 llvm20
# 使用时（或用等价 wrapper 把 LD_LIBRARY_PATH 指到 /usr/lib/llvm20/lib）：
export PATH=/usr/lib/llvm20/bin:$PATH
```

## 2. 编译

```bash
git clone -b Lineage-24.0 https://github.com/mengguizzz/kernel_xiaomi_pipa
cd kernel_xiaomi_pipa

make O=out ARCH=arm64 LLVM=1 LLVM_IAS=1 vendor/pipa_defconfig
make O=out ARCH=arm64 LLVM=1 LLVM_IAS=1 -j$(nproc) Image
# 产物：out/arch/arm64/boot/Image
```

`vendor/pipa_defconfig` 里已经包含这份内核的**全部配置改动**
（MEMCG+KMEM+SWAP、POWER_RESET_QCOM、CPU_BOOST、CPU_FREQ_STAT/TIMES、SCHED_DEBUG、
SCHEDSTATS、MQ_IOSCHED_DEADLINE、PSI、THERMAL_WRITABLE_TRIPS、LTO_NONE、
`LOCALVERSION="-perf"` 等）。用 `make vendor/pipa_defconfig && make olddefconfig`
重新展开得到的 `.config` 与实际发布内核逐字节一致。

> LTO 关闭（`CONFIG_LTO_NONE=y`）：这棵树的 LTO 是单线程整链，开着要很久；原厂内核也是 LTO_NONE。

## 3. 打包进 boot 镜像

```bash
# 用 magiskboot 解开现有的 boot.img（保留原 ramdisk / dtb / vbmeta）
magiskboot unpack boot.img
cp out/arch/arm64/boot/Image kernel
magiskboot repack boot.img boot_new.img
```

刷入（Xiaomi Pad 6 走 fastbootd 最稳）：

```bash
adb reboot fastboot
fastboot flash boot_b boot_new.img
fastboot reboot
```

## 4. 这份树里为移植做的内核改动

| 文件 | 改动 |
|---|---|
| `arch/arm64/configs/vendor/pipa_defconfig` | 配置集合（见上） |
| `kernel/sys.c` | ① 在 `newuname` 里调用 `susfs_spoof_uname()`（arm64 没有 `SYS_OLD_UNAME`，原代码挂在旧 `uname` 上等于失效）；② `CONFIG_FAKE_UNAME_5_10` 只对 **uid 0 的系统守护进程**生效，防止 App 通过 `prctl(PR_SET_NAME,"bpfloader")` 探测到假 uname |
| `fs/proc/cmdline.c` | 对 uid ≥ 2000 的进程把 `androidboot.verifiedbootstate=orange` 报成 `green`（`CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG`） |
| `SukiSU-Ultra/kernel/Kbuild` | `KSU_VERSION := 40900`（SukiSU-Ultra 目录本身是 git 仓库，不写死会算成 40940） |
| `SukiSU-Ultra/kernel/runtime/ksud_integration.c` | 音量键安全模式：只在开机后 30 秒内采样、hook 停止后立刻不再计数、判定结果只求值一次（原版在使用中按 3~4 下音量减会误进安全模式） |
| `drivers/gpu/msm/kgsl_pwrctrl.c` | 允许 GPU 降到最低档（空闲 305MHz，负载再升上去） |
| `drivers/power/supply/qcom/smb5-lib-pipa.c` | USB_CDP 对外报成 USB，插电脑能识别为数据连接 |
