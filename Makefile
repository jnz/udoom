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
BOARD ?= STM32F769I_DISCO

# =======================================================================

# =======================================================================

APPNAME         := firmware
OBJDIR          := build
CMSIS_DIR       := ST/Drivers/CMSIS
HAL_DIR         := ST/Drivers/STM32F7xx_HAL_Driver

# Optional WAD embed for STM32F7508_DK
WAD_INPUT := wad/DOOM1.WAD
WAD_OBJ := $(OBJDIR)/doom1wad.o

# App source directories
APP_SUBDIRS += \
	./src \
	./doomgeneric \
	./doomgeneric/stm32f7 \
	$(HAL_DIR)/Src \
	ST/STM32F7xx_shared

# Include directories
APP_INCLUDE_PATH += \
  -I./inc \
  -I./src/storage/FatFs \
  -I./doomgeneric \
  -I$(CMSIS_DIR)/Include \
  -I$(CMSIS_DIR)/Device/ST/STM32F7xx/Include \
  -I$(HAL_DIR)/Inc

# =======================================================================
# STM32F769I Discovery Board
# =======================================================================

ifeq ($(BOARD),STM32F769I_DISCO)

LINKER_SCRIPT   := ST/STM32F769I-Discovery/STM32F769NIHx_FLASH.ld
APP_CPP_FLAGS   += -DSTM32F769xx -DUSE_HAL_DRIVER -DUSE_FULL_LL_DRIVER
APP_SUBDIRS += \
	./src/storage \
	./src/storage/FatFs \
	ST/STM32F769I-Discovery \
	ST/Drivers/BSP/STM32F769I-Discovery \
	ST/Drivers/BSP/Components/nt35510/ \
	ST/Drivers/BSP/Components/otm8009a/

APP_INCLUDE_PATH += \
	-IST/Drivers/BSP/STM32F769I-Discovery \
	-IST/STM32F769I-Discovery

S_STARTUP := startup_stm32f769xx
S_SRC += ST/STM32F769I-Discovery/$(S_STARTUP).s

endif

# =======================================================================
# STM32F7508 Discovery Board
# =======================================================================

ifeq ($(BOARD),STM32F7508_DK)
LINKER_SCRIPT   := ST/STM32F7508-Discovery/STM32F750N8Hx_FLASH.ld
APP_CPP_FLAGS   += -DSTM32F750xx -DUSE_HAL_DRIVER -DUSE_FULL_LL_DRIVER -DWAD_EMBEDDED
APP_SUBDIRS += \
	ST/STM32F7508-Discovery \
	ST/Drivers/BSP/STM32F7508-Discovery \
	ST/Drivers/BSP/Components/rk043fn48h

APP_INCLUDE_PATH += \
	-IST/Drivers/BSP/STM32F7508-Discovery \
	-IST/STM32F7508-Discovery

S_STARTUP := startup_stm32f750xx
S_SRC += ST/STM32F7508-Discovery/$(S_STARTUP).s

WAD_ENABLED := 1

endif

# =======================================================================
ifndef S_STARTUP
$(error Unknown or unsupported BOARD: $(BOARD))
endif
# =======================================================================

TARGET_COMPILER ?= gcc
# default compiler optimization level:
export OPTIMIZE_LEVEL ?= g
APP_CPP_FLAGS   += -g3 -fno-builtin

APP_CPP_FLAGS   += -fno-strict-aliasing -fno-math-errno
ODFLAGS         := -x --syms

CROSS_COMPILE   ?= arm-none-eabi-
ARCH_FLAGS      += -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 -mlittle-endian --specs=nosys.specs
APP_CPP_FLAGS   += -nostdlib -ffreestanding
APP_CPP_FLAGS   += -D_DEFAULT_SOURCE  # only to enable strdup()

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
GCC_STACK_WARNING_BYTES := 1024
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

APP_SRCS         := $(foreach dir, $(APP_SUBDIRS), $(wildcard $(dir)/*.c))
C_SRCS           = $(APP_SRCS)
VPATH            = $(APP_SUBDIRS)
OBJ_NAMES        := $(notdir $(C_SRCS))
ALL_C_OBJS       := $(addprefix $(OBJDIR)/,$(OBJ_NAMES:%.c=%.o))
OBJS             += $(ALL_C_OBJS) $(OBJDIR)/$(S_STARTUP).o

# Add WAD object only for STM32F7508_DK board
ifeq ($(WAD_ENABLED),1)
OBJS += $(WAD_OBJ)
endif

C_DEPS           = $(OBJS:%.o=%.d)
C_INCLUDES       = $(APP_INCLUDE_PATH)

COMPILER_CMDLINE = -std=c99 $(COMPILER_FLAGS) $(C_INCLUDES)

# WAD embedding rule (only executed when WAD_ENABLED=1)
$(WAD_OBJ): $(WAD_INPUT) | $(OBJDIR)
	@echo "Embedding DOOM1.WAD -> $@"
	$(Q)${TOOLCHAIN_ROOT}$(CROSS_COMPILE)ld -r -b binary -o $@ $<
	$(Q)$(OC) --rename-section .data=.doomwad $@ $@

# Ensure build directory exists
$(OBJDIR):
	$(Q)mkdir -p $(OBJDIR)

$(OBJDIR)/$(S_STARTUP).o: $(S_SRC) | $(OBJDIR)
	@echo 'ASM: $<'
	$(Q)$(CC) $(ASM_FLAGS) $(C_INCLUDES) -o "$@" "$<"

$(OBJDIR)/%.o: %.c | $(OBJDIR)
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
