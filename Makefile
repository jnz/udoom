# µDoom
TARGET = udoom

## Makefile to build the firmware for STM32F769 Eval Board
#
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

TARGET_COMPILER ?= gcc
# default compiler optimization level:
export OPTIMIZE_LEVEL ?= 0
APP_CPP_FLAGS   += -g3 -fno-builtin

APPNAME         := firmware
OBJDIR          := build
CMSIS_DIR       := ST/Drivers/CMSIS
HAL_DIR         := ST/Drivers/STM32F7xx_HAL_Driver
BSP_DIR         := ST/Drivers/BSP/STM32F769I-Discovery
ADDITIONAL_DIR  := ST/STM32F769I-Discovery
COMP_DIR        := ST/Drivers/BSP/Components

APP_CPP_FLAGS   += -fno-strict-aliasing -fno-math-errno
ODFLAGS         := -x --syms

CROSS_COMPILE   ?= arm-none-eabi-
ARCH_FLAGS      := -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 -mlittle-endian --specs=nosys.specs
LINKER_SCRIPT   := $(ADDITIONAL_DIR)/STM32F769NIHx_FLASH.ld
APP_CPP_FLAGS   += -DUSE_HAL_DRIVER -DSTM32F769xx
APP_CPP_FLAGS   += -DUSE_FULL_LL_DRIVER
APP_CPP_FLAGS   += -nostdlib -ffreestanding
APP_CPP_FLAGS   += -D_GNU_SOURCE

# -MMD: to autogenerate dependencies for make
# -MP: These dummy rules work around errors make gives if you remove header
#      files without updating the Makefile to match.
# -MF: When used with the driver options -MD or -MMD, -MF overrides the default
#      dependency output file.
# -fno-common: This has the effect that if the same variable is declared
#              (without extern) in two different compilations, you get a
#              multiple-definition error when you link them.
# -fmessage-length=n: If n is zero, then no line-wrapping is done; each error
#                     message appears on a single line.
# -fno-math-errno: Otherwise the default sqrt() function is used and we won't
#                  get the ARM vsqrt.f32 (14 cycle) instruction (but not in O0)
# --specs=nano.specs: Use newlib nano libc
# --specs=nosys.specs: semihosting disabled

# GCC compiler warnings
GCC_STACK_WARNING_BYTES := 4096
WARNING_CHECKS  := -Wall
WARNING_CHECKS  += -Wframe-larger-than=$(GCC_STACK_WARNING_BYTES)
WARNING_CHECKS  += -Wstack-usage=$(GCC_STACK_WARNING_BYTES)
WARNING_CHECKS  += -Wdouble-promotion
WARNING_CHECKS  += -Wpointer-arith
WARNING_CHECKS  += -Wno-format # re-enable this later, but for now we trust the doom printfs
WARNING_CHECKS  += -Wmissing-include-dirs
WARNING_CHECKS  += -Wwrite-strings
WARNING_CHECKS  += -Wlogical-op
WARNING_CHECKS  += -Wunreachable-code
WARNING_CHECKS  += -Wno-unknown-pragmas
WARNING_CHECKS  += -Wno-discarded-qualifiers
WARNING_CHECKS  += -Wvla
WARNING_CHECKS  += -Wdate-time

default: all

# App source directories
APP_SUBDIRS += \
	./src \
	./src/storage \
	./doomgeneric \
	./doomgeneric/stm32f7 \
	$(HAL_DIR)/Src \
	$(ADDITIONAL_DIR) \
	$(BSP_DIR) \
	$(COMP_DIR)/nt35510/ \
	$(COMP_DIR)/otm8009a/

# Note: Display driver NT35510 or OTM8009A is dynamically selected at runtime.

# Include directories
APP_INCLUDE_PATH += \
  -I./inc \
  -I./doomgeneric \
  -I$(CMSIS_DIR)/Include \
  -I$(CMSIS_DIR)/Device/ST/STM32F7xx/Include \
  -I$(HAL_DIR)/Inc \
  -I$(BSP_DIR) \
  -I$(ADDITIONAL_DIR)


S_STARTUP := startup_stm32f769xx
S_SRC += $(ADDITIONAL_DIR)/$(S_STARTUP).s

LIBRARIES := -lm

# =======================================================================

COMPILER = $(CROSS_COMPILE)$(TARGET_COMPILER)

CPP_FLAGS = $(APP_CPP_FLAGS)
COMPILER_FLAGS  = -O$(OPTIMIZE_LEVEL)
# -c: Compile without linking:
COMPILER_FLAGS  += ${WARNING_CHECKS} -c
COMPILER_FLAGS  += -MMD ${CPP_FLAGS} ${ARCH_FLAGS}
ASM_FLAGS       = -x assembler-with-cpp ${COMPILER_FLAGS}
LINK_FLAGS      = -Wl,--gc-sections -T $(LINKER_SCRIPT) ${ARCH_FLAGS} -Wl,-Map=${APPNAME}.map
LINK_FLAGS      += -nostartfiles -nostdlib -nodefaultlibs

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
	$(RM) $(APPNAME).bin $(APPNAME).hex
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
