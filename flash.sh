#!/bin/sh
# 通过 CH340 和芯片内置 USART1 Bootloader 烧录。
# 下载前：BOOT0 接到 3.3V，按一次 RESET。
# 下载后：BOOT0 拨回 GND，再按一次 RESET。
# 验收：串口打开 /dev/cu.usbserial-1110，115200 8N1。
# 当前 main 跑两路舵机：PA2 TIM2_CH3、PA3 TIM2_CH4，串口应看到 pulse N。
# 两路都是 50Hz PWM，脉宽在 500~2500us 间一起往复。看输出时用 8N1。
# Bootloader 的偶校验只在本次下载里由编程器处理。
set -eu

cd "$(dirname "$0")"

PORT="${PORT:-/dev/cu.usbserial-1110}"
PROG=$(find /Applications/STM32CubeIDE.app/Contents/Eclipse/plugins \
    -path '*cubeprogrammer.macosaarch64*/tools/bin/STM32_Programmer_CLI' \
    -type f 2>/dev/null | head -1)

if [ -z "$PROG" ]; then
    echo "找不到 STM32CubeIDE 自带的 STM32_Programmer_CLI" >&2
    exit 1
fi

if [ ! -e "$PORT" ]; then
    echo "串口不存在：$PORT" >&2
    echo "插上板子后执行 ls /dev/cu.usbserial*，再用 PORT=那个设备名 ./flash.sh" >&2
    exit 1
fi

if [ ! -f build/hello.bin ]; then
    echo "还没有 build/hello.bin，请先在本目录执行 make" >&2
    exit 1
fi

echo "请确认已经把 BOOT0 接到 3.3V，并按过一次 RESET。"
echo "烧录端口：$PORT    文件：build/hello.bin -> 0x08000000"
echo "完成后把 BOOT0 拨回 GND，再按 RESET，用 115200 8N1 打开这个串口看 pulse。"

"$PROG" -c port="$PORT" br=115200 P=EVEN \
    -w build/hello.bin 0x08000000 -v -rst
