# USBInfo

一个使用 **VC / C++20 / WTL 10 / Win32** 编写的 USB 拓扑、HUB、存储、SMART 与读写测速工具。

- 作者：**anpho & Deepseek**
- 项目主页：**https://github.com/anpho/USBinfo**

![USBInfo 主界面](docs/screenshot.png)

可以直接用 Visual Studio 打开 `USBInfo.sln`，也可以使用 MSBuild 命令行构建。

## 0. 多语言

支持 8 种界面语言：

- 简体中文
- English
- Français
- Deutsch
- Español
- 日本語
- 한국어
- Русский

启动时根据 Windows 显示语言自动选择；也可以随时通过 **查看 → Language** 手动切换（含“自动（系统）”），切换后立即刷新菜单、树、详情、状态栏和测速页，无需重启、无需重新扫描设备。语言选择会保存到当前用户配置（受限环境无法写注册表时仅当次生效）。

文本资源集中在 `src/LanguageCore.inc`、`src/LanguageUi.inc`、`src/LanguageData.inc`；新增字符串时用 `Tr(L"中文")` 包一层，并在对应资源表补一行即可。

**关于** 对话框包含作者、项目主页；**帮助 → 项目主页** 可直接打开 GitHub。

## 1. 功能

### 左侧 USB 级联树

- 计算机 → USB 主机控制器 → 根集线器 → 外接 HUB → USB 设备 → 复合接口 / 存储 / 端口(线缆)
- 树节点使用**短标题**，只显示产品名、Hub 端口数、HUB 芯片短名、协商速度、存储标记；完整版本号、HCI 长描述、VID/PID、驱动信息等全部放到右侧详情页。
- 右侧内容区约占窗口 61.8%（左侧 38.2%），随窗口缩放保持比例。
- 支持展开全部、折叠全部、刷新；刷新在后台线程执行，界面不卡死。
- 复合设备节点下会显示 `&MI_xx` 功能接口。
- USB 存储设备下会生成一个“存储”子节点，选中后即可查看容量/SMART/测速。
- 只有在系统实际报告了 USB-C/Billboard/线缆信息时，才生成“端口/线缆”子节点；没有信息时不显示占位内容。
- 界面顶部/标题区域为白色背景；没有“USB 拓扑”“USB 根集线器”等灰色底块。

### 内容交互

- 右侧“设备信息”和“SMART / 健康”列表支持右键：
  - 复制值
  - 复制整行
  - 复制全部
- 驱动相关行（驱动版本、驱动键、INF、驱动文件、服务等）额外支持：
  - **定位到驱动文件**（在资源管理器中选中 `.sys`/驱动文件）
  - **打开 INF 所在目录**
- 顶部不再放置操作按钮，统一合并到菜单：
  - **操作 → 测速**
  - **操作 → 复制当前节点信息**
  - **操作 → 导出报告**（文件菜单也保留导出）
- 没有测速功能的节点，菜单中的“测速”置灰；右侧列表不会出现横向滚动条。

### HUB 与设备信息

尽量读取并展示：

- 品牌、产品/型号、制造商、序列号
- VID/PID、USB 版本（1.1/2.0/3.x）、设备类/子类/协议
- 配置描述符、接口、端点数、供电方式、最大电流
- BOS 能力：USB 2.0 Extension、SuperSpeed、SuperSpeed+、Billboard 等
- 协商链路速度与支持的协议（Low/Full/High/SuperSpeed/SuperSpeed+）
- USB HUB 类型、下行端口数、上电延迟、Hub 电流、Hub 能力、Hub 芯片
- 驱动服务、驱动键、驱动描述、版本、日期、提供商、INF、硬件 ID、兼容 ID、安装/最后到达时间、问题代码
- PnP 位置信息、端口地址、容器 ID、设备实例 ID

### USB 存储

- 磁盘型号、厂商、固件版本、序列号
- 容量、可用空间、逻辑/物理扇区大小、分区样式
- 卷列表、卷标、文件系统、可用空间、只读状态
- 总线类型、可移动介质、热插拔
- USB 桥接/主控芯片（按 VID/PID 推断，标注“推断”）
- 存储颗粒：**通常无法通过标准 USB 大容量存储协议读取**，会明确显示“无法读取”，不会伪造
- SMART / 健康数据（设备与驱动支持时）：
  - `IOCTL_STORAGE_PREDICT_FAILURE`
  - ATA SMART READ DATA / READ THRESHOLDS（`IOCTL_ATA_PASS_THROUGH`）
  - SAT ATA PASS-THROUGH (SCSI 0x85/0xA1)，用于 USB-SATA/UASP 桥接器
  - NVMe SMART / Health Information Log（适配器支持时）
  - 属性表：ID、名称、当前值、最差值、阈值、原始值、状态
  - 汇总：温度、通电时间、通电次数、重映射/待映射/不可修正扇区、CRC、磨损、剩余寿命等
