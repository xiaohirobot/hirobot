# hirobot

STM32F407 双接口舵机固件。两块 STM32 使用同一份程序，由 ECW 按命令选择通信通道：

| ECW 命令 | 目标 |
|---|---|
| `11,a,b` | SPI 连接的 STM32 两路舵机 |
| `12,a,b` | I²C 连接的 STM32 两路舵机 |
| `13,a,b` | ECW 本地两路舵机，不经过 STM32 |

`a,b` 为两路相对转角，单位为度。`1,a,b` 和 `2,a,b` 分别用于 SPI、I²C 板的加法诊断。

## Windows 编译与烧录

在本目录打开 PowerShell，先把待烧录 STM32 的 **BOOT0 设为1，并按一次 RESET**，然后执行：

```powershell
.\flash.ps1 -Build -Port COM14
```

此命令先编译，成功后才烧录并回读校验；编译失败会停止。将 `COM14` 换成实际下载串口。成功后将 **BOOT0 恢复为0，再按 RESET** 运行程序。

也可以分开执行，确认编译成功后再烧录：

```powershell
.\build.ps1
.\flash.ps1 -Port COM14
```

只编译并检查镜像，不连接串口、不烧录：

```powershell
.\flash.ps1 -Build -Port COM14 -CheckOnly
```

产物为 `build/dual_link/hirobot_dual.bin` 和 `hirobot_dual.elf`。工具默认使用本机的 Python、ARM 编译器及相邻 ECW 工程中的烧录依赖；自定义路径、依赖和备份说明见 [Windows 详细说明](WINDOWS.md)。

## STM32 接线

| 功能 | STM32 芯片引脚 |
|---|---|
| SPI SCK / MISO / MOSI | PB3 / PB4 / PB5 |
| SPI CS | PD7 |
| I²C SCL / SDA | **PB8 / PB9** |
| 两路舵机信号 | PA2 / PA3 |

I²C 7位地址为 `0x32`，速率100kHz。两端共地，SCL/SDA 需要上拉至3.3V；接口针序按板上标识和套件映射确认。两块相同固件的 STM32 不应同时接到同一条 I²C 总线，因为地址相同。

## macOS

现有 `Makefile` 和 `flash.sh` 使用 macOS STM32CubeIDE 工具路径。在具备对应工具链的 macOS 上可执行：

```bash
make && ./flash.sh
```

该流程使用 `build/hirobot.bin`，与 Windows 的产物路径不同；`flash.sh` 本身不编译。BOOT0 和 RESET 的操作同上。
