# Ath10kMac

[中文](#中文) | [English](#english)

## 中文

### 概述

Ath10kMac 是一个实验性的 macOS 内核扩展（kext），面向 Intel x86_64 Mac 与黑苹果（Hackintosh）系统上的 Qualcomm Atheros Wi-Fi 网卡。它是 [jonasw8/Ath10kMac](https://github.com/jonasw8/Ath10kMac) 的社区分支。

本分支记录并扩展了对 QCA9377 网卡的支持，包括 Dell Inspiron 15-3567 和 Samsung NP800G5M-X06CN。驱动仍属实验性。构建成功只代表代码编译通过，并不证明 Wi-Fi 在其它网卡版本、笔记本、macOS 版本或路由器上可用。

目前没有预编译发行包或安装程序。请从源码构建，并先用备用 USB EFI 测试，再改动可用的内置 EFI。

### 编译产物 / Build artifacts

当前本地构建产物（Intel x86_64）：

- **Kext**: [build/out/Ath10kMac.kext](build/out/Ath10kMac.kext)
- **App**: [build/Ath10k Connectivity.app](build/Ath10k%20Connectivity.app)

这两个目录是本地 `make kext` / `make app` 的输出，不是 GitHub 发行包。驱动仍属实验性，首次测试请先使用可恢复的 USB EFI，不要直接替换正在使用的内置 EFI。

### 本分支的改动

- 新增一个由网卡 PCI ID 选择的 QCA9377 board-data 配置：vendor/device **168c:0042**，subsystem **1028:1810**。固件准备步骤会把对应的 board data 打包进驱动 payload。
- 新增第二个 QCA9377 board-data 配置，对应 subsystem **144d:4133**（Samsung NP800G5M-X06CN）。它使用已验证 `board-2.bin` 中精确的 API2 条目（8124 字节，SHA-256 `cb4aa4796ed742722925d82eab92a85d9c400dda05e6f878b492264454b1b81d`）。目标机器现在运行 v0.0.100，报告 `hw1.1`，选择 `board-api2-exact`，可以扫描网络、连接 WPA2（包括 AES/TKIP 混合组播密钥的 AP）、获取 DHCP 并通过双向流量测试。
- 修复了主信道高于 14 的 AP 在 beacon 中携带 DS Parameter Set 时 5 GHz 网络无法被发现的问题。旧解析器会把整个 beacon 判为格式错误，导致受影响的 5 GHz 网络即使扫描访问了对应信道也从不出现。v0.0.101 接受同频段的 DS 信道，同时仍拒绝 DS/HT 信道冲突。
- v0.0.102 新增启动时的快速首次扫描。驱动现在以 200 ms 驻留时间扫描 22 个可连接的非 DFS 信道（约 4–5 秒），而不是 38 个信道各 500 ms（约 19 秒）。手动 Scan 仍覆盖全部 38 个信道，包括 DFS。配套应用也会复用驱动已完成的首次扫描，而不是立刻再发起一次完整扫描。
- 扩展了 WPA2-Personal 安全路径，支持 AES/CCMP pairwise 加密，组密钥可以是 AES/CCMP 或 TKIP。这使得已测试的混合 WPA2 配置可用：该 AP 同时宣告 TKIP 与 CCMP pairwise 套件，并使用 TKIP 组密钥。TKIP pairwise 加密仍不受支持。
- 修复了硬件卸载场景下 TKIP 组密钥的安装：RSN 密钥描述符中的两个 Michael MIC 密钥现在按固件期望的顺序交换，与 wpa_supplicant 一致。若不做这个交换，组播帧（DHCP 响应与 ARP）会 MIC 校验失败，应用可能显示 WPA2 已连接，但接口始终拿不到地址。
- 新增并扩展了针对 QCA9377 识别、WPA2 安全套件解析、密钥安装/更新处理和安全诊断的回归测试。
- 修复了 CE ring 的 DMA 映射：现在会遍历所有 `IODMACommand` 段，只有在这些段在网卡 32 位 DMA 地址范围内连续覆盖完整缓冲区时才接受该 ring。还新增了 ring 分配失败的诊断信息。这解决了测试系统恢复过程中观察到的 64 KiB ring 只得到 4 KiB 段结果的问题。
- 改进了 Ath10k Connectivity 配套应用：菜单栏连接状态、带动画的连接中指示、可选的 Keychain 密码存储、按优先顺序重连已保存网络、登录时启动，以及一份不包含 Wi-Fi 密码的可复制 AP 安全报告。

### 重要：为其他 QCA9377 板卡添加支持

kext 已经匹配 QCA9377 的 PCI ID `168c:0042`。该匹配只会让驱动附着到设备上，并不保证网卡有匹配的 board-data payload。Ath10kMac 在复位后读取芯片版本以选择受支持的硬件候选，然后按精确的 PCI subsystem ID 和芯片版本选择内嵌 payload。

当前 QCA9377 payload 白名单覆盖 subsystem `1028:1810`（已测试的 Dell 网卡）与 `144d:4133`（Samsung），芯片版本 `1`（`hw1.1`），target `0x05020001`。另一台笔记本可能使用相同的 QCA9377 PCI ID，但需要不同的板卡校准数据。仅有 PCI 匹配并不代表该板卡受支持。

要添加另一块 QCA9377：

1. 保留现有的 Dell 配置。使用新网卡真实的 subsystem ID，以及来自相应 Qualcomm/Linux 固件集的、针对该板卡验证过的 board data。
2. 扩展 `scripts/prepare-firmware.py`，校验并将该 board data 打包为独立配置。不要手动编辑生成的 `src/kext/Ath10kPayload.hpp`。
3. 按精确的 PCI device、subsystem vendor/device、芯片版本和兼容固件 target 注册配置。并在 `tests/test_qca9377_flow.cpp` 中加入配置选择覆盖。如果硅片版本或固件 target 不同，按需添加匹配的硬件候选与经验证的固件/OTP payload。
4. 不要伪造 subsystem ID，也不要盲目复用 Dell 校准数据。首次测试请使用已备份的 USB EFI，并在声称支持前验证驱动启动与 Wi-Fi。

### 硬件与软件验证

本分支的 QCA9377 工作已在以下环境测试：

| 项目 | 测试配置 |
| --- | --- |
| 笔记本 | Dell Inspiron 15-3567 |
| Wi-Fi 网卡 | Qualcomm Atheros QCA9377 |
| PCI vendor/device | 168c:0042 |
| PCI subsystem | 1028:1810 |
| 操作系统 | macOS Sequoia 15.6，build 24G84，x86_64 |
| 驱动 | Ath10kMac 0.0.98；后续一次运行中已加载且 CE ring 分配成功 |
| 构建工具 | Xcode 16.4 |

在该配置下，网络扫描、连接和互联网流量已在测试的 2.4 GHz 与 5 GHz 网络上得到确认。一个使用 CCMP/AES pairwise 和 TKIP 组加密的 WPA2-Personal 混合加密网络成功连接。

另一次 v0.0.98 运行报告了活动电源状态与成功的 CE ring 分配，Wi-Fi 在延迟后连接。仍需要重复的启动与睡眠/唤醒验证；造成延迟的阶段尚未定位。

这些结果来自一块网卡、一台笔记本和已测试的网络。它们不是兼容性保证，也不是受控的吞吐量基准。上游项目提到的 QCA6174 支持与性能数据并未在本分支的 QCA9377 工作中得到独立复验。

### Samsung NP800G5M-X06CN 目标机型

仓库中还包含一个针对以下机型的 QCA9377 配置：

| 项目 | 目标 |
| --- | --- |
| 笔记本 | Samsung NP800G5M-X06CN |
| PCI 功能 | `03:00.0` |
| PCI vendor/device | `168c:0042` |
| PCI subsystem | `144d:4133` |
| ACPI 路径 | `PciRoot(0x0)/Pci(0x1D,0x4)/Pci(0x0,0x0)` |
| 已测试系统 / build | macOS Sequoia 15.7.5，build 24G624 |
| 固件配置 | `QCA9377/hw1.1-samsung`，芯片版本 `1`，精确 API2 board |

2026-11-07，在将 `EFI/OC/Kexts/Ath10kMac.kext` 替换为 v0.0.100 构建并重启后，机器报告 `Ath10kDriverVersion=0.0.100`、`Ath10kChipRevision=1`、`Ath10kTargetRevision=hw1.1`、`Ath10kPreparedBoardSelection=board-api2-exact`，以及上述精确的 Samsung board key。驱动选择了 31 个 BSS 条目并连接到 `铁塔之巅`（`64:09:80:12:5d:f4`，信道 11），使用 CCMP pairwise 与 TKIP 组密钥。运行状态达到 `Ath10kConnectionState=connected-wpa2`、`Ath10kSecurityPhase=5`、`Ath10kPortAuthorized=Yes`；`en3` 通过 DHCP 获得 `192.168.1.11`，经该接口重复发起 HTTPS 请求均返回 HTTP 200。这验证了 v0.0.100 上的 Samsung 板卡选择、WPA2 握手/密钥安装、DHCP 与双向数据路径。

本仓库中的 v0.0.100 构建包含上游的 QCA9377/TKIP/DMA/睡眠修复以及 0.1.13 配套应用。Python 测试套件、完整的原生测试套件、干净的 kext 构建、符号检查与 app 构建均通过。配套应用现在会清除失败的会话并重新启用 Connect，而不再需要单独恢复步骤。

当前 v0.0.102 构建保留了这些修复并新增启动快速扫描。其完整的 Python 与原生测试套件、干净的 kext 构建、符号检查与 app 构建均通过；驱动 0.0.102 搭配 Ath10k Connectivity 0.1.14。

针对该机型的构建、安装、诊断与回滚清单见 [SAMSUNG-QCA9377-ADAPTATION.md](SAMSUNG-QCA9377-ADAPTATION.md)。

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

### 已知限制与已报告的行为

- Dell subsystem 已用 v0.0.98 完成端到端验证。Samsung subsystem 已用 v0.0.100 完成端到端验证：扫描、WPA2 关联与密钥安装、DHCP，以及经网卡接口的 HTTPS 流量。其他 QCA9377 版本与 subsystem 仍需硬件测试。
- 启动后的自动重连在扫描阶段之外仍可能比预期更久。v0.0.102 在测试系统上把首次扫描从约 19 秒缩短到约 4–5 秒。重连仍是尽力而为；请保持应用打开，若未完成则使用 Scan/Retry。手动 Scan 有意执行较慢的全 38 信道扫描，以便 DFS 网络仍可见。
- 在测试系统上以 verbose/debug 启动时会显示重复的 Ath10kMac 底层诊断信息，并似乎会在启动过程中停顿。启动时的影响与原因尚未测量或定位。报告时应说明是否启用了 verbose 或 Ath10k debug 启动参数。
- 睡眠恢复会重启网卡并重连；不会保留此前的 Wi-Fi 会话。v0.0.100 包含延迟探测与唤醒恢复改动，但 Samsung 目标机型上的重复睡眠/唤醒验证仍待完成。
- kext 是实验性的，若设备或配置不兼容，可能导致内核恐慌或无法启动。

### 从源码构建

在 Intel Mac 上使用 macOS、Xcode 及其命令行工具、Python 3，以及仓库的 MacKernelSDK 子模块进行构建。

克隆仓库并初始化子模块：

```sh
git clone --recurse-submodules https://github.com/thekunalthakur/Ath10kMac.git
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

项目构建目标为 Intel x86_64。上文记录的 QCA9377 运行时验证分别在 Dell 系统的 Sequoia 15.6 和 Samsung v0.0.100 构建的 Sequoia 15.7.5 上完成。构建成功不能验证固件、OpenCore 配置，或其他系统上的 Wi-Fi 行为。

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

Ath10kMac is an experimental macOS kernel extension for Qualcomm Atheros Wi-Fi adapters on Intel x86_64 Macs and Hackintosh systems. It is a community fork of [jonasw8/Ath10kMac](https://github.com/jonasw8/Ath10kMac).

This fork documents and extends support for QCA9377 adapters, including the Dell Inspiron 15-3567 and the Samsung NP800G5M-X06CN. The driver is experimental. A successful build only means that the code compiled; it does not prove that Wi-Fi will work on a different adapter revision, laptop, macOS release, or router.

There is currently no prebuilt release package or installer. The local build outputs are linked under Build artifacts below. Build from source and test with a spare USB EFI before changing a working internal EFI.

### Build artifacts

Current local build outputs (Intel x86_64):

- **Kext**: [build/out/Ath10kMac.kext](build/out/Ath10kMac.kext)
- **App**: [build/Ath10k Connectivity.app](build/Ath10k%20Connectivity.app)

These directories are the `make kext` / `make app` outputs, not a GitHub release package.

### Work in this fork

- Added a QCA9377 board-data profile selected by the adapter's PCI IDs: vendor/device **168c:0042**, subsystem **1028:1810**. The firmware preparation step includes the corresponding board data in the driver payload.
- Added a second QCA9377 board-data profile for subsystem **144d:4133** (Samsung NP800G5M-X06CN). It uses the exact API2 entry from the verified `board-2.bin` (8124 bytes, SHA-256 `cb4aa4796ed742722925d82eab92a85d9c400dda05e6f878b492264454b1b81d`). The target machine now runs v0.0.100, reports `hw1.1`, selects `board-api2-exact`, scans networks, connects to WPA2 (including a mixed AES/TKIP-group AP), obtains DHCP, and passes bidirectional traffic.
- Fixed 5 GHz network discovery for APs that include a DS Parameter Set whose primary channel is above 14. The old parser rejected the entire beacon as malformed, so affected 5 GHz networks never appeared even though the scan visited their channels. v0.0.101 accepts a same-band DS channel and still rejects a DS/HT channel conflict.
- Added a fast first scan for startup in v0.0.102. The driver now scans 22 connectable non-DFS channels with a 200 ms dwell (about 4–5 seconds) instead of 38 channels at 500 ms (about 19 seconds). A manual Scan still covers all 38 channels, including DFS. The companion app also reuses the driver's completed first scan instead of immediately starting another full scan.
- Extended the WPA2-Personal security path to handle AES/CCMP pairwise encryption with either AES/CCMP or TKIP group keys. This allows the tested mixed WPA2 configuration where the access point advertises both TKIP and CCMP pairwise suites and uses a TKIP group key. TKIP pairwise encryption is still unsupported.
- Fixed TKIP group-key installation for hardware offload: the two Michael MIC keys from the RSN key descriptor are now swapped into the order the firmware expects, matching wpa_supplicant. Without this swap, group-addressed frames (DHCP replies and ARP) fail their MIC check, so the app can show a WPA2 connection while the interface never obtains an address.
- Added and expanded regression coverage for QCA9377 identification, WPA2 security-suite parsing, key installation/rekey handling, and security diagnostics.
- Fixed CE-ring DMA mapping to walk all `IODMACommand` segments and accept a ring only when they cover the full buffer contiguously within the adapter's 32-bit DMA address range. Added diagnostics for ring allocation failures. This addresses a 4 KiB segment result for a 64 KiB ring observed during recovery on the test system.
- Improved the Ath10k Connectivity companion app: menu-bar connection states, animated connecting indicator, optional Keychain password storage, preferred saved-network reconnect, Launch at Login, and a copyable access-point security report that does not include the Wi-Fi password.

### Important: Adding support for another QCA9377 board

The kext already matches the QCA9377 PCI ID `168c:0042`. That match only attaches the driver; it does not guarantee that the adapter has a matching board-data payload. Ath10kMac reads the chip revision after reset to choose a supported hardware candidate, then selects an embedded payload by exact PCI subsystem IDs and chip revision.

The current QCA9377 payloads are allowlisted for subsystem `1028:1810` (the tested Dell adapter) and `144d:4133` (Samsung), chip revision `1` (`hw1.1`), and target `0x05020001`. Another laptop can use the same QCA9377 PCI ID but require different board calibration. A PCI match alone does not mean that board is supported.

To add another QCA9377 board:

1. Keep the existing Dell profile. Use the new adapter's actual subsystem IDs and verified board data for that exact board from the appropriate Qualcomm/Linux firmware set.
2. Extend `scripts/prepare-firmware.py` to validate and package that board data as a separate profile. Do not hand-edit the generated `src/kext/Ath10kPayload.hpp`.
3. Register the profile for the exact PCI device, subsystem vendor/device, chip revision, and compatible firmware target. Add profile-selection coverage in `tests/test_qca9377_flow.cpp`. If the silicon revision or firmware target differs, add the matching hardware candidate and verified firmware/OTP payload as needed.
4. Do not spoof the subsystem IDs or reuse Dell calibration blindly. Keep the first test on a backed-up USB EFI and verify driver startup and Wi-Fi before claiming support.

### Hardware and software validation

The QCA9377 work in this fork was tested on:

| Item | Tested configuration |
| --- | --- |
| Laptop | Dell Inspiron 15-3567 |
| Wi-Fi adapter | Qualcomm Atheros QCA9377 |
| PCI vendor/device | 168c:0042 |
| PCI subsystem | 1028:1810 |
| Operating system | macOS Sequoia 15.6, build 24G84, x86_64 |
| Driver | Ath10kMac 0.0.98; loaded and CE-ring allocation succeeded in a follow-up run |
| Build tools | Xcode 16.4 |

On that configuration, network scanning, connection, and internet traffic were confirmed on the tested 2.4 GHz and 5 GHz networks. A WPA2-Personal mixed-cipher network using CCMP/AES pairwise and TKIP group encryption connected successfully.

A follow-up v0.0.98 run reported active power and successful CE-ring allocation, and Wi-Fi connected after a delay. Repeated startup and sleep/wake validation is still needed; the stage responsible for the delay has not been isolated.

These are results from one adapter, one laptop, and the networks tested. They are not a compatibility guarantee or a controlled throughput benchmark. The QCA6174 support and performance figures mentioned in the upstream project have not been independently revalidated as part of this QCA9377 work.

### Samsung NP800G5M-X06CN target

The repository also carries a target-specific QCA9377 profile for:

| Item | Target |
| --- | --- |
| Laptop | Samsung NP800G5M-X06CN |
| PCI function | `03:00.0` |
| PCI vendor/device | `168c:0042` |
| PCI subsystem | `144d:4133` |
| ACPI path | `PciRoot(0x0)/Pci(0x1D,0x4)/Pci(0x0,0x0)` |
| OS / build tested | macOS Sequoia 15.7.5, build 24G624 |
| Firmware profile | `QCA9377/hw1.1-samsung`, chip revision `1`, exact API2 board |

On 2026-11-07, after replacing `EFI/OC/Kexts/Ath10kMac.kext` with the v0.0.100 build and rebooting, the machine reported `Ath10kDriverVersion=0.0.100`, `Ath10kChipRevision=1`, `Ath10kTargetRevision=hw1.1`, `Ath10kPreparedBoardSelection=board-api2-exact`, and the exact Samsung board key above. The driver selected 31 BSS entries and connected to `铁塔之巅` (`64:09:80:12:5d:f4`, channel 11) with CCMP pairwise and a TKIP group key. Runtime state reached `Ath10kConnectionState=connected-wpa2`, `Ath10kSecurityPhase=5`, and `Ath10kPortAuthorized=Yes`; `en3` obtained `192.168.1.11` through DHCP, and repeated HTTPS requests through that interface returned HTTP 200. This validates the Samsung board selection, WPA2 handshake/key installation, DHCP, and bidirectional data path on v0.0.100.

The v0.0.100 build in this repository adds the upstream QCA9377/TKIP/DMA/sleep fixes and the 0.1.13 companion app. Its Python suite, complete native test suite, clean kext build, symbol check, and app build pass. The companion app now clears a failed session and re-enables Connect instead of requiring a separate recovery step.

The current v0.0.102 build keeps those fixes and adds the fast startup scan. Its full Python and native test suites, clean kext build, symbol check, and app build pass; driver 0.0.102 is paired with Ath10k Connectivity 0.1.14.

See [SAMSUNG-QCA9377-ADAPTATION.md](SAMSUNG-QCA9377-ADAPTATION.md) for the target-specific build, installation, diagnostic, and rollback checklist.

### How the driver and app work

Ath10kMac handles the adapter and Wi-Fi connection. macOS sees the resulting connection through an Ethernet-style network interface; the driver does not add the normal macOS Wi-Fi controls for this adapter.

Use **Ath10k Connectivity** to scan, choose a network, enter its password, connect, and view connection status. Keep the app running while connected. If enabled, the app stores the password in the macOS Keychain and tries saved networks in preferred order when it starts. Its menu-bar menu can be set to launch the app at login. Launch at Login uses macOS 13 or later.

After sleep, the app can reinitialize the adapter and reconnect. The Wi-Fi association is not maintained during sleep (WoWLAN is not implemented).

### Wi-Fi security support

| Network/security mode | Status |
| --- | --- |
| Open network | Supported |
| WPA2-Personal / PSK | Supported with AES/CCMP pairwise encryption and AES/CCMP or TKIP group encryption |
| Mixed WPA2 advertisement | Supported when the adapter can select CCMP pairwise; TKIP group encryption was tested |
| WPA3-Personal / SAE | Not supported |
| Required Protected Management Frames (PMF / 802.11w) | Not supported |
| TKIP pairwise encryption | Not supported |
| WPA/WPA2 Enterprise / EAP | Not supported |
| DFS channels | Not supported |
| Access point/host mode, AirDrop, and AWDL | Not supported |

Security support is intentionally limited. A router that requires WPA3, PMF, TKIP pairwise, or enterprise authentication will not connect. Optional PMF behavior has not been broadly tested.

### Known limitations and reported behavior

- The Dell subsystem has end-to-end validation with v0.0.98. The Samsung subsystem is validated end-to-end with v0.0.100: scan, WPA2 association and key installation, DHCP, and HTTPS traffic through the adapter interface. Other QCA9377 revisions and subsystems still need hardware testing.
- Automatic reconnect after startup can still take longer than expected outside the scan phase. v0.0.102 reduces the first scan from about 19 seconds to about 4–5 seconds on the tested system. Reconnection remains best-effort; keep the app open and use Scan/Retry if it does not finish. A manual Scan intentionally performs the slower full 38-channel scan so DFS networks remain visible.
- Verbose/debug boot on the test system displayed repeated low-level Ath10kMac diagnostics and appeared to pause during startup. The boot-time impact and cause have not been measured or isolated. Reports should include whether verbose or Ath10k debug boot arguments were enabled.
- Sleep recovery restarts the adapter and reconnects; it does not preserve the previous Wi-Fi session. v0.0.100 includes delayed-probe and wake-recovery changes, but repeated sleep/wake validation on the Samsung target is still pending.
- The kext is experimental and can cause a kernel panic or prevent startup if a device or configuration is incompatible.

### Build from source

Build on an Intel Mac with macOS, Xcode and its command-line tools, Python 3, and the repository's MacKernelSDK submodule.

Clone the repository and initialize its submodules:

```sh
git clone --recurse-submodules https://github.com/thekunalthakur/Ath10kMac.git
cd Ath10kMac
```

If you already cloned without submodules, run:

```sh
git submodule update --init --recursive
```

Build the kext and the companion app:

```sh
make clean
make kext
make app
```

The firmware preparation and kext symbol check run as part of the build. Outputs:

- Kext: `build/out/Ath10kMac.kext`
- App: `build/Ath10k Connectivity.app`

The project build targets Intel x86_64. QCA9377 runtime validation documented above was on Sequoia 15.6 for the Dell system and Sequoia 15.7.5 for the Samsung v0.0.100 build. Building successfully does not validate the firmware, OpenCore setup, or Wi-Fi behavior on another system.

### Test with OpenCore

**Do not replace a working internal EFI for the first test.** Keep a recoverable backup and boot from a separate USB EFI first.

1. Build the kext and copy `build/out/Ath10kMac.kext` into the USB EFI at `EFI/OC/Kexts/`. When replacing an older injection, quit Ath10k Connectivity and replace the entire kext bundle; do not merge the two `Contents` directories.
2. In OpenCore's `Kernel -> Add`, add an entry for `Ath10kMac.kext` with:
   - Arch: `x86_64`
   - BundlePath: `Ath10kMac.kext`
   - ExecutablePath: `Contents/MacOS/Ath10kMac`
   - PlistPath: `Contents/Info.plist`
   - Enabled: `True`
3. Validate `config.plist` with the tools matching your OpenCore version and boot using the USB's OpenCore entry.
4. Confirm that the kext loads, scan and connect with Ath10k Connectivity, and verify actual traffic before considering any internal-EFI change.

OpenCore configurations differ by system. Preserve the existing kext order, the existing `Kernel -> Add` bundle path, and other required kexts; this README does not provide a universal EFI or config.plist.

### Report a test or issue

Reports from other machines—especially QCA9377 systems—are welcome. Include:

- Laptop and adapter model, adapter revision if known, PCI vendor/device and subsystem IDs.
- macOS version/build, Ath10kMac version, and Ath10k Connectivity version.
- Wi-Fi band, channel, and security mode (for example, WPA2-PSK with CCMP pairwise and TKIP group).
- What happened, what you expected, and steps to reproduce it.
- Whether the problem occurs at boot, scan, authentication/negotiation, traffic, disconnect, or wake.
- For boot/reconnect delays, whether verbose or debug boot arguments were enabled.

To collect diagnostics from the project directory:

```sh
sudo sh scripts/test-runtime.sh > ~/Desktop/ath10k-runtime.txt
```

The script samples Ath10kMac runtime state, reads recent kernel logs, and records network-interface/DHCP details. It writes the report to the Desktop; it does not upload it. Review and redact SSIDs, BSSIDs, MAC addresses, IP addresses, serial numbers, and any other private details before attaching it. Do not include Wi-Fi passwords or an unredacted EFI/config.plist.

### License

The Ath10kMac project code in this repository is released under the **GNU General Public License v3.0 (GPL-3.0)**. See the repository-root [LICENSE](LICENSE). Third-party files and bundled assets can have separate terms.

The QCA9377 firmware is distributed under separate Qualcomm Atheros terms and includes additional notices. Review [the firmware license](firmware/LICENSE.QualcommAtheros_ath10k), [the firmware notices](firmware/QCA9377/hw1.0/notice_ath10k_firmware-6.txt), and [the provenance record](firmware/QCA9377/hw1.0/PROVENANCE.json) before redistributing firmware. The prepared firmware payload is derived from those files; the root GPL license does not replace their terms.

MacKernelSDK is included as a Git submodule and retains the license provided by its upstream project. See [Acidanthera/MacKernelSDK](https://github.com/acidanthera/MacKernelSDK) for its terms.

### Credits

This fork builds on [jonasw8/Ath10kMac](https://github.com/jonasw8/Ath10kMac) and the wider community work already credited in the project. See the source files and firmware directory for component licenses, provenance, and detailed acknowledgments.
