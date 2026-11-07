# Samsung NP800G5M-X06CN / QCA9377 适配说明

本文件记录本仓库针对 Samsung NP800G5M-X06CN 上 QCA9377 的具体适配、验证边界和真机安装步骤。

## 目标环境

| 项目 | 值 |
| --- | --- |
| 笔记本 | Samsung NP800G5M-X06CN |
| PCI 地址 | `03:00.0` |
| PCI vendor/device | `168c:0042` |
| PCI subsystem | `144d:4133` |
| ACPI 路径 | `PciRoot(0x0)/Pci(0x1D,0x4)/Pci(0x0,0x0)` |
| 系统 | macOS Sequoia 15.7.5（24G624），x86_64 |
| 引导 | OpenCore |
| 芯片/固件目标 | chip revision `1`（`hw1.1`），target `0x05020001` |

## 已完成的适配

- 在 `scripts/prepare-firmware.py` 中新增 Samsung 精确 board key：

  `bus=pci,vendor=168c,device=0042,subsystem-vendor=144d,subsystem-device=4133`

- 从仓库内已验证的 `firmware/QCA9377/hw1.0/board-2.bin` 中取出该 key 对应的 API2 board，并生成独立 profile：

  - profile：`QCA9377/hw1.1-samsung`
  - board：8124 bytes
  - SHA-256：`cb4aa4796ed742722925d82eab92a85d9c400dda05e6f878b492264454b1b81d`
  - 输出：`firmware/prepared/QCA9377-144d-4133/`

- 保留 Dell `1028:1810` 的独立 profile，两个 profile 都按 PCI subsystem 和芯片 revision 精确匹配，不使用通用 board 或跨机型校准。
- 引入并整合 QCA9377/TKIP 组密钥、CE ring 全段 DMA 映射、延迟探测、休眠恢复和 Ath10k Connectivity 0.1.14 的改动。
- 将驱动版本统一为 `0.0.102`，App 版本为 `0.1.14`。

生成的 `src/kext/Ath10kPayload.hpp` 是构建产物，不要手工修改。需要变更 board、固件或 OTP 时，修改 `scripts/prepare-firmware.py` 后重新运行固件准备步骤。

## 芯片 revision 限制

QCA9377 的 PCI configuration revision 不等于 ath10k 的 target chip revision。驱动在 reset 后读取 `SOC_CHIP_ID`：

- 只有运行日志/属性显示 chip revision `1`（`hw1.1`）时，才应使用当前 QCA9377 payload。
- 如果显示 revision `0`（`hw1.0`），当前 profile 不应被用于固件/OTP 写入；需要另找经过验证的 hw1.0 固件和 board profile。
- 目标机 v0.0.100 已报告 `Ath10kChipRevision=1`、`Ath10kTargetRevision=hw1.1`，因此当前 `hw1.1-samsung` 匹配目标硬件。

## 验证状态

### 已完成

- 目标机 v0.0.100 真机状态（2026-11-07，替换 `EFI/OC/Kexts/Ath10kMac.kext` 并重启后）：
  - `Ath10kDriverVersion=0.0.100`
  - `Ath10kChipRevision=1`
  - `Ath10kTargetRevision=hw1.1`
  - `Ath10kPreparedBoardKey=bus=pci,vendor=168c,device=0042,subsystem-vendor=144d,subsystem-device=4133`
  - `Ath10kPreparedBoardSelection=board-api2-exact`
  - `Ath10kPreparedBoardSHA256=cb4aa479...`
  - 扫描到 31 个 BSS
  - 连接 `铁塔之巅`（`64:09:80:12:5d:f4`，Ch 11），CCMP pairwise + TKIP group
  - `Ath10kConnectionState=connected-wpa2`
  - `Ath10kSecurityPhase=5`，`Ath10kPortAuthorized=Yes`
  - `en3` 通过 DHCP 获取 `192.168.1.11`
  - `curl --interface en3 https://www.apple.com/` 连续返回 HTTP 200
  - 链路为 HT20-1SS，RSSI 约 -45 dBm
- v0.0.100 源码验证：
  - `python3 -m unittest discover -s tests -p 'test_*.py' -v`：18 项通过
  - `tests/run-tests.sh`：完整主机回归通过
  - `make clean && make kext`：通过
  - kext 符号检查：通过
  - `make app`：通过
