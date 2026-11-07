# Ath10kMac

[中文](#中文) | [English](#english)

## 中文

### 概述

Ath10kMac 是一个实验性的 macOS 内核扩展（kext），面向 Intel x86_64 Mac 与黑苹果（Hackintosh）系统上的 Qualcomm Atheros Wi-Fi 网卡。它是 [jonasw8/Ath10kMac](https://github.com/jonasw8/Ath10kMac) 的社区分支。

本分支记录并扩展了 三星 NP800G5M-X06CN 上 QCA9377 网卡的支持。构建成功只代表代码编译通过，并不证明 Wi-Fi 在其它网卡版本、笔记本、macOS 版本或路由器上可用。

### 编译产物

当前本地构建产物（Intel x86_64）：

- **Kext**: [Ath10kMac.kext](build/out/Ath10kMac.kext)
- **App**: [Ath10k Connectivity.app](build/Ath10k%20Connectivity.app)

这两个目录是本地 `make kext` / `make app` 的输出，不是 GitHub 发行包。驱动仍属实验性，首次测试请先使用可恢复的 USB EFI，不要直接替换正在使用的内置 EFI。

### 本分支的改动

- 新增一个 QCA9377 board-data 配置，对应 subsystem **144d:4133**（三星 NP800G5M-X06CN）。它使用已验证 `board-2.bin` 中精确的 API2 条目（8124 字节，SHA-256 `cb4aa4796ed742722925d82eab92a85d9c400dda05e6f878b492264454b1b81d`）。目标机器报告 `hw1.1`，选择 `board-api2-exact`，可以扫描网络、连接 WPA2（包括 AES/TKIP 混合组播密钥的 AP）、获取 DHCP 并通过双向流量测试。
- 修复了主信道高于 14 的 AP 在 beacon 中携带 DS Parameter Set 时 5 GHz 网络无法被发现的问题。旧解析器会把整个 beacon 判为格式错误，导致受影响的 5 GHz 网络即使扫描访问了对应信道也从不出现。
- 新增启动时的快速首次扫描。驱动现在以 200 ms 驻留时间扫描 22 个可连接的非 DFS 信道（约 4–5 秒），而不是 38 个信道各 500 ms（约 19 秒）。手动 Scan 仍覆盖全部 38 个信道，包括 DFS。配套应用也会复用驱动已完成的首次扫描，而不是立刻再发起一次完整扫描。
- 扩展了 WPA2-Personal 安全路径，支持 AES/CCMP pairwise 加密，组密钥可以是 AES/CCMP 或 TKIP。这使得已测试的混合 WPA2 配置可用：该 AP 同时宣告 TKIP 与 CCMP pairwise 套件，并使用 TKIP 组密钥。TKIP pairwise 加密仍不受支持。
- 修复了硬件卸载场景下 TKIP 组密钥的安装：RSN 密钥描述符中的两个 Michael MIC 密钥现在按固件期望的顺序交换，与 wpa_supplicant 一致。若不做这个交换，组播帧（DHCP 响应与 ARP）会 MIC 校验失败，应用可能显示 WPA2 已连接，但接口始终拿不到地址。
- 新增并扩展了针对 QCA9377 识别、WPA2 安全套件解析、密钥安装/更新处理和安全诊断的回归测试。
- 修复了 CE ring 的 DMA 映射：现在会遍历所有 `IODMACommand` 段，只有在这些段在网卡 32 位 DMA 地址范围内连续覆盖完整缓冲区时才接受该 ring。还新增了 ring 分配失败的诊断信息。这解决了测试系统恢复过程中观察到的 64 KiB ring 只得到 4 KiB 段结果的问题。
- 改进了 Ath10k Connectivity 配套应用：菜单栏连接状态、带动画的连接中指示、可选的 Keychain 密码存储、按优先顺序重连已保存网络、登录时启动，以及一份不包含 Wi-Fi 密码的可复制 AP 安全报告。

### 重要：为其他 QCA9377 板卡添加支持

kext 已经匹配 QCA9377 的 PCI ID `144d:4133`。该匹配只会让驱动附着到设备上，并不保证网卡有匹配的 board-data payload。Ath10kMac 在复位后读取芯片版本以选择受支持的硬件候选，然后按精确的 PCI subsystem ID 和芯片版本选择内嵌 payload。

要添加另一块 QCA9377：

1. 保留现有的 Samsung 配置。使用新网卡真实的 subsystem ID，以及来自相应 Qualcomm/Linux 固件集的、针对该板卡验证过的 board data。
2. 扩展 `scripts/prepare-firmware.py`，校验并将该 board data 打包为独立配置。不要手动编辑生成的 `src/kext/Ath10kPayload.hpp`。
3. 按精确的 PCI device、subsystem vendor/device、芯片版本和兼容固件 target 注册配置。并在 `tests/test_qca9377_flow.cpp` 中加入配置选择覆盖。如果硅片版本或固件 target 不同，按需添加匹配的硬件候选与经验证的固件/OTP payload。
4. 不要伪造 subsystem ID，也不要盲目复用 Samsung 校准数据。首次测试请使用已备份的 USB EFI，并在声称支持前验证驱动启动与 Wi-Fi。

#### 社区其他对 QCA9377 板卡的支持

- [戴尔 Inspiron 15-3567](https://github.com/thekunalthakur/Ath10kMac)

### 硬件与软件验证

本分支的 QCA9377 工作已在以下环境测试：

| 项目 | 目标 |
| --- | --- |
| 笔记本 | Samsung NP800G5M-X06CN |
| PCI 功能 | `03:00.0` |
| PCI vendor/device | `168c:0042` |
| PCI subsystem | `144d:4133` |
| ACPI 路径 | `PciRoot(0x0)/Pci(0x1D,0x4)/Pci(0x0,0x0)` |
| 已测试系统 / build | macOS Sequoia 15.7.5，build 24G624 |
| 固件配置 | `QCA9377/hw1.1-samsung`，芯片版本 `1`，精确 API2 board |

在该配置下，网络扫描、连接和互联网流量已在测试的 2.4 GHz 与 5 GHz 网络上得到确认。一个使用 CCMP/AES pairwise 和 TKIP 组加密的 WPA2-Personal 混合加密网络成功连接。

### 驱动与应用的工作方式

Ath10kMac 负责网卡与 Wi-Fi 连接。macOS 通过一个以太网风格的网络接口看到该连接；驱动不会为该网卡添加 macOS 常规的 Wi-Fi 控制界面。

使用 **Ath10k Connectivity** 扫描、选择网络、输入密码、连接并查看连接状态。连接期间请保持应用运行。若启用，应用会把密码存入 macOS Keychain，并在启动时按优先顺序尝试已保存网络。其菜单栏菜单可设置为登录时启动应用。Launch at Login 需要 macOS 13 或更高版本。

睡眠唤醒后，应用可以重新初始化网卡并重连。Wi-Fi 关联在睡眠期间不会保持（WoWLAN 未实现）。

### Wi-Fi 安全支持

| 网络/安全模式 | 状态 |
| --- | --- |
| 开放网络 | 支持 |
| WPA2-Personal / PSK | 支持 AES/CCMP pairwise 加密，以及 AES/CCMP 或 TKIP 组加密 |
| 混合 WPA2 宣告 | 当网卡能选择 CCMP pairwise 时支持；TKIP 组加密已测试 |
| WPA3-Personal / SAE | 不支持 |
| 强制 Protected Management Frames（PMF / 802.11w） | 不支持 |
| TKIP pairwise 加密 | 不支持 |
| WPA/WPA2 Enterprise / EAP | 不支持 |
| DFS 信道 | 不支持 |
| 接入点/主机模式、AirDrop 与 AWDL | 不支持 |

安全支持是有意受限的。要求 WPA3、PMF、TKIP pairwise 或企业级认证的路由器将无法连接。可选 PMF 行为尚未广泛测试。

### 从源码构建

在 Intel Mac 上使用 macOS、Xcode 及其命令行工具、Python 3，以及仓库的 MacKernelSDK 子模块进行构建。

克隆仓库并初始化子模块：

```sh
git clone --recurse-submodules https://github.com/zjr-fork/Ath10kMac.git
cd Ath10kMac
```

若已克隆但未初始化子模块，运行：

```sh
git submodule update --init --recursive
```

构建 kext 与配套应用：

```sh
make clean
make kext
make app
```

固件准备与 kext 符号检查会在构建中执行。输出：

- Kext: `build/out/Ath10kMac.kext`
- App: `build/Ath10k Connectivity.app`

### 使用 OpenCore 测试

**首次测试不要替换可用的内置 EFI。** 请保留可恢复的备份，并先通过独立的 USB EFI 启动。

1. 构建 kext，并将 `build/out/Ath10kMac.kext` 复制到 USB EFI 的 `EFI/OC/Kexts/`。替换旧注入时，请退出 Ath10k Connectivity 并替换整个 kext bundle；不要合并两个 `Contents` 目录。
2. 在 OpenCore 的 `Kernel -> Add` 中为 `Ath10kMac.kext` 添加条目：
   - Arch: `x86_64`
   - BundlePath: `Ath10kMac.kext`
   - ExecutablePath: `Contents/MacOS/Ath10kMac`
   - PlistPath: `Contents/Info.plist`
   - Enabled: `True`
3. 使用与你的 OpenCore 版本匹配的工具校验 `config.plist`，并通过 USB 的 OpenCore 条目启动。
4. 确认 kext 已加载，使用 Ath10k Connectivity 扫描并连接，并在考虑修改内置 EFI 前验证实际流量。

OpenCore 配置因系统而异。请保留现有 kext 顺序、现有 `Kernel -> Add` bundle 路径及其他必需的 kext；本 README 不提供通用 EFI 或 config.plist。

### 报告测试或问题

欢迎来自其他机器——尤其是 QCA9377 系统——的报告。请包含：

- 笔记本与网卡型号、已知的网卡版本、PCI vendor/device 与 subsystem ID。
- macOS 版本/build、Ath10kMac 版本与 Ath10k Connectivity 版本。
- Wi-Fi 频段、信道与安全模式（例如 WPA2-PSK 使用 CCMP pairwise 与 TKIP 组密钥）。
- 发生了什么、你期望什么，以及复现步骤。
- 问题发生在启动、扫描、认证/协商、流量、断开还是唤醒阶段。
- 对于启动/重连延迟，请说明是否启用了 verbose 或 debug 启动参数。

在项目目录中收集诊断信息：

```sh
sudo sh scripts/test-runtime.sh > ~/Desktop/ath10k-runtime.txt
```

该脚本会采样 Ath10kMac 运行时状态、读取近期内核日志，并记录网络接口/DHCP 详情。报告写入桌面；不会上传。附加前请检查并隐去 SSID、BSSID、MAC 地址、IP 地址、序列号及其他隐私信息。不要包含 Wi-Fi 密码或未经脱敏的 EFI/config.plist。

### 许可证

本仓库中的 Ath10kMac 项目代码以 **GNU General Public License v3.0（GPL-3.0）** 发布。见仓库根目录的 [LICENSE](LICENSE)。第三方文件与捆绑资源可能有单独的条款。

QCA9377 固件按 Qualcomm Atheros 的独立条款分发，并包含附加声明。再分发固件前，请阅读 [固件许可证](firmware/LICENSE.QualcommAtheros_ath10k)、[固件声明](firmware/QCA9377/hw1.0/notice_ath10k_firmware-6.txt) 与 [来源记录](firmware/QCA9377/hw1.0/PROVENANCE.json)。准备好的固件 payload 源自这些文件；根目录的 GPL 许可不能替代它们的条款。

MacKernelSDK 以 Git 子模块形式包含，并保留其上游项目提供的许可。条款见 [Acidanthera/MacKernelSDK](https://github.com/acidanthera/MacKernelSDK)。

### 致谢

本分支基于 [jonasw8/Ath10kMac](https://github.com/jonasw8/Ath10kMac) 以及项目已致谢的更广泛社区工作。组件许可、来源与详细致谢见源码文件与 firmware 目录。

## English

### Overview

Ath10kMac is an experimental macOS kernel extension (kext) for Qualcomm Atheros Wi-Fi cards on Intel x86_64 Macs and Hackintosh systems. It is a community fork of [jonasw8/Ath10kMac](https://github.com/jonasw8/Ath10kMac).

This branch documents and extends support for the QCA9377 card in the Samsung NP800G5M-X06CN. A successful build only means the code compiles; it does not prove that Wi-Fi works on other card revisions, laptops, macOS versions, or routers.

### Build Artifacts

Current local build artifacts (Intel x86_64):

- **Kext**: [Ath10kMac.kext](build/out/Ath10kMac.kext)
- **App**: [Ath10k Connectivity.app](build/Ath10k%20Connectivity.app)

These directories are the output of local `make kext` / `make app` runs, not GitHub release packages. The driver is still experimental. For first-time testing, use a recoverable USB EFI; do not replace a working built-in EFI directly.

### Changes in This Branch

- Added a QCA9377 board-data configuration for subsystem **144d:4133** (Samsung NP800G5M-X06CN). It uses a precisely verified API2 entry from a `board-2.bin` (8124 bytes, SHA-256 `cb4aa4796ed742722925d82eab92a85d9c400dda05e6f878b492264454b1b81d`). The target machine reports `hw1.1`, selects `board-api2-exact`, and can scan networks, connect to WPA2 (including APs with AES/TKIP mixed group ciphers), obtain DHCP, and pass bidirectional traffic tests.
- Fixed 5 GHz networks not being discovered when an AP on a primary channel above 14 carried a DS Parameter Set in its beacon. The old parser rejected the entire beacon as malformed, so affected 5 GHz networks never appeared even if the corresponding channel was scanned.
- Added a fast initial scan at startup. The driver now scans 22 connectable non-DFS channels with a 200 ms dwell time (about 4–5 seconds) instead of 38 channels at 500 ms each (about 19 seconds). A manual Scan still covers all 38 channels, including DFS. The companion app also reuses the driver's completed initial scan instead of immediately starting another full scan.
- Extended the WPA2-Personal security path to support AES/CCMP pairwise encryption with either AES/CCMP or TKIP group ciphers. This makes the tested mixed WPA2 configuration work: the AP advertises both TKIP and CCMP pairwise suites and uses a TKIP group key. TKIP pairwise encryption remains unsupported.
- Fixed installation of TKIP group keys in the hardware-offload scenario: the two Michael MIC keys in the RSN key descriptor are now swapped in the order the firmware expects, matching wpa_supplicant. Without this swap, multicast frames (DHCP responses and ARP) fail the MIC check; the app may show WPA2 as connected, but the interface never obtains an address.
- Added and extended regression tests covering QCA9377 identification, WPA2 security suite parsing, key installation/update handling, and security diagnostics.
- Fixed DMA mapping for CE rings: all `IODMACommand` segments are now walked, and a ring is accepted only when those segments contiguously cover the full buffer within the card's 32-bit DMA address range. Also added diagnostics for ring allocation failures. This resolves an issue seen during system restore on the test machine, where a 64 KiB ring received only a 4 KiB segment result.
- Improved the Ath10k Connectivity companion app: menu bar connection status, an animated connecting indicator, optional Keychain password storage, priority-ordered reconnection to saved networks, launch at login, and a copyable AP security report that excludes Wi-Fi passwords.

### Important: Adding Support for Other QCA9377 Boards

The kext already matches the QCA9377 PCI ID `144d:4133`. That match only lets the driver attach to the device; it does not guarantee that the card has a matching board-data payload. After reset, Ath10kMac reads the chip version to select supported hardware candidates, then selects an embedded payload by exact PCI subsystem ID and chip version.

To add another QCA9377:

1. Keep the existing Samsung configuration. Use the new card's real subsystem ID and board data verified for that board from the corresponding Qualcomm/Linux firmware set.
2. Extend `scripts/prepare-firmware.py` to validate and package that board data as a separate configuration. Do not manually edit the generated `src/kext/Ath10kPayload.hpp`.
3. Register the configuration with the exact PCI device, subsystem vendor/device, chip version, and a compatible firmware target. Add a configuration-selection override in `tests/test_qca9377_flow.cpp`. If the silicon version or firmware target differs, add matching hardware candidates and verified firmware/OTP payloads as needed.
4. Do not fake subsystem IDs, and do not blindly reuse Samsung calibration data. For first-time testing, use a backed-up USB EFI and verify driver startup and Wi-Fi before claiming support.

#### Other Community Support for QCA9377 Boards

- [Dell Inspiron 15-3567](https://github.com/thekunalthakur/Ath10kMac)

### Hardware and Software Validation

The QCA9377 work in this branch has been tested in the following environment:

| Item | Target |
| --- | --- |
| Laptop | Samsung NP800G5M-X06CN |
| PCI function | `03:00.0` |
| PCI vendor/device | `168c:0042` |
| PCI subsystem | `144d:4133` |
| ACPI path | `PciRoot(0x0)/Pci(0x1D,0x4)/Pci(0x0,0x0)` |
| Tested system / build | macOS Sequoia 15.7.5, build 24G624 |
| Firmware configuration | `QCA9377/hw1.1-samsung`, chip version `1`, exact API2 board |

For this configuration, network scanning, connection, and internet traffic have been confirmed on the tested 2.4 GHz and 5 GHz networks. A WPA2-Personal mixed-security network using CCMP/AES pairwise and TKIP group encryption connected successfully.

### How the Driver and App Work

Ath10kMac handles the card and the Wi-Fi connection. macOS sees that connection as an Ethernet-style network interface; the driver does not add macOS's usual Wi-Fi control interface for the card.

Use **Ath10k Connectivity** to scan, choose a network, enter a password, connect, and view connection status. Keep the app running while connecting. If enabled, the app stores passwords in the macOS Keychain and tries saved networks in priority order at startup. Its menu bar menu can be set to launch the app at login. Launch at Login requires macOS 13 or later.

After sleep and wake, the app can reinitialize the card and reconnect. Wi-Fi association is not maintained during sleep (WoWLAN is not implemented).

### Wi-Fi Security Support

| Network / security mode | Status |
| --- | --- |
| Open networks | Supported |
| WPA2-Personal / PSK | Supported with AES/CCMP pairwise encryption, and AES/CCMP or TKIP group encryption |
| Mixed WPA2 advertisements | Supported when the card can select CCMP pairwise; TKIP group encryption tested |
| WPA3-Personal / SAE | Not supported |
| Required Protected Management Frames (PMF / 802.11w) | Not supported |
| TKIP pairwise encryption | Not supported |
| WPA/WPA2 Enterprise / EAP | Not supported |
| DFS channels | Not supported |
| Access point / host mode, AirDrop, and AWDL | Not supported |

Security support is intentionally limited. Routers requiring WPA3, PMF, TKIP pairwise, or enterprise authentication will not connect. Optional PMF behavior is not widely tested.

### Building from Source

Build on an Intel Mac with macOS, Xcode and its command-line tools, Python 3, and the repository's MacKernelSDK submodule.

Clone the repository and initialize submodules:

```sh
git clone --recurse-submodules https://github.com/zjr-fork/Ath10kMac.git
cd Ath10kMac
```

If you already cloned but did not initialize submodules, run:

```sh
git submodule update --init --recursive
```

Build the kext and the companion app:

```sh
make clean
make kext
make app
```

Firmware preparation and kext symbol checks run as part of the build. Outputs:

- Kext: `build/out/Ath10kMac.kext`
- App: `build/Ath10k Connectivity.app`

### Testing with OpenCore

**Do not replace a working built-in EFI for first-time testing.** Keep a recoverable backup and boot from a separate USB EFI first.

1. Build the kext and copy `build/out/Ath10kMac.kext` to `EFI/OC/Kexts/` on the USB EFI. When replacing an older injection, quit Ath10k Connectivity and replace the whole kext bundle; do not merge two `Contents` directories.
2. Add an entry for `Ath10kMac.kext` under OpenCore's `Kernel -> Add`:
   - Arch: `x86_64`
   - BundlePath: `Ath10kMac.kext`
   - ExecutablePath: `Contents/MacOS/Ath10kMac`
   - PlistPath: `Contents/Info.plist`
   - Enabled: `True`
3. Validate `config.plist` with a tool matching your OpenCore version, and boot via the USB OpenCore entry.
4. Confirm the kext is loaded, use Ath10k Connectivity to scan and connect, and verify real traffic before considering changes to the built-in EFI.

OpenCore configurations vary by system. Keep your existing kext order, existing `Kernel -> Add` bundle paths, and other required kexts; this README does not provide a generic EFI or config.plist.

### Reporting Test Results or Issues

Reports from other machines — especially QCA9377 systems — are welcome. Please include:

- Laptop and card model, known card revision, PCI vendor/device and subsystem IDs.
- macOS version/build, Ath10kMac version, and Ath10k Connectivity version.
- Wi-Fi band, channel, and security mode (e.g. WPA2-PSK with CCMP pairwise and TKIP group keys).
- What happened, what you expected, and reproduction steps.
- Whether the issue occurs at startup, scanning, authentication/negotiation, traffic, disconnect, or wake.
- For startup/reconnection latency, state whether verbose or debug boot arguments are enabled.

Collect diagnostics from the project directory:

```sh
sudo sh scripts/test-runtime.sh > ~/Desktop/ath10k-runtime.txt
```

The script samples Ath10kMac runtime state, reads recent kernel logs, and records network interface/DHCP details. The report is written to the Desktop; nothing is uploaded. Review and redact SSIDs, BSSIDs, MAC addresses, IP addresses, serial numbers, and other private information before attaching. Do not include Wi-Fi passwords or unredacted EFI/config.plist files.

### License

The Ath10kMac project code in this repository is released under the **GNU General Public License v3.0 (GPL-3.0)**. See [LICENSE](LICENSE) at the repository root. Third-party files and bundled resources may carry separate terms.

The QCA9377 firmware is distributed under Qualcomm Atheros' own terms and includes additional notices. Read the [firmware license](firmware/LICENSE.QualcommAtheros_ath10k), the [firmware notice](firmware/QCA9377/hw1.0/notice_ath10k_firmware-6.txt), and the [provenance record](firmware/QCA9377/hw1.0/PROVENANCE.json) before redistributing firmware. The prepared firmware payloads derive from these files; the GPL license at the root does not replace their terms.

MacKernelSDK is included as a Git submodule and keeps the license provided by its upstream project. See [Acidanthera/MacKernelSDK](https://github.com/acidanthera/MacKernelSDK) for terms.

### Acknowledgements

This fork is based on [jonasw8/Ath10kMac](https://github.com/jonasw8/Ath10kMac) and the broader community work the project already credits. See source files and the firmware directory for component licenses, provenance, and detailed acknowledgements.
