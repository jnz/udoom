TARGET = stm32f750_qspi_bootloader

# Call with "make Q=" to enable full path names
#
# TOOLCHAIN_ROOT path must be set.
# Create a config.mk with the following path to the cross compiler GCC:
#
# 	TOOLCHAIN_ROOT=/path/to/gcc-arm-none-eabi-XX.XX-XX/bin/
#

-include config.mk

ifndef TOOLCHAIN_ROOT
$(error TOOLCHAIN_ROOT is not defined. Please set TOOLCHAIN_ROOT in a config.mk file)
endif

Q ?= @
BOARD ?= STM32F7508_DK

# =======================================================================

# =======================================================================

APPNAME         := bootloader
OBJDIR          := build
CMSIS_DIR       := ../../Drivers/CMSIS
HAL_DIR         := ../../Drivers/STM32F7xx_HAL_Driver

# App source directories
APP_SUBDIRS += \
	./ \
	$(HAL_DIR)/Src \
	../../Drivers/BSP/STM32F7508-Discovery

# Include directories
APP_INCLUDE_PATH += \
  -I. \
  -I$(CMSIS_DIR)/Include \
  -I$(CMSIS_DIR)/Device/ST/STM32F7xx/Include \
  -I$(HAL_DIR)/Inc

# =======================================================================
# STM32F7508 Discovery Board
# =======================================================================

ifeq ($(BOARD),STM32F7508_DK)
LINKER_SCRIPT   := stm32f750vb_flash.ld
APP_CPP_FLAGS   += -DSTM32F750xx -DUSE_HAL_DRIVER -DUSE_FULL_LL_DRIVER

APP_INCLUDE_PATH += \
	-I../../Drivers/BSP/STM32F7508-Discovery \
	-I../../Drivers/BSP/Components/n25q128a


S_STARTUP := startup_stm32f750xx
S_SRC += $(S_STARTUP).s

endif

# =======================================================================
ifndef S_STARTUP
$(error Unknown or unsupported BOARD: $(BOARD))
endif
# =======================================================================

TARGET_COMPILER ?= gcc
# default compiler optimization level:
export OPTIMIZE_LEVEL ?= 2
APP_CPP_FLAGS   += -g3 -fno-builtin
APP_CPP_FLAGS   += -fno-strict-aliasing -fno-math-errno

ODFLAGS         := -x --syms

CROSS_COMPILE   ?= arm-none-eabi-
ARCH_FLAGS      += -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 -mlittle-endian --specs=nosys.specs -fno-common -ffreestanding
WARNING_CHECKS  := -Wall

default: all

LIBRARIES :=

# =======================================================================

COMPILER = $(CROSS_COMPILE)$(TARGET_COMPILER)

CPP_FLAGS = $(APP_CPP_FLAGS)
COMPILER_FLAGS  = -O$(OPTIMIZE_LEVEL)
COMPILER_FLAGS  += ${WARNING_CHECKS} -c
COMPILER_FLAGS  += -MMD ${CPP_FLAGS} ${ARCH_FLAGS}
ASM_FLAGS       = -x assembler-with-cpp ${COMPILER_FLAGS}
LINK_FLAGS      = -Wl,--gc-sections -T $(LINKER_SCRIPT) ${ARCH_FLAGS} -Wl,-Map=${APPNAME}.map
LINK_FLAGS      += -nostartfiles

OC              := ${TOOLCHAIN_ROOT}${CROSS_COMPILE}objcopy
OD              := ${TOOLCHAIN_ROOT}${CROSS_COMPILE}objdump
HEX             := ${OC} -O ihex   # intel .hex file output
BIN             := ${OC} -O binary # binary output
SIZEINFO        := ${TOOLCHAIN_ROOT}${CROSS_COMPILE}size
RM              := rm -f
CP              := cp
CC              := ${TOOLCHAIN_ROOT}${COMPILER}
LINK            := ${TOOLCHAIN_ROOT}${COMPILER}

APP_SRCS         = $(foreach dir, $(APP_SUBDIRS), $(wildcard $(dir)/*.c))
C_SRCS           = $(APP_SRCS)
VPATH            = $(APP_SUBDIRS)
OBJ_NAMES        = $(notdir $(C_SRCS))
OBJS             = $(addprefix $(OBJDIR)/,$(OBJ_NAMES:%.c=%.o))
OBJS             += $(OBJDIR)/$(S_STARTUP).o
C_DEPS           = $(OBJS:%.o=%.d)
C_INCLUDES       = $(APP_INCLUDE_PATH)

COMPILER_CMDLINE = -std=c99 $(COMPILER_FLAGS) $(C_INCLUDES)

$(OBJDIR)/$(S_STARTUP).o: $(S_SRC)
	@echo 'ASM: $<'
	$(Q)$(CC) $(ASM_FLAGS) $(C_INCLUDES) -o "$@" "$<"

$(OBJDIR)/%.o: %.c
	@echo 'CC: $<'
	$(Q)$(CC) $(COMPILER_CMDLINE) -o "$@" "$<"

EXECUTABLES += \
	${APPNAME}.elf \
	${APPNAME}.list \
	${APPNAME}.dmp \

# All Target
all: $(EXECUTABLES)

# Make sure that we recompile if a header file was changed
-include $(C_DEPS)

# App
${APPNAME}.elf: $(OBJS) ${LINKER_SCRIPT}
	@echo 'Object files: '$(OBJS)
	@echo 'Building target: $@ with '$(CC)
	@echo 'Building machine: '$(HOSTNAME)
	@echo 'Optimize level:' $(OPTIMIZE_LEVEL)
	$(Q)$(LINK) ${LINK_FLAGS} -o "$@" $(OBJS) $(LIBRARIES)
	@echo 'Finished building target: $@'
	$(Q)$(MAKE) --no-print-directory post-build

clean:
	$(RM) $(APPNAME).bin
	$(RM) $(EXECUTABLES) ${APPNAME}.map
	$(RM) $(OBJDIR)/*.d
	$(RM) $(OBJDIR)/*.o
	$(RM) $(OBJDIR)/*.dbo
	$(RM) $(OBJDIR)/*.lst

post-build:
	@echo "Creating ${APPNAME}.bin"
	$(Q)$(BIN) "${APPNAME}.elf" "${APPNAME}.bin"
	@echo "Creating ${APPNAME}.hex"
	$(Q)$(HEX) "${APPNAME}.elf" "${APPNAME}.hex"
	$(Q)$(SIZEINFO) "${APPNAME}.elf"

%.list: %.elf
	@echo "Assembler output $@"
	$(Q)$(OD) -S $< > $@

%.dmp: %.elf
	@echo "Creating dump file $@"
	$(Q)$(OD) $(ODFLAGS) $< > $@

flash: all
ifeq ($(OS),Windows_NT)
	@flash.bat
else
	@./flash.sh
endif

.FORCE:

.PHONY: all clean flash post-build

.SECONDARY: post-build