- v0.0.101 真机验证（2026-11-07）：
  - 5 GHz DS Parameter Set 修复生效，扫描结果中出现 `铁塔之巅_5G`（BSSID `64:09:80:12:5d:f5`，Ch 44，SNR 44）
  - 当前连接 `铁塔之巅_5G`：`Ath10kConnectionState=connected-wpa2`，`HT40-1SS`，TX/RX rate 约 150 Mbps，noise 约 -98 dBm
  - 网关 `192.168.1.1` 20 次 ping 0% 丢包，HTTPS 请求返回 HTTP 200
  - 首次连接曾在 2.4 GHz Ch 11 被 AP 主动 Deauth：`Ath10kConnectionMLMESubtype=192`（Deauth）、`Ath10kConnectionMLMEReason=3`、`Ath10kConnectionMLMEChannel=11`，驱动记录为 `Ath10kConnectionLastDisconnectReason=ap-disconnected`
  - 该次断开不是 beacon 超时：`Ath10kStationBeaconMisses=0`，且断开时链路已到 `stage=10`（Connected）、security phase 5
  - 随后切换到 5 GHz `铁塔之巅_5G`，`Ath10kDisconnectCount` 保持为 2，未再增长
- Ath10k Connectivity 0.1.13：
  - 修正窗口标题中的旧版本号
  - 连接失败后会清除残留的 `_session` 状态并重新启用 Connect；只有驱动仍在进行部分关联时才发送 Cancel，不再直接 Abort 整个 WMI runtime
- v0.0.101 修复 5 GHz 网络发现：
  - 部分 5 GHz AP 仍会携带 DS Parameter Set，且主信道号大于 14
  - 旧解析器把这类信标整帧判为 `ds-parameter` 错误并丢弃
  - 新解析器允许与接收信道同频段的 DS 信道，同时保留 DS/HT 信道冲突检查
  - 已在主机侧加入 5 GHz DS Parameter Set 回归测试
- v0.0.102 开机扫描优化（2026-11-07）：
  - 首次开机扫描由 38 信道 × 500 ms（约 19 秒）缩短为 22 个可连接非 DFS 信道 × 200 ms（约 4.4–5 秒）
  - 快速扫描覆盖 2.4 GHz 1–13 和 5 GHz 36/40/44/48/149/153/157/161/165
  - 手动 Scan 仍执行完整 38 信道、500 ms 扫描，DFS 信道仍可被发现，但驱动不能连接 DFS
  - App 启动时若驱动首次扫描已完成，直接复用结果，不再立即发起第二次完整扫描
  - 新增诊断属性：`Ath10kWMIScanMode`、`Ath10kWMIScanFastDwellMs`、`Ath10kWMIScanFastChannelCount`、`Ath10kWMIScanActiveDwellMs`
  - 首次快速扫描后 `Ath10kWMIScanMode=initial-fast`、`Ath10kWMIScanActiveDwellMs=200`；`Ath10kWMIScanAllChannelsObserved=No` 属于预期，因为本次只访问 22 个非 DFS 信道。手动 Scan 后应变为 `manual-full` / `500`，并覆盖全部 38 个信道
  - 主机侧新增快速/完整扫描帧、信道集合和 dwell 时间回归测试

### 尚待确认

- 多次重启后的稳定性
- 休眠/唤醒后的恢复和自动重连
- 2.4 GHz AP 主动 Deauth 的复现条件与路由器侧原因
- 其他 AP、5 GHz 和 DFS 信道下的长期稳定性

## 构建

在目标 Mac 的项目目录中执行：

```sh
python3 scripts/prepare-firmware.py
make clean
make kext
make app
```

产物：

- `build/out/Ath10kMac.kext`
- `build/Ath10k Connectivity.app`

快速核对：

```sh
plutil -p build/out/Ath10kMac.kext/Contents/Info.plist | grep -E 'CFBundleVersion|CFBundleShortVersionString|IOPCIMatch'
xcrun dwarfdump --uuid build/out/Ath10kMac.kext/Contents/MacOS/Ath10kMac
strings -a build/out/Ath10kMac.kext/Contents/MacOS/Ath10kMac | grep -E '144d|4133|cb4aa479|0\.0\.102'
```

预期关键值：

- `CFBundleVersion = 0.0.102`
- `IOPCIMatch = 0x0042168C`
- 二进制中包含 `subsystem-vendor=144d`、`subsystem-device=4133`
- 二进制中包含 Samsung board SHA-256 前缀 `cb4aa479`

## OpenCore 安装

> 第一次测试请使用可恢复的 USB EFI，并先备份当前可启动的内部 EFI。替换错误 EFI 可能导致无法启动。不要在无法进入恢复环境时直接覆盖唯一 EFI。

1. 关闭 Ath10k Connectivity。
2. 确认内部 EFI 分区设备号：

   ```sh
   diskutil list
   ```

