# Windows 编译与串口烧录

在本目录打开 PowerShell。烧录前把 STM32 BOOT0 设为1并按 RESET，完成后设回0并再按 RESET。

```powershell
# 编译成功后才烧录；失败会终止
.\flash.ps1 -Build -Port COM14
```

也可分开执行，确认编译成功后再运行第二句：

```powershell
.\build.ps1
.\flash.ps1 -Port COM14
```

产物为 `build/dual_link/hirobot_dual.bin` 和 `.elf`。两块 STM32 使用同一份程序，I²C 使用板载 PB8=SCL、PB9=SDA，SPI 仍为 PB3/PB4/PB5、PD7 CS。

本机默认 Python 为 `D:\MOCE\runtime\python.exe`，编译器目录为 `D:\MOCEworkplace\tools\st-windows\compiler\bin`，可用 `-Python`、`-Compiler` 覆盖。烧录依赖默认复用相邻 `hirobot-ecw/.cache/stm32loader-deps` 中已有的 stm32loader 1.0.0、pyserial 3.5、progress 1.6.1，也可用 `-LoaderPath` 指定准备好的依赖目录；脚本不会自动安装依赖。

烧录前校验构建清单、镜像哈希、向量表、芯片ID，备份扇区0后才擦除写入，并回读校验。备份及本次写入快照在 `build/backups/`。当前工具仅支持不超过16KiB的扇区0固件。编译失败会删除旧构建清单，防止继续误烧旧文件。串口握手失败时不会擦除或写入。

只验证构建和镜像、不打开串口：

```powershell
.\flash.ps1 -Build -Port COM14 -CheckOnly
```

`make`、`flash.sh` 保留原 macOS 工作流，不用于本机 Windows 操作。