- SMART 通常需要管理员权限；USB 闪存盘往往不支持 ATA SMART，工具会说明原因。
- **没有 SMART/健康数据时，右侧不会显示“SMART / 健康”Tab**；没有可写卷的存储节点不会显示“测速”Tab。
- 切换到另一个 USB 存储设备时，测速结果、曲线和进度会清空，避免串用上一台设备的数据。

### 测速

在存储节点上点击“测速”进入测速页。支持：

- 测试卷选择（自动列出该 USB 存储的卷）
- 大文件大小：128 MB / 256 MB / 512 MB / 1024 MB
- 固定执行 8 项测试，明确覆盖用户要求的四类：
  1. 大文件顺序写入（1 MiB 块）
  2. 大文件顺序读取（1 MiB 块）
  3. 4K 随机写入
  4. 4K 随机读取
  5. 4K 顺序写入
  6. 4K 顺序读取
  7. 1 MiB 随机写入
  8. 1 MiB 随机读取
- **实时吞吐曲线**：白底、网格、蓝色折线，显示阶段标题、当前 MB/s、平均 MB/s，类似任务管理器磁盘曲线；每个阶段自动清空并重新绘制。
- 结果展示：数据量、耗时、MB/s、IOPS、平均延迟、状态
- 后台线程执行，支持取消；进度条实时更新
- 结果表支持右键“复制当前行 / 复制全部结果”
- **只写文件系统上的临时文件，不写裸盘**
- 临时文件名为 `USBInfo_bench_<pid>_<tick>.tmp`，测试结束后自动删除
- 优先使用 `FILE_FLAG_NO_BUFFERING` 绕过系统缓存；文件系统不支持时自动回退并提示
- 开始前会弹出确认框，明确测试卷、大小、写入量和风险

### 报告

- “导出报告”生成 UTF-8 文本报告，包含拓扑树、全部节点详情、警告与限制。
- 无界面模式：

```text
USBInfo.exe --report [输出路径]
```

  - 不带路径时默认输出到桌面 `USBInfo_Report.txt`
  - 适合脚本采集、远程支持、对比不同机器

### 权限

默认 `asInvoker` 启动：

- 标准用户可以查看大部分 PnP、驱动、卷、容量信息，但物理磁盘 IOCTL、SMART、部分 HUB 描述符可能不可读。
- 通过菜单 **查看 → 以管理员身份重启** 可以重新以管理员身份启动（UAC）。
- 状态栏和页面顶部会明确显示当前权限和限制。

## 2. 构建

### 环境

本工程在以下环境构建通过：

- Windows 10/11 x64
- Visual Studio 2026 Build Tools（MSVC 19.51，PlatformToolset `v145`）
- Windows SDK `10.0.28000.0`
- WTL 10（已放在 `third_party/wtl`，无需另行安装）
- CMake 不是必需的

如果你使用 **Visual Studio 2022**，请把 `USBInfo.vcxproj` 中的：

```xml
<PlatformToolset>v145</PlatformToolset>
```

改为：

```xml
<PlatformToolset>v143</PlatformToolset>
```

并确认安装了 ATL（WTL 依赖 ATL 头文件）。

### 命令行构建

在 “x64 Native Tools Command Prompt for VS” 中：

```bat
msbuild USBInfo.sln /m /p:Configuration=Release /p:Platform=x64
```

或在普通 PowerShell 中：

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe" `
  USBInfo.sln /m /p:Configuration=Release /p:Platform=x64
```

输出：

```text
bin\Release\USBInfo.exe
```

Release 使用静态 CRT（`/MT`），生成物不依赖 VC++ 运行库，可直接拷贝运行。

调试构建：

```bat
msbuild USBInfo.sln /m /p:Configuration=Debug /p:Platform=x64
```

## 3. 工程结构

```text
USBInfo.sln
USBInfo.vcxproj
src/
  main.cpp                 WinMain / --report 模式
  MainFrame.*              主窗口、菜单、异步扫描、导出
  TreePane.*               左侧 USB 级联树、节点图标
  DetailPane.*             右侧详情、分组属性、SMART 页
  BenchmarkPane.*          测速页、卷/大小选择、进度和结果
  ThroughputChart.*        任务管理器风格实时吞吐曲线
  UsbEnumerator.*          SetupAPI/CM + HUB IOCTL 枚举与描述符
  StorageEnumerator.*      磁盘/卷/USB 桥接/容量与 SMART 关联
  Smart.*                  SMART/ATA/SCSI/NVMe 读取
  Benchmark.*              文件级测速引擎
  Model.*                  数据模型、详情行、文本报告
  Language.*               多语言检测、切换、Tr() 翻译查表
  LanguageCore.inc         菜单/状态/About/TreePane 翻译
  LanguageUi.inc           详情/SMART/测速/曲线/右键菜单翻译
  LanguageData.inc         设备详情行/单位/报告/SMART/警告翻译
  DeviceProps.*            PnP/注册表/设备属性封装
  ChipDb.*                 VID/PID 芯片推断库
  Util.*                   格式化与通用工具
  USBInfo.rc / app.manifest / usbinfo.ico