3. 挂载 EFI（示例为 `disk0s1`，请按实际输出替换）：

   ```sh
   sudo diskutil mount disk0s1
   ```

4. 备份整个 EFI 目录：

   ```sh
   ditto "/Volumes/EFI" "$HOME/Desktop/EFI-backup-$(date +%Y%m%d-%H%M%S)"
   ```

5. 替换完整 kext bundle（不要只覆盖部分文件）：

   ```sh
   rm -rf "/Volumes/EFI/EFI/OC/Kexts/Ath10kMac.kext"
   ditto "build/out/Ath10kMac.kext" "/Volumes/EFI/EFI/OC/Kexts/Ath10kMac.kext"
   ```

6. 打开 `EFI/OC/config.plist`，确认 `Kernel -> Add` 中已有条目仍为：

   - `BundlePath = Ath10kMac.kext`
   - `ExecutablePath = Contents/MacOS/Ath10kMac`
   - `PlistPath = Contents/Info.plist`
   - `Arch = x86_64`
   - `Enabled = true`

   如果原来已有旧版 Ath10kMac 条目，保持路径不变，只替换 kext 文件即可。

7. 使用与当前 OpenCore 版本匹配的 `ocvalidate` 检查 `config.plist`。不要用不同版本的 OpenCore 工具直接改写配置。
8. 重新启动。第一次可以先用 USB EFI 启动，在确认新 kext 能加载后再替换内部 EFI。

## 加载后的验证

先确认驱动版本和 board 选择：

```sh
ioreg -c Ath10kPCIDevice -r -l -w0 | grep -E \
  'Ath10kDriverVersion|Ath10kChipRevision|Ath10kTargetRevision|Ath10kPreparedBoardKey|Ath10kPreparedBoardSelection|Ath10kPreparedBoardSHA256|Ath10kRuntimeStatus|Ath10kPowerStatus'
```

应看到：

- `Ath10kDriverVersion = 0.0.101`
- `Ath10kChipRevision = 1`
- `Ath10kTargetRevision = hw1.1`
- `Ath10kPreparedBoardKey = ...144d...4133`
- `Ath10kPreparedBoardSelection = board-api2-exact`
- `Ath10kPreparedBoardSHA256 = cb4aa479...`
- `Ath10kRuntimeStatus` 进入 active/WMI worker 状态

然后打开 `build/Ath10k Connectivity.app`：

1. 点击 Scan，确认能看到周围网络。
2. 选择自己的 SSID，输入 WPA2 密码并连接。
3. 确认 macOS 的 Ethernet 风格接口通过 DHCP 获得 IPv4。
4. 用接口名做连通性测试（`enX` 以实际为准）：

   ```sh
   ifconfig enX
   ping -c 4 -I enX 1.1.1.1
   ```

5. 测试断开/重连，再测试一次休眠/唤醒。

完整诊断可以运行：

```sh
sudo sh scripts/test-runtime.sh > "$HOME/Desktop/ath10k-runtime.txt"
```

不要把未脱敏的 SSID、BSSID、MAC、IP、路由器密码或 EFI/config.plist 上传到公开 issue。

## 已知限制

- 仅支持客户端模式；不支持 AP、AirDrop、AWDL。
- WPA2-Personal：pairwise 使用 AES/CCMP；组密钥可以使用 AES/CCMP 或 TKIP。
- 不支持 WPA3/SAE、强制 PMF/802.11w、TKIP pairwise、WPA/WPA2 Enterprise/EAP。
- 不支持 DFS 信道。
- 驱动仍是实验性 kext，错误配置可能导致 kernel panic 或无法启动。
- 不要用其他 QCA9377 子系统的 board 数据替代 Samsung `144d:4133` 的精确 board。

## 回滚

如果新 kext 无法启动或出现不稳定：

1. 关机，使用 USB EFI 启动。
2. 挂载内部 EFI，删除新 kext，恢复备份中的 `EFI/OC/Kexts/Ath10kMac.kext`。
3. 或者暂时在 `Kernel -> Add` 中把 Ath10kMac 条目设为 `Enabled = false`。
4. 重启后确认系统恢复，再收集 `Ath10kRuntimeStatus`、kernel log 和 OpenCore 配置（脱敏后）排查。

## 本地产物

构建脚本会生成本地分发包，位置形如：

`build/dist/Ath10kMac-0.0.101-QCA9377-Samsung.zip`

该包未经过 Apple 公证，仅用于本机 OpenCore 测试。压缩包内的 `SHA256SUMS` 可用于核对 kext 和 App 是否完整。
