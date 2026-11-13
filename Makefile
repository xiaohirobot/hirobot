GNU_BIN := $(firstword $(wildcard /Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*.macosaarch64_*/tools/bin))
ifeq ($(GNU_BIN),)
$(error 找不到 STM32CubeIDE 自带的 arm-none-eabi-gcc)
endif

CC      := $(GNU_BIN)/arm-none-eabi-gcc
OBJCOPY := $(GNU_BIN)/arm-none-eabi-objcopy
SIZE    := $(GNU_BIN)/arm-none-eabi-size

BUILD   := build
TARGET  := $(BUILD)/hirobot
LDSCRIPT := STM32F407VETX_FLASH.ld

CFLAGS  := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding \
           -Wall -Wextra -g -O2 -ffunction-sections -fdata-sections \
           -Isrc
LDFLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib \
           -T $(LDSCRIPT) -Wl,--gc-sections -lgcc

SRCS    := src/startup.c src/main.c \
           src/components/delay.c src/components/uart3.c src/components/servo.c \
           src/components/spi1.c \
           src/demos/demo_uart3_log.c src/demos/demo_servo.c src/demos/demo_spi.c \
           src/demos/demo_servo_link.c
OBJS    := $(addprefix $(BUILD)/,$(notdir $(SRCS:.c=.o)))

vpath %.c src src/components src/demos

.PHONY: all clean

all: $(TARGET).bin

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD)