third_party/wtl/           WTL 10 头文件（MS-PL）
docs/screenshot.png        界面截图
```

## 4. 技术实现

### USB 拓扑

1. `SetupDiGetClassDevs(..., DIGCF_PRESENT | DIGCF_ALLCLASSES)` 枚举当前设备。
2. 过滤 `USB\`、`USB4\`、以及 USB 类的 PCI 主机控制器。
3. 使用 `CM_Get_Parent` / `CM_Get_Device_ID` 构建 PnP 父子关系。
4. 用 `GUID_DEVINTERFACE_USB_HUB` 找到 HUB 设备接口，打开后发送：
   - `IOCTL_USB_GET_NODE_INFORMATION`
   - `IOCTL_USB_GET_HUB_INFORMATION_EX`
   - `IOCTL_USB_GET_HUB_CAPABILITIES_EX`
   - `IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX(_V2)`
   - `IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME`
   - `IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION`
   - `IOCTL_USB_GET_PORT_CONNECTOR_PROPERTIES`
5. 通过 HUB 端口返回的 Device Descriptor、Configuration Descriptor、BOS、字符串描述符补齐设备详情。
6. 通过 DriverKey 或 (父 HUB, 端口地址) 把 HUB 端口与 PnP 节点匹配，避免重复设备。

### 线缆 / 端口信息

Windows 的公开 USB API **不提供**线缆 E-Marker、CC 线序、线材阻抗等电气信息。只有在系统实际报告以下信息时，工具才显示“端口/线缆”内容：

- `USB_PORT_CONNECTOR_PROPERTIES`：
  - 是否为 USB Type-C 连接器
  - 是否外部可插拔、是否支持调试
  - Companion Port / Companion Hub（USB 2.0 与 3.x 共享物理端口）
- USB BOS 中的 Billboard Descriptor：
  - Alternate Mode 数量与 SVID

如果标准 USB 描述符没有提供任何这类信息，界面**不会显示“线缆 / 端口”分组或“未读取”占位**；只有实际 Type-C/Billboard 信息才展示，且不再写“标准 USB 描述符不提供……”这类空内容。

### 存储关联

- 枚举磁盘类设备（`GUID_DEVCLASS_DISKDRIVE`）。
- 通过 `CM_Get_Parent` 找到 USB 祖先，或用 Container ID 与 USB 节点匹配。
- 通过 `FindFirstVolume`、`GetVolumePathNamesForVolumeName`、`IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS` 把卷映射到磁盘号。
- 物理磁盘可打开时读取：
  - `IOCTL_STORAGE_QUERY_PROPERTY`（StorageDeviceProperty / AdapterProperty / AccessAlignment）
  - `IOCTL_DISK_GET_LENGTH_INFO`
  - `IOCTL_DISK_GET_DRIVE_GEOMETRY_EX`
  - `IOCTL_STORAGE_GET_HOTPLUG_INFO`
  - `IOCTL_DISK_GET_DRIVE_LAYOUT_EX`
- 标准用户无法打开 `\\.\PhysicalDriveN` 时，仍能通过卷 API/USBSTOR 注册表显示容量、卷和型号，并提示 SMART 不可用。

### SMART

详见 `src/Smart.cpp`。支持三类路径，失败时会逐级回退：

1. `IOCTL_STORAGE_PREDICT_FAILURE`
2. `IOCTL_ATA_PASS_THROUGH`：ATA SMART READ DATA (0xB0/0xD0)、READ THRESHOLDS (0xD1)
3. `IOCTL_SCSI_PASS_THROUGH_DIRECT`：SAT ATA PASS-THROUGH (16/12)
4. NVMe SMART / Health Information Log（`IOCTL_STORAGE_QUERY_PROPERTY` + protocol specific）

很多 USB-SATA 桥接器只透传 SCSI，不透传 ATA SMART；USB 闪存盘通常完全没有 SMART。工具会显示“不可用”及原因，不会伪造数据。

### 测速安全设计

- 只通过 `CreateFile` 创建/写入卷上的临时文件，**不打开裸磁盘写入**。
- 打开前弹出确认框，明确卷、文件大小、测试类型和写入风险。
- 检查可用空间（测试大小 + 64 MiB 余量）。
- 使用 `FILE_FLAG_NO_BUFFERING`，按扇区对齐；不支持时回退到普通文件 I/O 并提示。
- 写入阶段结束后 `FlushFileBuffers`，再统计时间。
- 退出/取消/异常时 `TempBenchFile` 析构会关闭并删除临时文件。
- 结果受 USB 桥接器缓存、SLC 缓存、文件系统、设备温度影响；这是“实际文件读写速度”，不是芯片理论带宽。

## 5. 已在本机验证

本机（Windows 10 19044，标准用户）实际扫描到：

- 2 个 Intel xHCI 主机控制器
- 2 个根集线器（16 端口 / 5 端口）
- USB 复合摄像头、蓝牙、指纹
- Generic USB Flash Disk 14.84 GB（480 Mbps，FAT32，D:）

以下为在 SanDisk Extreme Pro 上的历史文件级测速示例（不同版本/环境会有波动）：

| 测试 | 速度 |
|---|---:|
| 大文件顺序写入 | 34.92 MB/s |
| 大文件顺序读取 | 310.75 MB/s |
| 4K 随机写入 | 10.90 MB/s / 2789 IOPS |
| 4K 随机读取 | 8.89 MB/s / 2275 IOPS |
| 4K 顺序写入 | 23.43 MB/s / 5999 IOPS |
| 4K 顺序读取 | 22.54 MB/s / 5770 IOPS |
| 1 MiB 随机写入 | 8.90 MB/s / 9 IOPS |
| 1 MiB 随机读取 | 268.72 MB/s / 269 IOPS |

这些数值只代表本次测试条件，换机器/换线/换文件系统结果会不同。

## 6. WTL 说明

- 使用 WTL 10 头文件（`third_party/wtl/Include`），许可证见 `third_party/wtl/MS-PL.txt`。
- 为了在 MSVC C++20 两阶段名字查找下编译，做了 1 处最小补丁：
  - `atlframe.h` 中 `UISetText(...)` 改为 `T::UISetText(...)`
  - 详见 `third_party/wtl/PATCHES.md`
- 工程使用 `ConformanceMode=false` 以兼容 WTL 10 的旧式模板代码；语言标准仍为 `stdcpp20`。

## 7. 已知限制

- 线缆 E-Marker/CC/阻抗等电气信息：公开 Windows API 不提供。
- USB 桥接器背后的 NAND/闪存颗粒型号：标准 USB 大容量存储协议不暴露；需要厂商专用工具。
- 芯片型号根据 VID/PID 推断，不能保证 100% 准确；界面会显示“推断”来源。
- SMART 对 USB 设备依赖桥接器/驱动透传，不能保证所有设备可读。
- 测速结果受缓存、SLC 缓存、温度、文件系统和 USB 链路影响。
- 目前只构建 x64；未做 ARM64 和 Windows 7 兼容。
- 未实现 USB4/TB 的完整拓扑（USB4 节点只作为 USB4 设备显示；本机没有 USB4 设备可验证）。

## 8. 常见问题

**Q: 为什么没有“线缆 / 端口”内容？**  
A: 这是正常行为。标准 USB 描述符没有提供 Type-C/Billboard/线缆信息时，工具不显示该项，也不显示“未读取”占位；只有系统实际报告时才展示。

**Q: 为什么 SMART 显示不可用？**  
A: 三个常见原因：没有管理员权限；USB 桥接器不透传 SMART；设备是 USB 闪存盘，本身没有 ATA SMART。没有 SMART/健康数据时，“SMART / 健康”Tab 会被隐藏；以管理员身份运行且设备支持时才会出现。

**Q: 测速会不会损坏 U 盘？**  
A: 不会写裸盘，只写卷上的临时文件并自动删除。但测试会产生真实写入、占用空间、发热；请确保卷上有足够空间并避免在重要数据卷上做长时间测试。

**Q: 为什么小数据随机写比顺序写慢很多？**  
A: 这通常是 USB 闪存盘/桥接器的正常表现，尤其是 4 KiB 随机写触发 FTL 垃圾回收和写放大。结果本身是有参考价值的。

**Q: 如何切换语言？**  
A: 菜单 **查看 → Language**，可选“自动（系统）”或 8 种语言；切换后立即生效，不需要重启或重新扫描。启动时的默认值来自 Windows 显示语言。

## 9. 许可

MIT. 本项目代码可自由用于学习和内部工具。第三方 WTL 按 MS-PL 许可使用，见 `third_party/wtl/MS-PL.txt`。
